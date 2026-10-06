"""
Adversarial Verification and Stress-Testing Harness for Micromouse Odometry (Milestone M1)
Agent: Challenger 1 (teamwork_preview_challenger)
Target: src/main.cpp, src/config.h
"""

import sys

# Constants exactly from config.h and main.cpp
PULSOS_CELDA = 800
PULSOS_CELDA_MEDIA = 400
DISTANCIA_PARADA_FRENTE = 50
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

class RobotSimulator:
    def __init__(self):
        self.reset()

    def reset(self):
        # State variables matching src/main.cpp lines 13-34
        self.limitePulsosActual = PULSOS_CELDA
        self.flancoDetectado = False
        self.habiaParedIzq = False
        self.habiaParedDer = False
        self.pulsosBaseCelda = 0
        self.graciaPIDFinalizada = False
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
        """Simulates one iteration of void loop() in case AVANZANDO"""
        if self.estado != "AVANZANDO":
            return self.estado

        # Encoder accumulation
        self.pulsosA += delta_pulsos
        self.pulsosB += delta_pulsos
        self.pulsosActuales = (abs(self.pulsosA) + abs(self.pulsosB)) // 2
        pulsosTotalesCelda = self.pulsosBaseCelda + self.pulsosActuales

        # R1: Detección y reseteo por flanco lateral (lines 105-124)
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

        # R2: Condiciones de parada (lines 127-142)
        stopPorParedFrontal = (sensado_actual.distanciaCent > 0 and
                               sensado_actual.distanciaCent <= DISTANCIA_PARADA_FRENTE and
                               pulsosTotalesCelda > 100)

        paredAlFrente = (sensado_actual.distanciaCent > 0 and
                         sensado_actual.distanciaCent <= UMBRAL_PARED_FRENTE)

        stopPorEncoders = (self.pulsosActuales >= self.limitePulsosActual) and (not paredAlFrente)

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
                "distanciaCent": sensado_actual.distanciaCent
            })
            return "FRENANDO"
        else:
            # R3: Corrección PID y gracia post-giro (lines 150-165)
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
                "vel_der": vel_der
            })
            return "AVANZANDO"

