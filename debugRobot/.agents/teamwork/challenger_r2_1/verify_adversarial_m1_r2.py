"""
Adversarial Verification and Stress-Testing Harness for Micromouse Odometry (Milestone M1 Iteration 2)
Agent: Challenger 1 (teamwork_preview_challenger)
Target: src/main.cpp, src/config.h

This harness empirically models the exact ESP32 MicroMouse FSM, sensor math, and odometry mechanics
from Milestone M1 Iteration 2. It rigorously tests:
1. The 3 previously failing scenarios (3.3, 4.2, 4.3) to verify that the fixes in Worker 2 pass.
2. All regression scenarios (1.1-1.4, 2.1, 3.1-3.2, 4.1, 5.1-5.2).
3. Advanced coupled interactions and boundary edge cases (4.4, 6.1).
"""

import sys

# Constants exactly matching src/config.h and src/main.cpp
PULSOS_CELDA = 800
PULSOS_CELDA_MEDIA = 400
DISTANCIA_PARADA_FRENTE = 50
DISTANCIA_MIN_VALIDA = -20
PULSOS_GRACIA_PID = 100
PULSOS_MIN_DETECCION_FLANCO = 150
PULSOS_WATCHDOG_SEGURIDAD = 1050
UMBRAL_PARED_ESTADO_NORMAL = 130
UMBRAL_PARED_FRENTE = 120

VEL_BASE_DER = 45
VEL_BASE_IZQ = 45
KP = 0.5
KD = 0.3

class Sensado:
    def __init__(self, cent=200, der=80, izq=80):
        self.distanciaCent = cent
        self.distanciaDer = der
        self.distanciaIzq = izq

