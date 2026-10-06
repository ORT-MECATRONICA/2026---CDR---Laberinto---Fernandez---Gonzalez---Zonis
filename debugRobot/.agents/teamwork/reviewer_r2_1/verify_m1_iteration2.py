"""
Adversarial Verification and Stress-Testing Harness for Milestone M1 Iteration 2
Agent: Reviewer 1 (teamwork_preview_reviewer)
Files under review: src/main.cpp, src/config.h
"""

import sys

# Constants from src/config.h
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

class RobotFirmwareM1It2:
    def __init__(self):
        self.reset_all()

    def reset_all(self):
        # Global variables from src/main.cpp
        self.estado = "LISTO"
        self.pulsosActuales = 0
        self.tiempoInicioFreno = 0
        self.estadoPostFreno = "LISTO"
        self.PULSOS_GIRO_90_DER = 300
        self.PULSOS_GIRO_90_IZQ = 280
        self.PULSOS_GIRO_180 = 300

        self.limitePulsosActual = PULSOS_CELDA
        self.flancoDetectado = False
        self.habiaParedIzq = False
        self.habiaParedDer = False
        self.pulsosBaseCelda = 0
        self.graciaPIDFinalizada = False
        self.aproximandoParedFrontal = False

        self.pulsosEncoderA = 0
        self.pulsosEncoderB = 0
        self.errorAnterior = 0
        self.sensadoActual = Sensado()
        self.trace = []

    def iniciarAvanceCelda(self):
        self.resetearEncoders()
        self.resetearErrorAnterior()
        self.limitePulsosActual = PULSOS_CELDA
        self.flancoDetectado = False
        self.habiaParedIzq = False
        self.habiaParedDer = False
        self.pulsosBaseCelda = 0
        self.graciaPIDFinalizada = False
        self.aproximandoParedFrontal = False
        # sensadoActual = actualizarSensado() simulated externally
        self.estado = "AVANZANDO"

    def resetearEncoders(self):
        self.pulsosEncoderA = 0
        self.pulsosEncoderB = 0
        self.pulsosActuales = 0

    def resetearErrorAnterior(self):
        self.errorAnterior = 0

    def calcularCorreccion(self, mediciones):
        hayIzq = mediciones.distanciaIzq < (UMBRAL_PARED_ESTADO_NORMAL + 50)
        hayDer = mediciones.distanciaDer < (UMBRAL_PARED_ESTADO_NORMAL + 50)
        error = 0
        if hayIzq and hayDer:
            error = mediciones.distanciaDer - mediciones.distanciaIzq
        elif hayIzq:
            error = -mediciones.distanciaIzq
        elif hayDer:
            error = mediciones.distanciaDer
        else:
            error = 0

        correccion = (KP * error) + (KD * (error - self.errorAnterior))
        self.errorAnterior = error
        return max(-25, min(25, int(correccion)))

    def step_avanzando(self, sensado_medicion, delta_pulsos=10):
        if self.estado != "AVANZANDO":
            return self.estado

        self.pulsosEncoderA += delta_pulsos
        self.pulsosEncoderB += delta_pulsos
        pulsosA = abs(self.pulsosEncoderA)
        pulsosB = abs(self.pulsosEncoderB)
        self.pulsosActuales = (pulsosA + pulsosB) // 2
        pulsosTotalesCelda = self.pulsosBaseCelda + self.pulsosActuales

        self.sensadoActual = sensado_medicion

        # R1: Detección y reseteo por flanco lateral
        if not self.flancoDetectado:
            if self.sensadoActual.distanciaIzq > 0 and self.sensadoActual.distanciaIzq <= UMBRAL_PARED_ESTADO_NORMAL:
                self.habiaParedIzq = True
            if self.sensadoActual.distanciaDer > 0 and self.sensadoActual.distanciaDer <= UMBRAL_PARED_ESTADO_NORMAL:
                self.habiaParedDer = True

            flancoIzq = self.habiaParedIzq and (self.sensadoActual.distanciaIzq > UMBRAL_PARED_ESTADO_NORMAL)
            flancoDer = self.habiaParedDer and (self.sensadoActual.distanciaDer > UMBRAL_PARED_ESTADO_NORMAL)

            if (flancoIzq or flancoDer) and pulsosTotalesCelda > PULSOS_MIN_DETECCION_FLANCO:
                self.flancoDetectado = True
                self.pulsosBaseCelda += self.pulsosActuales
                self.resetearEncoders()
                self.pulsosActuales = 0
                self.limitePulsosActual = PULSOS_CELDA_MEDIA
                self.trace.append({
                    "event": "FLANCO_R1",
                    "pulsosTotalesCelda": pulsosTotalesCelda,
                    "pulsosBaseCelda": self.pulsosBaseCelda,
                    "limitePulsosActual": self.limitePulsosActual
                })

        # R2: Condiciones de parada
        stopPorParedFrontal = (self.sensadoActual.distanciaCent >= DISTANCIA_MIN_VALIDA and
                               self.sensadoActual.distanciaCent <= DISTANCIA_PARADA_FRENTE)

        paredAlFrente = (self.sensadoActual.distanciaCent >= DISTANCIA_MIN_VALIDA and
                         self.sensadoActual.distanciaCent <= UMBRAL_PARED_FRENTE)

        if paredAlFrente and self.pulsosActuales >= (self.limitePulsosActual - 100):
            self.aproximandoParedFrontal = True

        stopPorEncoders = (self.pulsosActuales >= self.limitePulsosActual) and (not paredAlFrente) and (not self.aproximandoParedFrontal)

        stopPorWatchdog = (pulsosTotalesCelda >= PULSOS_WATCHDOG_SEGURIDAD)

        if stopPorParedFrontal or stopPorEncoders or stopPorWatchdog:
            reasons = []
            if stopPorParedFrontal: reasons.append("STOP_FRONTAL")
            if stopPorEncoders: reasons.append("STOP_ENCODERS")
            if stopPorWatchdog: reasons.append("STOP_WATCHDOG")

            self.estadoPostFreno = "DECISION"
            self.estado = "FRENANDO"
            self.trace.append({
                "event": "STOP",
                "reasons": reasons,
                "pulsosActuales": self.pulsosActuales,
                "pulsosTotalesCelda": pulsosTotalesCelda,
                "distanciaCent": self.sensadoActual.distanciaCent,
                "aproximandoParedFrontal": self.aproximandoParedFrontal
            })
            return "FRENANDO"
        else:
            correccion = self.calcularCorreccion(self.sensadoActual)
            if not self.graciaPIDFinalizada:
                if pulsosTotalesCelda < PULSOS_GRACIA_PID:
                    correccion = 0
                else:
                    self.graciaPIDFinalizada = True

            vel_izq = max(0, min(255, VEL_BASE_DER - correccion))
            vel_der = max(0, min(255, VEL_BASE_IZQ + correccion))
            self.trace.append({
                "event": "STEP_AVANZANDO",
                "pulsosActuales": self.pulsosActuales,
                "pulsosTotalesCelda": pulsosTotalesCelda,
                "correccion": correccion,
                "vel_izq": vel_izq,
                "vel_der": vel_der
            })
            return "AVANZANDO"