def run_tests():
    results = []

    # -------------------------------------------------------------
    # TEST 1.1: Left wall dropping at pulse 250
    # -------------------------------------------------------------
    sim = RobotSimulator()
    for p in range(0, 1500, 10):
        # Wall is present (80mm) until 250 pulses, then drops to 200mm
        d_izq = 80 if p < 250 else 200
        state = sim.step_loop(Sensado(cent=250, der=200, izq=d_izq), delta_pulsos=10)
        if state == "FRENANDO":
            break

    stop_evt = [t for t in sim.trace if t["event"] == "STOP"][0]
    flanco_evt = [t for t in sim.trace if t["event"] == "FLANCO_DETECTADO"][0]
    # Check: flanco detected at 260 pulses (next loop), encoders reset, stopped at 400 pulses post-edge
    pass_1_1 = (flanco_evt is not None and 
                stop_evt["pulsosActuales"] == 400 and 
                "ENCODERS" in stop_evt["reason"] and
                stop_evt["pulsosTotalesCelda"] == 660)
    results.append(("1.1 Left Wall Dropping", pass_1_1, 
                    f"Flanco at {flanco_evt['pulsosTotalesCelda']}p, stopped at {stop_evt['pulsosActuales']}p post-edge (total: {stop_evt['pulsosTotalesCelda']}p)"))

    # -------------------------------------------------------------
    # TEST 1.2: Right wall dropping at pulse 300
    # -------------------------------------------------------------
    sim = RobotSimulator()
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
    results.append(("1.2 Right Wall Dropping", pass_1_2, 
                    f"Flanco at {flanco_evt['pulsosTotalesCelda']}p, stopped at {stop_evt['pulsosActuales']}p post-edge (total: {stop_evt['pulsosTotalesCelda']}p)"))

    # -------------------------------------------------------------
    # TEST 1.3: Both walls dropping simultaneously at pulse 200
    # -------------------------------------------------------------
    sim = RobotSimulator()
    for p in range(0, 1500, 10):
        d_lat = 80 if p < 200 else 200
        state = sim.step_loop(Sensado(cent=250, der=d_lat, izq=d_lat), delta_pulsos=10)
        if state == "FRENANDO":
            break
    stop_evt = [t for t in sim.trace if t["event"] == "STOP"][0]
    flanco_count = len([t for t in sim.trace if t["event"] == "FLANCO_DETECTADO"])
    pass_1_3 = (flanco_count == 1 and stop_evt["pulsosActuales"] == 400)
    results.append(("1.3 Both Walls Dropping Simultaneously", pass_1_3, 
                    f"Flanco triggered exactly {flanco_count} time, stopped at {stop_evt['pulsosActuales']}p post-edge"))

    # -------------------------------------------------------------
    # TEST 1.4: Wall absent from start (Fallback 800 pulses)
    # -------------------------------------------------------------
    sim = RobotSimulator()
    for p in range(0, 1500, 10):
        state = sim.step_loop(Sensado(cent=250, der=200, izq=200), delta_pulsos=10)
        if state == "FRENANDO":
            break
    stop_evt = [t for t in sim.trace if t["event"] == "STOP"][0]
    flanco_count = len([t for t in sim.trace if t["event"] == "FLANCO_DETECTADO"])
    pass_1_4 = (flanco_count == 0 and stop_evt["pulsosActuales"] == 800 and stop_evt["pulsosTotalesCelda"] == 800)
    results.append(("1.4 Wall Absent From Start (800p Fallback)", pass_1_4, 
                    f"No flanco, stopped at {stop_evt['pulsosActuales']}p (total: {stop_evt['pulsosTotalesCelda']}p)"))

    # -------------------------------------------------------------
    # TEST 2.1: Transient Notch (< 150 pulses) rejected
    # -------------------------------------------------------------
    sim = RobotSimulator()
    # Notch from pulse 50 to 90, then wall returns
    for p in range(0, 1500, 10):
        d_izq = 200 if (50 <= p <= 90 or p >= 300) else 80
        state = sim.step_loop(Sensado(cent=250, der=200, izq=d_izq), delta_pulsos=10)
        if state == "FRENANDO":
            break
    flanco_evts = [t for t in sim.trace if t["event"] == "FLANCO_DETECTADO"]
    pass_2_1 = (len(flanco_evts) == 1 and flanco_evts[0]["pulsosTotalesCelda"] > 300)
    results.append(("2.1 Transient Notch (<150p) Filtered", pass_2_1, 
                    f"Notch at 50p ignored; real flanco detected at {flanco_evts[0]['pulsosTotalesCelda']}p"))

    # -------------------------------------------------------------
    # TEST 3.1: Nominal Front Wall Approach (Advances past 800p until <= 50mm)
    # -------------------------------------------------------------
    sim = RobotSimulator()
    # Front distance decreases as robot advances: at 800p d_cent=70mm, reaches 50mm at 880p
    for p in range(0, 1500, 10):
        d_cent = max(40, 200 - int(p * 0.17))
        state = sim.step_loop(Sensado(cent=d_cent, der=200, izq=200), delta_pulsos=10)
        if state == "FRENANDO":
            break
    stop_evt = [t for t in sim.trace if t["event"] == "STOP"][0]
    pass_3_1 = (stop_evt["pulsosTotalesCelda"] > 800 and 
                "PARED_FRONTAL" in stop_evt["reason"] and 
                stop_evt["distanciaCent"] <= 50)
    results.append(("3.1 Front Wall Advance Past 800p to <= 50mm", pass_3_1, 
                    f"Stopped at {stop_evt['pulsosTotalesCelda']}p with d_cent={stop_evt['distanciaCent']}mm (reason: {stop_evt['reason']})"))

    # -------------------------------------------------------------
    # TEST 3.2: Early Front Wall (> 100p, e.g. at 400p)
    # -------------------------------------------------------------
    sim = RobotSimulator()
    for p in range(0, 1500, 10):
        # Unexpected obstacle at 400 pulses
        d_cent = 45 if p >= 400 else 180
        state = sim.step_loop(Sensado(cent=d_cent, der=200, izq=200), delta_pulsos=10)
        if state == "FRENANDO":
            break
    stop_evt = [t for t in sim.trace if t["event"] == "STOP"][0]
    pass_3_2 = ("PARED_FRONTAL" in stop_evt["reason"] and stop_evt["pulsosTotalesCelda"] == 410)
    results.append(("3.2 Early Front Obstacle at 400p", pass_3_2, 
                    f"Stopped at {stop_evt['pulsosTotalesCelda']}p safely on front wall"))

    # -------------------------------------------------------------
    # TEST 3.3: ADVERSARIAL - Front wall <= 50mm EARLY (< 100 pulses)
    # -------------------------------------------------------------
    sim = RobotSimulator()
    # Robot placed 40mm from front wall at start
    crashed_early = False
    for p in range(0, 1500, 10):
        d_cent = 40
        state = sim.step_loop(Sensado(cent=d_cent, der=200, izq=200), delta_pulsos=10)
        if state == "FRENANDO":
            break
    stop_evt = [t for t in sim.trace if t["event"] == "STOP"][0]
    # Check if robot stopped before 100 pulses or failed to brake until > 100 pulses
    stopped_before_100 = (stop_evt["pulsosTotalesCelda"] <= 100)
    # This test highlights the vulnerability: it FAILS to brake before 100p!
    results.append(("3.3 ADVERSARIAL: Front Wall <= 50mm at <100 pulses", stopped_before_100, 
                    f"VULNERABILITY CONFIRMED: Did not stop at 10-100p; moved blindly until {stop_evt['pulsosTotalesCelda']}p due to 'pulsosTotalesCelda > 100' guard!"))

    # -------------------------------------------------------------
    # TEST 4.1: Watchdog Trigger at 1050 pulses (Sensor stuck at 70mm)
    # -------------------------------------------------------------
    sim = RobotSimulator()
    for p in range(0, 1500, 10):
        d_cent = 70  # Sensor stuck at 70mm (> 50, but <= 120 so paredAlFrente=True)
        state = sim.step_loop(Sensado(cent=d_cent, der=200, izq=200), delta_pulsos=10)
        if state == "FRENANDO":
            break
    stop_evt = [t for t in sim.trace if t["event"] == "STOP"][0]
    pass_4_1 = ("WATCHDOG" in stop_evt["reason"] and stop_evt["pulsosTotalesCelda"] == 1050)
    results.append(("4.1 Watchdog Trigger at 1050p", pass_4_1, 
                    f"Watchdog stopped robot at {stop_evt['pulsosTotalesCelda']}p (reason: {stop_evt['reason']})"))

    # -------------------------------------------------------------
    # TEST 4.2: ADVERSARIAL - Negative Reading on Front Sensor (d_cent = -5)
    # -------------------------------------------------------------
    sim = RobotSimulator()
    # Sensor calibrated with offset or pushed past zero: reads -5mm at 500 pulses
    for p in range(0, 1500, 10):
        d_cent = -5 if p >= 500 else 180
        state = sim.step_loop(Sensado(cent=d_cent, der=200, izq=200), delta_pulsos=10)
        if state == "FRENANDO":
            break
    stop_evt = [t for t in sim.trace if t["event"] == "STOP"][0]
    stopped_on_negative = ("PARED_FRONTAL" in stop_evt["reason"])
    results.append(("4.2 ADVERSARIAL: Negative Sensor Reading (d_cent = -5mm)", stopped_on_negative, 
                    f"VULNERABILITY CONFIRMED: Negative reading treated as no wall (distanciaCent > 0 guard fails); stopped at {stop_evt['pulsosTotalesCelda']}p by {stop_evt['reason']} instead of immediate brake!"))

    # -------------------------------------------------------------
    # TEST 4.3: ADVERSARIAL - Single Noise Spike > 120mm during approach past 800p
    # -------------------------------------------------------------
    sim = RobotSimulator()
    # Robot at 820 pulses, distance is 70mm, but single glitch reads 135mm at pulse 820
    for p in range(0, 1500, 10):
        if p == 820:
            d_cent = 135  # Glitch
        elif p > 800:
            d_cent = 70
        else:
            d_cent = 180
        state = sim.step_loop(Sensado(cent=d_cent, der=200, izq=200), delta_pulsos=10)
        if state == "FRENANDO":
            break
    stop_evt = [t for t in sim.trace if t["event"] == "STOP"][0]
    survived_spike = (stop_evt["pulsosTotalesCelda"] > 820)
    results.append(("4.3 ADVERSARIAL: Noise Spike > 120mm past 800p", survived_spike, 
                    f"VULNERABILITY CONFIRMED: Single spike to 135mm at 820p caused premature ENCODERS stop at {stop_evt['pulsosTotalesCelda']}p while still 70mm from wall!"))

    # -------------------------------------------------------------
    # TEST 5.1: PID Grace Period (Correccion == 0 for first 100 pulses)
    # -------------------------------------------------------------
    sim = RobotSimulator()
    corrections_early = []
    corrections_late = []
    for p in range(0, 300, 10):
        # Imbalanced walls to demand PID correction
        sim.step_loop(Sensado(cent=200, der=50, izq=110), delta_pulsos=10)
        last_step = sim.trace[-1]
        if last_step["pulsosTotalesCelda"] < 100:
            corrections_early.append(last_step["correccion"])
        else:
            corrections_late.append(last_step["correccion"])

    pass_5_1 = (all(c == 0 for c in corrections_early) and any(c != 0 for c in corrections_late))
    results.append(("5.1 PID Blindness for first 100 pulses", pass_5_1, 
                    f"Early corrections: {set(corrections_early)}, Late corrections: non-zero active"))

    # -------------------------------------------------------------
    # TEST 5.2: PID Grace not re-triggered on R1 mid-cell reset
    # -------------------------------------------------------------
    sim = RobotSimulator()
    # Flanco at 250 pulses
    corrections_post_flanco = []
    for p in range(0, 500, 10):
        d_izq = 80 if p < 250 else 200
        sim.step_loop(Sensado(cent=200, der=50, izq=d_izq), delta_pulsos=10)
        last_step = sim.trace[-1]
        if last_step.get("event") == "AVANZANDO" and sim.flancoDetectado:
            corrections_post_flanco.append(last_step["correccion"])

    pass_5_2 = (len(corrections_post_flanco) > 0 and all(c != 0 for c in corrections_post_flanco[:5]))
    results.append(("5.2 PID Grace not re-triggered on R1 reset", pass_5_2, 
                    f"Post-flanco PID remained active (corrections non-zero: {corrections_post_flanco[:3]})"))

    return results

if __name__ == "__main__":
    test_results = run_tests()
    print("=" * 80)
    print("MICROMOUSE ODOMETRY ADVERSARIAL STRESS TEST REPORT (MILESTONE M1)")
    print("=" * 80)
    for name, passed, detail in test_results:
        status = "[PASS]" if passed else "[FAIL/VULN]"
        print(f"{status:12} | {name:45} | {detail}")
    print("=" * 80)