class RobotSimulatorM1R2:
    def __init__(self):
        self.reset()

    def reset(self):
        # State variables matching src/main.cpp lines 20-53
        self.limitePulsosActual = PULSOS_CELDA
        self.flancoDetectado = False
        self.habiaParedIzq = False
        self.habiaParedDer = False
        self.pulsosBaseCelda = 0
        self.graciaPIDFinalizada = False
        self.aproximandoParedFrontal = False
        self.pulsosActuales = 0
        self.pulsosA = 0
        self.pulsosB = 0
        self.estado = "AVANZANDO"
        self.tiempoInicioFreno = 0
        self.estadoPostFreno = "LISTO"
        self.errorAnterior = 0
        self.trace = []

    def calcular_correccion(self, sensado_actual):
        # Implementation matching src/hardware/movimiento/PID.cpp
        hayIzq = sensado_actual.distanciaIzq < (UMBRAL_PARED_ESTADO_NORMAL + 50)
        hayDer = sensado_actual.distanciaDer < (UMBRAL_PARED_ESTADO_NORMAL + 50)
        error = 0
        if hayIzq and hayDer:
            error = sensado_actual.distanciaDer - sensado_actual.distanciaIzq
        elif hayIzq:
            error = -sensado_actual.distanciaIzq
        elif hayDer:
            error = sensado_actual.distanciaDer
        else:
            error = 0

        correccion = (KP * error) + (KD * (error - self.errorAnterior))
        self.errorAnterior = error
        return max(-25, min(25, int(correccion)))

    def step_loop(self, sensado_actual, delta_pulsos=10):
        """Simulates one iteration of void loop() in case AVANZANDO (src/main.cpp lines 98-174)"""
        if self.estado != "AVANZANDO":
            return self.estado

        # Encoder accumulation
        self.pulsosA += delta_pulsos
        self.pulsosB += delta_pulsos
        self.pulsosActuales = (abs(self.pulsosA) + abs(self.pulsosB)) // 2
        pulsosTotalesCelda = self.pulsosBaseCelda + self.pulsosActuales

        # R1: Detección y reseteo por flanco lateral (lines 107-126)
        if not self.flancoDetectado:
            if sensado_actual.distanciaIzq > 0 and sensado_actual.distanciaIzq <= UMBRAL_PARED_ESTADO_NORMAL:
                self.habiaParedIzq = True
            if sensado_actual.distanciaDer > 0 and sensado_actual.distanciaDer <= UMBRAL_PARED_ESTADO_NORMAL:
                self.habiaParedDer = True

            flancoIzq = self.habiaParedIzq and (sensado_actual.distanciaIzq > UMBRAL_PARED_ESTADO_NORMAL)
            flancoDer = self.habiaParedDer and (sensado_actual.distanciaDer > UMBRAL_PARED_ESTADO_NORMAL)

            if (flancoIzq or flancoDer) and pulsosTotalesCelda > PULSOS_MIN_DETECCION_FLANCO:
                self.flancoDetectado = True
                self.pulsosBaseCelda += self.pulsosActuales
                # resetearEncoders()
                self.pulsosA = 0
                self.pulsosB = 0
                self.pulsosActuales = 0
                self.limitePulsosActual = PULSOS_CELDA_MEDIA
                self.trace.append({
                    "event": "FLANCO_DETECTADO",
                    "pulsosTotalesCelda": pulsosTotalesCelda,
                    "pulsosBaseCelda": self.pulsosBaseCelda,
                    "limitePulsosActual": self.limitePulsosActual
                })

        # R2: Condiciones de parada (lines 128-148)
        # 1. Parada por pared frontal a <= 50 mm (con guarda de validez física >= DISTANCIA_MIN_VALIDA)
        stopPorParedFrontal = (sensado_actual.distanciaCent >= DISTANCIA_MIN_VALIDA and
                               sensado_actual.distanciaCent <= DISTANCIA_PARADA_FRENTE)

        # 2. Detección de pared frontal en aproximación
        paredAlFrente = (sensado_actual.distanciaCent >= DISTANCIA_MIN_VALIDA and
                         sensado_actual.distanciaCent <= UMBRAL_PARED_FRENTE)

        # Enclavar aproximación a pared frontal cerca o más allá del límite de celda
        if paredAlFrente and self.pulsosActuales >= (self.limitePulsosActual - 100):
            self.aproximandoParedFrontal = True

        # 3. Parada por encoders (400 pulsos si hubo flanco, o 800 fallback si no hubo flanco).
        # R2 sobreescribe la parada por encoders si hay pared al frente o si se enclavó aproximación.
        stopPorEncoders = (self.pulsosActuales >= self.limitePulsosActual) and (not paredAlFrente) and (not self.aproximandoParedFrontal)

        # 4. Watchdog de seguridad (anti-colisión ante fallo de sensor frontal)
        stopPorWatchdog = (pulsosTotalesCelda >= PULSOS_WATCHDOG_SEGURIDAD)

        if stopPorParedFrontal or stopPorEncoders or stopPorWatchdog:
            stop_reason = []
            if stopPorParedFrontal: stop_reason.append("PARED_FRONTAL")
            if stopPorEncoders: stop_reason.append("ENCODERS")
            if stopPorWatchdog: stop_reason.append("WATCHDOG")

            self.estadoPostFreno = "DECISION"
            self.estado = "FRENANDO"
            self.trace.append({
                "event": "STOP",
                "reason": stop_reason,
                "pulsosActuales": self.pulsosActuales,
                "pulsosTotalesCelda": pulsosTotalesCelda,
                "distanciaCent": sensado_actual.distanciaCent,
                "aproximandoParedFrontal": self.aproximandoParedFrontal
            })
            return "FRENANDO"
        else:
            # R3: Corrección PID y gracia post-giro (lines 156-172)
            correccion = self.calcular_correccion(sensado_actual)
            if not self.graciaPIDFinalizada:
                if pulsosTotalesCelda < PULSOS_GRACIA_PID:
                    correccion = 0
                else:
                    self.graciaPIDFinalizada = True

            vel_izq = max(0, min(255, VEL_BASE_DER - correccion))
            vel_der = max(0, min(255, VEL_BASE_IZQ + correccion))
            self.trace.append({
                "event": "AVANZANDO",
                "pulsosActuales": self.pulsosActuales,
                "pulsosTotalesCelda": pulsosTotalesCelda,
                "correccion": correccion,
                "vel_izq": vel_izq,
                "vel_der": vel_der,
                "aproximandoParedFrontal": self.aproximandoParedFrontal
            })
            return "AVANZANDO"