def run_suite():
    tests = []

    # -------------------------------------------------------------
    # 1. Verification of 3 Challenger 1 Vulnerabilities
    # -------------------------------------------------------------
    # Vulnerability 1: Front obstacle <= 50mm immediately at < 100 pulses (e.g. 40mm at pulse 10)
    sim = RobotFirmwareM1It2()
    sim.iniciarAvanceCelda()
    # At pulse 10, robot reads 40mm
    state = sim.step_avanzando(Sensado(cent=40, der=150, izq=150), delta_pulsos=10)
    stop_evts = [t for t in sim.trace if t["event"] == "STOP"]
    v1_passed = (len(stop_evts) > 0 and 
                 "STOP_FRONTAL" in stop_evts[0]["reasons"] and 
                 stop_evts[0]["pulsosTotalesCelda"] == 10)
    tests.append(("V1: Immediate front stop <= 50mm without 100p delay", v1_passed,
                  f"Stopped at {stop_evts[0]['pulsosTotalesCelda']}p (reasons: {stop_evts[0]['reasons']})"))

    # Vulnerability 1b: Front obstacle <= 50mm at pulse 0 (boundary test)
    sim = RobotFirmwareM1It2()
    sim.iniciarAvanceCelda()
    state = sim.step_avanzando(Sensado(cent=45, der=150, izq=150), delta_pulsos=0)
    stop_evts = [t for t in sim.trace if t["event"] == "STOP"]
    v1b_passed = (len(stop_evts) > 0 and 
                  "STOP_FRONTAL" in stop_evts[0]["reasons"] and 
                  stop_evts[0]["pulsosTotalesCelda"] == 0)
    tests.append(("V1b: Immediate front stop at pulse 0", v1b_passed,
                  f"Stopped at pulse {stop_evts[0]['pulsosTotalesCelda']} cleanly"))

    # Vulnerability 2: Negative reading down to -20mm (DISTANCIA_MIN_VALIDA)
    sim = RobotFirmwareM1It2()
    sim.iniciarAvanceCelda()
    # Advance to 400 pulses, then front distance reads -5mm (physical contact or calibration offset)
    for p in range(0, 400, 10):
        sim.step_avanzando(Sensado(cent=200, der=150, izq=150), delta_pulsos=10)
    state = sim.step_avanzando(Sensado(cent=-5, der=150, izq=150), delta_pulsos=10)
    stop_evts = [t for t in sim.trace if t["event"] == "STOP"]
    v2_passed = (len(stop_evts) > 0 and 
                 "STOP_FRONTAL" in stop_evts[0]["reasons"] and 
                 stop_evts[0]["pulsosTotalesCelda"] == 410)
    tests.append(("V2: Negative reading (-5mm) recognized as valid front obstacle", v2_passed,
                  f"Stopped safely at {stop_evts[0]['pulsosTotalesCelda']}p on -5mm reading"))

    # Vulnerability 2b: Boundary check on DISTANCIA_MIN_VALIDA = -20mm
    sim = RobotFirmwareM1It2()
    sim.iniciarAvanceCelda()
    state = sim.step_avanzando(Sensado(cent=-20, der=150, izq=150), delta_pulsos=10)
    stop_evts = [t for t in sim.trace if t["event"] == "STOP"]
    v2b_passed = (len(stop_evts) > 0 and "STOP_FRONTAL" in stop_evts[0]["reasons"])
    tests.append(("V2b: Boundary check exactly at -20mm", v2b_passed,
                  f"Recognized -20mm as valid obstacle: {stop_evts[0]['reasons']}"))

    # Vulnerability 2c: Reading below -20mm (e.g. -25mm) not treated as valid obstacle
    sim = RobotFirmwareM1It2()
    sim.iniciarAvanceCelda()
    state = sim.step_avanzando(Sensado(cent=-25, der=150, izq=150), delta_pulsos=10)
    stop_evts = [t for t in sim.trace if t["event"] == "STOP"]
    v2c_passed = (len(stop_evts) == 0 and state == "AVANZANDO")
    tests.append(("V2c: Reading below -20mm (-25mm) rejected as corrupted/invalid", v2c_passed,
                  f"State remains {state} without false emergency brake"))

    # Vulnerability 3: Latching approach past 800 pulses with noise spike > 120mm
    sim = RobotFirmwareM1It2()
    sim.iniciarAvanceCelda()
    # Approaching wall: at pulse 750, distance is 75mm (latches aproximandoParedFrontal)
    # At pulse 800, distance is 68mm
    # At pulse 820, noise spike reads 135mm
    # At pulse 850, distance reaches 48mm -> STOP_FRONTAL
    for p in range(0, 1500, 10):
        if p < 700:
            d_cent = 200
        elif p < 820:
            d_cent = 75 - int((p - 700) * 0.1) # 75 down to ~63mm
        elif p == 820:
            d_cent = 135 # NOISE SPIKE
        elif p < 860:
            d_cent = 55
        else:
            d_cent = 48 # Reached stop threshold <= 50mm

        state = sim.step_avanzando(Sensado(cent=d_cent, der=150, izq=150), delta_pulsos=10)
        if state == "FRENANDO":
            break

    stop_evts = [t for t in sim.trace if t["event"] == "STOP"]
    v3_passed = (len(stop_evts) > 0 and 
                 "STOP_FRONTAL" in stop_evts[0]["reasons"] and 
                 stop_evts[0]["pulsosTotalesCelda"] == 870 and
                 stop_evts[0]["distanciaCent"] == 48)
    tests.append(("V3: Optical noise spike at 820p immune via latching", v3_passed,
                  f"Did NOT stop on spike at 820p; stopped at {stop_evts[0]['pulsosTotalesCelda']}p at 48mm via {stop_evts[0]['reasons']}"))

    # -------------------------------------------------------------
    # 2. Verification of R1 (Falling Edge & Fallback)
    # -------------------------------------------------------------
    # R1.1: Left wall falling at pulse 250 -> 400p reset -> total 660p
    sim = RobotFirmwareM1It2()
    sim.iniciarAvanceCelda()
    for p in range(0, 1500, 10):
        d_izq = 80 if p < 250 else 200
        state = sim.step_avanzando(Sensado(cent=200, der=200, izq=d_izq), delta_pulsos=10)
        if state == "FRENANDO":
            break
    stop_evts = [t for t in sim.trace if t["event"] == "STOP"]
    flanco_evts = [t for t in sim.trace if t["event"] == "FLANCO_R1"]
    r1_1_passed = (len(flanco_evts) == 1 and 
                   flanco_evts[0]["pulsosTotalesCelda"] == 260 and 
                   stop_evts[0]["pulsosActuales"] == 400 and 
                   stop_evts[0]["pulsosTotalesCelda"] == 660 and
                   "STOP_ENCODERS" in stop_evts[0]["reasons"])
    tests.append(("R1.1: Left wall falling edge -> reset to 400p", r1_1_passed,
                  f"Flank at {flanco_evts[0]['pulsosTotalesCelda']}p, stopped at {stop_evts[0]['pulsosActuales']}p post-edge (total: {stop_evts[0]['pulsosTotalesCelda']}p)"))

    # R1.2: Right wall falling at pulse 300 -> 400p reset -> total 710p
    sim = RobotFirmwareM1It2()
    sim.iniciarAvanceCelda()
    for p in range(0, 1500, 10):
        d_der = 80 if p < 300 else 200
        state = sim.step_avanzando(Sensado(cent=200, der=d_der, izq=200), delta_pulsos=10)
        if state == "FRENANDO":
            break
    stop_evts = [t for t in sim.trace if t["event"] == "STOP"]
    flanco_evts = [t for t in sim.trace if t["event"] == "FLANCO_R1"]
    r1_2_passed = (len(flanco_evts) == 1 and 
                   flanco_evts[0]["pulsosTotalesCelda"] == 310 and 
                   stop_evts[0]["pulsosActuales"] == 400 and 
                   stop_evts[0]["pulsosTotalesCelda"] == 710 and
                   "STOP_ENCODERS" in stop_evts[0]["reasons"])
    tests.append(("R1.2: Right wall falling edge -> reset to 400p", r1_2_passed,
                  f"Flank at {flanco_evts[0]['pulsosTotalesCelda']}p, stopped at {stop_evts[0]['pulsosActuales']}p post-edge (total: {stop_evts[0]['pulsosTotalesCelda']}p)"))

    # R1.3: Fallback 800 pulses when no lateral walls present from start
    sim = RobotFirmwareM1It2()
    sim.iniciarAvanceCelda()
    for p in range(0, 1500, 10):
        state = sim.step_avanzando(Sensado(cent=200, der=200, izq=200), delta_pulsos=10)
        if state == "FRENANDO":
            break
    stop_evts = [t for t in sim.trace if t["event"] == "STOP"]
    flanco_evts = [t for t in sim.trace if t["event"] == "FLANCO_R1"]
    r1_3_passed = (len(flanco_evts) == 0 and 
                   stop_evts[0]["pulsosActuales"] == 800 and 
                   stop_evts[0]["pulsosTotalesCelda"] == 800 and
                   "STOP_ENCODERS" in stop_evts[0]["reasons"])
    tests.append(("R1.3: No lateral walls -> Fallback to 800 pulses", r1_3_passed,
                  f"No flank, stopped at exact fallback {stop_evts[0]['pulsosActuales']}p"))

    # R1.4: Notch before 150 pulses ignored, real flank at 300 pulses
    sim = RobotFirmwareM1It2()
    sim.iniciarAvanceCelda()
    for p in range(0, 1500, 10):
        d_izq = 200 if (50 <= p <= 80 or p >= 300) else 80
        state = sim.step_avanzando(Sensado(cent=200, der=200, izq=d_izq), delta_pulsos=10)
        if state == "FRENANDO":
            break
    flanco_evts = [t for t in sim.trace if t["event"] == "FLANCO_R1"]
    r1_4_passed = (len(flanco_evts) == 1 and flanco_evts[0]["pulsosTotalesCelda"] == 310)
    tests.append(("R1.4: Transient notch (<150p) ignored; real flank detected at 300p", r1_4_passed,
                  f"Notch at 50-80p suppressed; real flank at {flanco_evts[0]['pulsosTotalesCelda']}p"))

    # -------------------------------------------------------------
    # 3. Verification of R2 Interaction with R1 (Flank + Front Wall)
    # -------------------------------------------------------------
    # Flank at 250 pulses, front wall detected at pulsosActuales = 320 (total 570p)
    # Target 400 overridden by front wall -> stops at <= 50mm
    sim = RobotFirmwareM1It2()
    sim.iniciarAvanceCelda()
    for p in range(0, 1500, 10):
        d_izq = 80 if p < 250 else 200
        # Front wall: starts far, reaches <= 50mm at total pulses ~620
        d_cent = max(45, 200 - int(p * 0.25))
        state = sim.step_avanzando(Sensado(cent=d_cent, der=200, izq=d_izq), delta_pulsos=10)
        if state == "FRENANDO":
            break
    stop_evts = [t for t in sim.trace if t["event"] == "STOP"]
    flanco_evts = [t for t in sim.trace if t["event"] == "FLANCO_R1"]
    r2_r1_passed = (len(flanco_evts) == 1 and 
                    "STOP_FRONTAL" in stop_evts[0]["reasons"] and 
                    stop_evts[0]["distanciaCent"] <= 50)
    tests.append(("R2+R1: Front wall overrides 400p encoder limit post-flank", r2_r1_passed,
                  f"Flank at {flanco_evts[0]['pulsosTotalesCelda']}p, stopped at {stop_evts[0]['distanciaCent']}mm ({stop_evts[0]['reasons']})"))

    # -------------------------------------------------------------
    # 4. Verification of Watchdog Safety Stop
    # -------------------------------------------------------------
    # Front sensor broken/stuck at 70mm (> 50mm, but <= 120mm) -> Watchdog at 1050 pulses
    sim = RobotFirmwareM1It2()
    sim.iniciarAvanceCelda()
    for p in range(0, 1500, 10):
        state = sim.step_avanzando(Sensado(cent=70, der=200, izq=200), delta_pulsos=10)
        if state == "FRENANDO":
            break
    stop_evts = [t for t in sim.trace if t["event"] == "STOP"]
    wd_passed = (len(stop_evts) > 0 and 
                 "STOP_WATCHDOG" in stop_evts[0]["reasons"] and 
                 stop_evts[0]["pulsosTotalesCelda"] == 1050)
    tests.append(("Watchdog: Safety timeout triggers at exactly 1050 pulses", wd_passed,
                  f"Stopped at {stop_evts[0]['pulsosTotalesCelda']}p (reasons: {stop_evts[0]['reasons']})"))

    # -------------------------------------------------------------
    # 5. Verification of R3 (PID Grace & Decoupling)
    # -------------------------------------------------------------
    # Grace: 0 in pulses 0..99, active at 100
    sim = RobotFirmwareM1It2()
    sim.iniciarAvanceCelda()
    early_corr = []
    late_corr = []
    for p in range(0, 300, 10):
        sim.step_avanzando(Sensado(cent=200, der=50, izq=110), delta_pulsos=10)
        last = sim.trace[-1]
        if last["event"] == "STEP_AVANZANDO":
            if last["pulsosTotalesCelda"] < 100:
                early_corr.append(last["correccion"])
            else:
                late_corr.append(last["correccion"])
    r3_grace_passed = (all(c == 0 for c in early_corr) and len(early_corr) > 0 and any(c != 0 for c in late_corr))
    tests.append(("R3.1: PID forced to 0 for pulses 0..99 and active >= 100", r3_grace_passed,
                  f"Early corrections: {set(early_corr)}, Late non-zero corrections verified"))

    # R3.2: Decoupled from R1 flank reset
    sim = RobotFirmwareM1It2()
    sim.iniciarAvanceCelda()
    post_flank_corr = []
    for p in range(0, 500, 10):
        d_izq = 80 if p < 250 else 200
        sim.step_avanzando(Sensado(cent=200, der=50, izq=d_izq), delta_pulsos=10)
        last = sim.trace[-1]
        if last["event"] == "STEP_AVANZANDO" and sim.flancoDetectado:
            post_flank_corr.append(last["correccion"])
    r3_decouple_passed = (len(post_flank_corr) > 0 and all(c != 0 for c in post_flank_corr[:5]))
    tests.append(("R3.2: PID remains active post-flank without secondary silence", r3_decouple_passed,
                  f"First 5 post-flank corrections: {post_flank_corr[:5]} (all non-zero)"))

    return tests

if __name__ == "__main__":
    results = run_suite()
    all_pass = True
    print(f"{'STATUS':12} | {'TEST NAME':60} | DETAILS")
    print("-" * 110)
    for name, passed, detail in results:
        status = "[PASS]" if passed else "[FAIL]"
        if not passed:
            all_pass = False
        print(f"{status:12} | {name:60} | {detail}")
    print("-" * 110)
    print(f"OVERALL RESULT: {'ALL TESTS PASSED' if all_pass else 'FAILURES DETECTED'}")
    sys.exit(0 if all_pass else 1)