def run_all_tests():
    test_suite = []

    # =========================================================================
    # 1. PREVIOUSLY FAILING TESTS (MUST PASS IN ITERATION 2)
    # =========================================================================

    # TEST 3.3: Obstacle frontal < 100p (e.g. 40mm at pulse 10)
    sim = RobotSimulatorM1R2()
    for p in range(0, 1500, 10):
        d_cent = 40  # Obstacle present right from cell entry
        state = sim.step_loop(Sensado(cent=d_cent, der=200, izq=200), delta_pulsos=10)
        if state == "FRENANDO":
            break
    stop_evt = [t for t in sim.trace if t["event"] == "STOP"][0]
    pass_3_3 = ("PARED_FRONTAL" in stop_evt["reason"] and 
                stop_evt["pulsosTotalesCelda"] == 10 and 
                stop_evt["distanciaCent"] == 40)
    test_suite.append((
        "3.3 Obstacle frontal < 100p (40mm at pulse 10)",
        pass_3_3,
        f"Robot stopped immediately at {stop_evt['pulsosTotalesCelda']}p (d_cent={stop_evt['distanciaCent']}mm, reason={stop_evt['reason']})"
    ))

    # TEST 4.2: Negative sensor readings (-5mm to -20mm)
    # Testing both -5mm and boundary -20mm
    sim = RobotSimulatorM1R2()
    for p in range(0, 1500, 10):
        d_cent = -5 if p >= 500 else 180
        state = sim.step_loop(Sensado(cent=d_cent, der=200, izq=200), delta_pulsos=10)
        if state == "FRENANDO":
            break
    stop_evt = [t for t in sim.trace if t["event"] == "STOP"][0]
    pass_4_2_a = ("PARED_FRONTAL" in stop_evt["reason"] and stop_evt["pulsosTotalesCelda"] == 510)

    sim_b = RobotSimulatorM1R2()
    for p in range(0, 1500, 10):
        d_cent = -20 if p >= 350 else 180
        state = sim_b.step_loop(Sensado(cent=d_cent, der=200, izq=200), delta_pulsos=10)
        if state == "FRENANDO":
            break
    stop_evt_b = [t for t in sim_b.trace if t["event"] == "STOP"][0]
    pass_4_2_b = ("PARED_FRONTAL" in stop_evt_b["reason"] and stop_evt_b["pulsosTotalesCelda"] == 360)

    pass_4_2 = pass_4_2_a and pass_4_2_b
    test_suite.append((
        "4.2 Negative sensor readings (-5mm to -20mm)",
        pass_4_2,
        f"-5mm stopped immediately at {stop_evt['pulsosTotalesCelda']}p; -20mm boundary stopped at {stop_evt_b['pulsosTotalesCelda']}p (reason={stop_evt['reason']})"
    ))

    # TEST 4.3: Optical noise spike (>120mm at pulse 820)
    sim = RobotSimulatorM1R2()
    for p in range(0, 1500, 10):
        if p == 820:
            d_cent = 135  # Glitch above UMBRAL_PARED_FRENTE (120mm)
        elif p >= 700:
            # Gradually approaches wall: 75mm at 800p, reaches 45mm at 880p
            d_cent = max(45, 110 - int((p - 700) * 0.36))
        else:
            d_cent = 180
        state = sim.step_loop(Sensado(cent=d_cent, der=200, izq=200), delta_pulsos=10)
        if state == "FRENANDO":
            break
    stop_evt = [t for t in sim.trace if t["event"] == "STOP"][0]
    pass_4_3 = (stop_evt["pulsosTotalesCelda"] > 820 and 
                "PARED_FRONTAL" in stop_evt["reason"] and 
                stop_evt["distanciaCent"] <= 50 and 
                stop_evt["aproximandoParedFrontal"] is True)
    test_suite.append((
        "4.3 Optical noise spike (>120mm at pulse 820)",
        pass_4_3,
        f"Approach latch survived spike at 820p; stopped cleanly on front wall at {stop_evt['pulsosTotalesCelda']}p with d_cent={stop_evt['distanciaCent']}mm"
    ))

    # =========================================================================
    # 2. REGRESSION VERIFICATION (R1, R2, R3, R4)
    # =========================================================================

    # TEST 1.1: Left wall dropping at pulse 250
    sim = RobotSimulatorM1R2()
    for p in range(0, 1500, 10):
        d_izq = 80 if p < 250 else 200
        state = sim.step_loop(Sensado(cent=250, der=200, izq=d_izq), delta_pulsos=10)
        if state == "FRENANDO":
            break
    stop_evt = [t for t in sim.trace if t["event"] == "STOP"][0]
    flanco_evt = [t for t in sim.trace if t["event"] == "FLANCO_DETECTADO"][0]
    pass_1_1 = (flanco_evt is not None and 
                stop_evt["pulsosActuales"] == 400 and 
                "ENCODERS" in stop_evt["reason"] and 
                stop_evt["pulsosTotalesCelda"] == 660)
    test_suite.append((
        "1.1 Left wall falling edge at 250p",
        pass_1_1,
        f"Flanco detected at {flanco_evt['pulsosTotalesCelda']}p; stopped at {stop_evt['pulsosActuales']}p post-edge (total: {stop_evt['pulsosTotalesCelda']}p)"
    ))

    # TEST 1.2: Right wall dropping at pulse 300
    sim = RobotSimulatorM1R2()
    for p in range(0, 1500, 10):
        d_der = 80 if p < 300 else 200
        state = sim.step_loop(Sensado(cent=250, der=d_der, izq=200), delta_pulsos=10)
        if state == "FRENANDO":
            break
    stop_evt = [t for t in sim.trace if t["event"] == "STOP"][0]
    flanco_evt = [t for t in sim.trace if t["event"] == "FLANCO_DETECTADO"][0]
    pass_1_2 = (flanco_evt is not None and 
                stop_evt["pulsosActuales"] == 400 and 
                "ENCODERS" in stop_evt["reason"] and 
                stop_evt["pulsosTotalesCelda"] == 710)
    test_suite.append((
        "1.2 Right wall falling edge at 300p",
        pass_1_2,
        f"Flanco detected at {flanco_evt['pulsosTotalesCelda']}p; stopped at {stop_evt['pulsosActuales']}p post-edge (total: {stop_evt['pulsosTotalesCelda']}p)"
    ))

    # TEST 1.3: Both walls dropping simultaneously at pulse 200
    sim = RobotSimulatorM1R2()
    for p in range(0, 1500, 10):
        d_lat = 80 if p < 200 else 200
        state = sim.step_loop(Sensado(cent=250, der=d_lat, izq=d_lat), delta_pulsos=10)
        if state == "FRENANDO":
            break
    stop_evt = [t for t in sim.trace if t["event"] == "STOP"][0]
    flanco_count = len([t for t in sim.trace if t["event"] == "FLANCO_DETECTADO"])
    pass_1_3 = (flanco_count == 1 and stop_evt["pulsosActuales"] == 400 and stop_evt["pulsosTotalesCelda"] == 610)
    test_suite.append((
        "1.3 Both walls dropping simultaneously at 200p",
        pass_1_3,
        f"One-shot latch fired exactly {flanco_count} time; stopped at 400p post-edge (total: {stop_evt['pulsosTotalesCelda']}p)"
    ))

    # TEST 1.4: Wall absent from start (Fallback 800 pulses)
    sim = RobotSimulatorM1R2()
    for p in range(0, 1500, 10):
        state = sim.step_loop(Sensado(cent=250, der=200, izq=200), delta_pulsos=10)
        if state == "FRENANDO":
            break
    stop_evt = [t for t in sim.trace if t["event"] == "STOP"][0]
    flanco_count = len([t for t in sim.trace if t["event"] == "FLANCO_DETECTADO"])
    pass_1_4 = (flanco_count == 0 and stop_evt["pulsosActuales"] == 800 and stop_evt["pulsosTotalesCelda"] == 800)
    test_suite.append((
        "1.4 Wall absent from start (Fallback 800p)",
        pass_1_4,
        f"No flanco detected; stopped at exactly {stop_evt['pulsosActuales']}p fallback"
    ))

    # TEST 2.1: Transient lateral notch (< 150p) ignored
    sim = RobotSimulatorM1R2()
    for p in range(0, 1500, 10):
        # Notch from 50 to 90 pulses
        d_izq = 200 if (50 <= p <= 90 or p >= 300) else 80
        state = sim.step_loop(Sensado(cent=250, der=200, izq=d_izq), delta_pulsos=10)
        if state == "FRENANDO":
            break
    flanco_evts = [t for t in sim.trace if t["event"] == "FLANCO_DETECTADO"]
    stop_evt = [t for t in sim.trace if t["event"] == "STOP"][0]
    pass_2_1 = (len(flanco_evts) == 1 and flanco_evts[0]["pulsosTotalesCelda"] > 300 and stop_evt["pulsosActuales"] == 400)
    test_suite.append((
        "2.1 Transient notch (<150p) filtered out",
        pass_2_1,
        f"Notch at 50-90p ignored; true edge latched at {flanco_evts[0]['pulsosTotalesCelda']}p; stopped at 400p post-edge"
    ))

    # TEST 3.1: Nominal front wall approach past 800p until <= 50mm
    sim = RobotSimulatorM1R2()
    for p in range(0, 1500, 10):
        d_cent = max(40, 200 - int(p * 0.17))
        state = sim.step_loop(Sensado(cent=d_cent, der=200, izq=200), delta_pulsos=10)
        if state == "FRENANDO":
            break
    stop_evt = [t for t in sim.trace if t["event"] == "STOP"][0]
    pass_3_1 = (stop_evt["pulsosTotalesCelda"] > 800 and 
                "PARED_FRONTAL" in stop_evt["reason"] and 
                stop_evt["distanciaCent"] <= 50)
    test_suite.append((
        "3.1 Nominal front wall approach (>800p to <=50mm)",
        pass_3_1,
        f"Overrode encoders; stopped on front wall at {stop_evt['pulsosTotalesCelda']}p (d_cent={stop_evt['distanciaCent']}mm)"
    ))

    # TEST 3.2: Early front obstacle (> 100p, e.g. at 400p)
    sim = RobotSimulatorM1R2()
    for p in range(0, 1500, 10):
        d_cent = 45 if p >= 400 else 180
        state = sim.step_loop(Sensado(cent=d_cent, der=200, izq=200), delta_pulsos=10)
        if state == "FRENANDO":
            break
    stop_evt = [t for t in sim.trace if t["event"] == "STOP"][0]
    pass_3_2 = ("PARED_FRONTAL" in stop_evt["reason"] and stop_evt["pulsosTotalesCelda"] == 410)
    test_suite.append((
        "3.2 Early front obstacle at 400p",
        pass_3_2,
        f"Stopped safely on front obstacle at {stop_evt['pulsosTotalesCelda']}p (reason={stop_evt['reason']})"
    ))

    # TEST 4.1: Watchdog trigger at 1050p (Sensor stuck at 70mm)
    sim = RobotSimulatorM1R2()
    for p in range(0, 1500, 10):
        d_cent = 70  # Wall detected in approach (>50mm, <=120mm), but never drops below 50mm
        state = sim.step_loop(Sensado(cent=d_cent, der=200, izq=200), delta_pulsos=10)
        if state == "FRENANDO":
            break
    stop_evt = [t for t in sim.trace if t["event"] == "STOP"][0]
    pass_4_1 = ("WATCHDOG" in stop_evt["reason"] and stop_evt["pulsosTotalesCelda"] == 1050)
    test_suite.append((
        "4.1 Watchdog trigger at 1050p",
        pass_4_1,
        f"Anti-collision watchdog engaged at {stop_evt['pulsosTotalesCelda']}p (reason={stop_evt['reason']})"
    ))

    # TEST 5.1: PID grace period (Correccion == 0 for first 100 pulses)
    sim = RobotSimulatorM1R2()
    corrections_early = []
    corrections_late = []
    for p in range(0, 300, 10):
        sim.step_loop(Sensado(cent=200, der=50, izq=110), delta_pulsos=10)
        last_step = sim.trace[-1]
        if last_step["pulsosTotalesCelda"] < 100:
            corrections_early.append(last_step["correccion"])
        else:
            corrections_late.append(last_step["correccion"])
    pass_5_1 = (all(c == 0 for c in corrections_early) and any(c != 0 for c in corrections_late))
    test_suite.append((
        "5.1 PID blindness for first 100 pulses",
        pass_5_1,
        f"Early corrections zeroed: {set(corrections_early) == {0}}; active late corrections engaged"
    ))

    # TEST 5.2: PID grace not re-triggered on R1 mid-cell reset
    sim = RobotSimulatorM1R2()
    corrections_post_flanco = []
    for p in range(0, 500, 10):
        d_izq = 80 if p < 250 else 200
        sim.step_loop(Sensado(cent=200, der=50, izq=d_izq), delta_pulsos=10)
        last_step = sim.trace[-1]
        if last_step.get("event") == "AVANZANDO" and sim.flancoDetectado:
            corrections_post_flanco.append(last_step["correccion"])
    pass_5_2 = (len(corrections_post_flanco) > 0 and all(c != 0 for c in corrections_post_flanco[:5]))
    test_suite.append((
        "5.2 PID grace not re-triggered on R1 reset",
        pass_5_2,
        f"PID remained continuous post-flanco (active corrections: {corrections_post_flanco[:3]})"
    ))

    # =========================================================================
    # 3. ADVANCED ADVERSARIAL & COUPLED SCENARIOS
    # =========================================================================

    # TEST 4.4: Corrupted negative reading below physical floor (e.g. -50mm from disconnected sensor)
    # When sensor returns raw=0 (0 - 50 = -50mm), it should NOT be accepted as a front wall stop,
    # but fall back safely to encoders.
    sim = RobotSimulatorM1R2()
    for p in range(0, 1500, 10):
        d_cent = -50  # Sensor error (below DISTANCIA_MIN_VALIDA = -20)
        state = sim.step_loop(Sensado(cent=d_cent, der=200, izq=200), delta_pulsos=10)
        if state == "FRENANDO":
            break
    stop_evt = [t for t in sim.trace if t["event"] == "STOP"][0]
    pass_4_4 = ("ENCODERS" in stop_evt["reason"] and stop_evt["pulsosTotalesCelda"] == 800)
    test_suite.append((
        "4.4 Corrupted negative reading < -20mm (d_cent = -50mm)",
        pass_4_4,
        f"Below-floor noise rejected; safely stopped on fallback encoders at {stop_evt['pulsosTotalesCelda']}p"
    ))

    # TEST 6.1: Coupled R1 Edge + R2 Front Wall Approach with Noise Spike at 320p post-edge
    # R1 resets at 250p -> limitePulsosActual becomes 400.
    # Approach latch should engage at limitePulsosActual - 100 = 300 pulses.
    # Noise spike at 320p should NOT trigger premature encoder brake.
    sim = RobotSimulatorM1R2()
    for p in range(0, 1500, 10):
        d_izq = 80 if p < 250 else 200
        # Front wall present: at pulse 550 (290 post-edge), d_cent is 80mm; at 580 (320 post-edge) spike to 140mm; reaches 45mm at 620
        if p == 580:
            d_cent = 140  # Glitch
        elif p >= 550:
            d_cent = max(45, 100 - int((p - 550) * 0.8))
        else:
            d_cent = 250
        state = sim.step_loop(Sensado(cent=d_cent, der=200, izq=d_izq), delta_pulsos=10)
        if state == "FRENANDO":
            break
    stop_evt = [t for t in sim.trace if t["event"] == "STOP"][0]
    pass_6_1 = (sim.flancoDetectado is True and 
                "PARED_FRONTAL" in stop_evt["reason"] and 
                stop_evt["distanciaCent"] <= 50 and 
                stop_evt["pulsosActuales"] < 400)
    test_suite.append((
        "6.1 Coupled R1 edge + R2 front wall with noise spike",
        pass_6_1,
        f"R1 edge + approach latch worked synchronously; stopped at {stop_evt['pulsosTotalesCelda']}p (reason={stop_evt['reason']})"
    ))

    return test_suite

if __name__ == "__main__":
    results = run_all_tests()
    all_passed = True
    print("=" * 90)
    print("MICROMOUSE ODOMETRY ADVERSARIAL STRESS TEST REPORT — ITERATION 2")
    print("=" * 90)
    for name, passed, detail in results:
        status = "[PASS]" if passed else "[FAIL]"
        if not passed:
            all_passed = False
        print(f"{status:8} | {name:48} | {detail}")
    print("=" * 90)
    print(f"OVERALL VERDICT: {'APPROVE (ALL PASS)' if all_passed else 'REQUEST_CHANGES'}")
    print("=" * 90)
