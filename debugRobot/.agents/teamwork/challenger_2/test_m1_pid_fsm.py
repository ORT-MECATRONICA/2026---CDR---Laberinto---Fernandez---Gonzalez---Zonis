#!/usr/bin/env python3
"""
Test Harness and Logic Simulator for Micromouse Odometry Correction (Milestone M1)
Author: Challenger 2 (teamwork_preview_challenger)

This harness models the exact state machine and PID logic from:
- src/main.cpp
- src/config.h
- src/hardware/movimiento/PID.cpp

Test Objectives:
1. Post-turn entry to AVANZANDO: verify PID correction is exactly 0 for pulses 0 through 100.
2. Verify behavior at pulse 101: does derivative kick occur? Does bumpless transfer work?
3. What happens if R1 resets encoders at pulse 250? Verify PID correction does NOT revert to 0!
4. State machine transitions: verify no deadlocks, infinite loops, or uninitialized variables
   across multiple cycles of AVANZANDO -> FRENANDO -> DECISION -> GIRANDO -> FRENANDO -> AVANZANDO.
"""

import sys

# Configuration constants from src/config.h
KP = 0.5
KI = 0.0
KD = 0.3

VEL_BASE_DER = 45
VEL_BASE_IZQ = 45

PULSOS_CELDA = 800
PULSOS_CELDA_MEDIA = 400
DISTANCIA_PARADA_FRENTE = 50
PULSOS_GRACIA_PID = 100
PULSOS_MIN_DETECCION_FLANCO = 150
PULSOS_WATCHDOG_SEGURIDAD = 1050

UMBRAL_PARED_ESTADO_NORMAL = 130
UMBRAL_PARED_FRENTE = 120

PULSOS_GIRO_90_DER = 300
PULSOS_GIRO_90_IZQ = 280
PULSOS_GIRO_180 = 300


def constrain(val, min_val, max_val):
    if val < min_val:
        return min_val
    elif val > max_val:
        return max_val
    return val


class PIDController:
    def __init__(self, kp=KP, kd=KD):
        self.kp = kp
        self.kd = kd
        self.error_anterior = 0

    def reset_error_anterior(self):
        self.error_anterior = 0

    def calcular_correccion(self, distancia_izq, distancia_cent, distancia_der):
        hay_izq = distancia_izq < (UMBRAL_PARED_ESTADO_NORMAL + 50)
        hay_der = distancia_der < (UMBRAL_PARED_ESTADO_NORMAL + 50)

        if hay_izq and hay_der:
            error = int(distancia_der) - int(distancia_izq)
        elif hay_izq:
            error = -int(distancia_izq)
        elif hay_der:
            error = int(distancia_der)
        else:
            error = 0

        p_term = self.kp * error
        d_term = self.kd * (error - self.error_anterior)
        correccion = int(round(p_term + d_term))

        self.error_anterior = error
        return constrain(correccion, -25, 25), error, p_term, d_term


class MicroMouseFSM:
    def __init__(self):
        self.pid = PIDController()
        self.estado = "LISTO"
        self.estado_post_freno = "LISTO"
        self.tiempo_inicio_freno = 0
        self.current_time_ms = 0

        # Odometry and cell control variables
        self.limite_pulsos_actual = PULSOS_CELDA
        self.flanco_detectado = False
        self.habia_pared_izq = False
        self.habia_pared_der = False
        self.pulsos_base_celda = 0
        self.gracia_pid_finalizada = False

        self.pulsos_a = 0
        self.pulsos_b = 0
        self.pulsos_actuales = 0

        self.vel_izq = 0
        self.vel_der = 0
        self.last_correccion = 0

        # Log trace
        self.trace = []

    def resetear_encoders(self):
        self.pulsos_a = 0
        self.pulsos_b = 0
        self.pulsos_actuales = 0

    def iniciar_avance_celda(self):
        self.resetear_encoders()
        self.pid.reset_error_anterior()
        self.limite_pulsos_actual = PULSOS_CELDA
        self.flanco_detectado = False
        self.habia_pared_izq = False
        self.habia_pared_der = False
        self.pulsos_base_celda = 0
        self.gracia_pid_finalizada = False
        self.estado = "AVANZANDO"
        self.trace.append((self.current_time_ms, self.estado, "iniciarAvanceCelda"))

    def step(self, dist_izq, dist_cent, dist_der, encoder_increment=1, dt_ms=10):
        self.current_time_ms += dt_ms

        if self.estado == "LISTO":
            pass

        elif self.estado == "AVANZANDO":
            self.pulsos_a += encoder_increment
            self.pulsos_b += encoder_increment
            self.pulsos_actuales = (self.pulsos_a + self.pulsos_b) // 2
            pulsos_totales_celda = self.pulsos_base_celda + self.pulsos_actuales

            # R1: Detección y reseteo por flanco lateral
            if not self.flanco_detectado:
                if 0 < dist_izq <= UMBRAL_PARED_ESTADO_NORMAL:
                    self.habia_pared_izq = True
                if 0 < dist_der <= UMBRAL_PARED_ESTADO_NORMAL:
                    self.habia_pared_der = True

                flanco_izq = self.habia_pared_izq and (dist_izq > UMBRAL_PARED_ESTADO_NORMAL)
                flanco_der = self.habia_pared_der and (dist_der > UMBRAL_PARED_ESTADO_NORMAL)

                if (flanco_izq or flanco_der) and pulsos_totales_celda > PULSOS_MIN_DETECCION_FLANCO:
                    self.flanco_detectado = True
                    self.pulsos_base_celda += self.pulsos_actuales
                    self.resetear_encoders()
                    self.pulsos_actuales = 0
                    self.limite_pulsos_actual = PULSOS_CELDA_MEDIA
                    self.trace.append((self.current_time_ms, "R1_FLANCO", f"base={self.pulsos_base_celda}"))

            # Recalculate totals after potential flank reset
            pulsos_totales_celda = self.pulsos_base_celda + self.pulsos_actuales

            # R2: Condiciones de parada
            stop_por_pared_frontal = (0 < dist_cent <= DISTANCIA_PARADA_FRENTE and pulsos_totales_celda > 100)
            pared_al_frente = (0 < dist_cent <= UMBRAL_PARED_FRENTE)
            stop_por_encoders = (self.pulsos_actuales >= self.limite_pulsos_actual) and not pared_al_frente
            stop_por_watchdog = (pulsos_totales_celda >= PULSOS_WATCHDOG_SEGURIDAD)

            if stop_por_pared_frontal or stop_por_encoders or stop_por_watchdog:
                reason = "FRONTAL" if stop_por_pared_frontal else ("ENCODERS" if stop_por_encoders else "WATCHDOG")
                self.tiempo_inicio_freno = self.current_time_ms
                self.estado_post_freno = "DECISION"
                self.estado = "FRENANDO"
                self.trace.append((self.current_time_ms, self.estado, f"stop_{reason}"))
            else:
                # R3: Corrección PID y gracia post-giro (ceguera en los primeros 100 pulsos con bumpless transfer)
                correccion, raw_err, p_term, d_term = self.pid.calcular_correccion(dist_izq, dist_cent, dist_der)
                pid_blind_active = False
                if not self.gracia_pid_finalizada:
                    if pulsos_totales_celda < PULSOS_GRACIA_PID:
                        correccion = 0
                        pid_blind_active = True
                    else:
                        self.gracia_pid_finalizada = True

                self.last_correccion = correccion
                self.vel_izq = constrain(VEL_BASE_DER - correccion, 0, 255)
                self.vel_der = constrain(VEL_BASE_IZQ + correccion, 0, 255)

                return {
                    "pulsos_actuales": self.pulsos_actuales,
                    "pulsos_totales": pulsos_totales_celda,
                    "correccion": correccion,
                    "pid_blind_active": pid_blind_active,
                    "raw_err": raw_err,
                    "p_term": p_term,
                    "d_term": d_term,
                    "vel_izq": self.vel_izq,
                    "vel_der": self.vel_der
                }

        elif self.estado == "FRENANDO":
            if self.current_time_ms - self.tiempo_inicio_freno >= 150:
                if self.estado_post_freno == "AVANZANDO":
                    self.iniciar_avance_celda()
                else:
                    self.pid.reset_error_anterior()
                    self.resetear_encoders()
                    self.estado = self.estado_post_freno
                    self.trace.append((self.current_time_ms, self.estado, "from_FRENANDO"))

        elif self.estado == "DECISION":
            condicion_giro_der = dist_der > UMBRAL_PARED_ESTADO_NORMAL
            condicion_avanzar = dist_der <= UMBRAL_PARED_ESTADO_NORMAL and dist_cent > UMBRAL_PARED_ESTADO_NORMAL
            condicion_giro_izq = dist_der <= UMBRAL_PARED_ESTADO_NORMAL and dist_cent <= UMBRAL_PARED_ESTADO_NORMAL and dist_izq > UMBRAL_PARED_ESTADO_NORMAL
            condicion_giro_180 = dist_der <= UMBRAL_PARED_ESTADO_NORMAL and dist_cent <= UMBRAL_PARED_ESTADO_NORMAL and dist_izq <= UMBRAL_PARED_ESTADO_NORMAL

            if condicion_giro_der:
                self.resetear_encoders()
                self.estado = "GIRANDO_DER"
            elif condicion_avanzar:
                self.iniciar_avance_celda()
            elif condicion_giro_izq:
                self.resetear_encoders()
                self.estado = "GIRANDO_IZQ"
            elif condicion_giro_180:
                self.resetear_encoders()
                self.estado = "GIRANDO_180"
            self.trace.append((self.current_time_ms, self.estado, "from_DECISION"))

        elif self.estado in ("GIRANDO_DER", "GIRANDO_IZQ", "GIRANDO_180"):
            threshold = (PULSOS_GIRO_90_DER if self.estado == "GIRANDO_DER"
                         else (PULSOS_GIRO_90_IZQ if self.estado == "GIRANDO_IZQ" else PULSOS_GIRO_180))
            self.pulsos_a += encoder_increment
            self.pulsos_actuales = self.pulsos_a
            if self.pulsos_actuales >= threshold:
                self.tiempo_inicio_freno = self.current_time_ms
                self.estado_post_freno = "AVANZANDO"
                self.estado = "FRENANDO"
                self.trace.append((self.current_time_ms, self.estado, "turn_complete"))

        return None


def run_all_tests():
    print("======================================================================")
    print("RUNNING MILERSTONE M1 EMPIRICAL CHALLENGER TEST SUITE")
    print("======================================================================")

    # --------------------------------------------------------------------------
    # TEST 1: Post-turn entry to AVANZANDO: verify PID correction is exactly 0
    # for pulses 0 through 100.
    # --------------------------------------------------------------------------
    print("\n--- TEST 1: PID Blindness Grace Period (Pulses 0 through 100) ---")
    fsm = MicroMouseFSM()
    # Simulate post-turn entry via FRENANDO -> AVANZANDO
    fsm.estado = "FRENANDO"
    fsm.estado_post_freno = "AVANZANDO"
    fsm.tiempo_inicio_freno = 0
    fsm.current_time_ms = 150
    fsm.step(dist_izq=40, dist_cent=200, dist_der=50, encoder_increment=0, dt_ms=0)
    assert fsm.estado == "AVANZANDO", f"Expected AVANZANDO, got {fsm.estado}"

    # Asymmetry in walls to induce non-zero raw error: dist_der=60, dist_izq=40 => error=20 != 0
    pulses_0_to_99_all_zero = True
    pulse_records = {}
    for p in range(105):
        # advance 1 pulse per step
        res = fsm.step(dist_izq=40, dist_cent=200, dist_der=60, encoder_increment=1, dt_ms=5)
        if res is not None:
            total_p = res["pulsos_totales"]
            pulse_records[total_p] = res
            if total_p < 100 and res["correccion"] != 0:
                pulses_0_to_99_all_zero = False

    print(f"Pulses 0..99 all zero correction: {pulses_0_to_99_all_zero}")
    print(f"Pulse 0: correccion={pulse_records[1]['correccion']}, blind={pulse_records[1]['pid_blind_active']}")
    print(f"Pulse 50: correccion={pulse_records[50]['correccion']}, blind={pulse_records[50]['pid_blind_active']}")
    print(f"Pulse 99: correccion={pulse_records[99]['correccion']}, blind={pulse_records[99]['pid_blind_active']}")
    print(f"Pulse 100: correccion={pulse_records[100]['correccion']}, blind={pulse_records[100]['pid_blind_active']}")
    print(f"Pulse 101: correccion={pulse_records[101]['correccion']}, blind={pulse_records[101]['pid_blind_active']}")

    assert pulses_0_to_99_all_zero, "PID correction was NOT zero during first 100 pulses!"
    print("TEST 1 RESULT: PASS (Pulses 1..99 strictly 0, exactly 100 pulses of grace elapsed).")

    # --------------------------------------------------------------------------
    # TEST 2: Behavior at pulse 100 and 101: Bumpless transfer and derivative kick
    # --------------------------------------------------------------------------
    print("\n--- TEST 2: Bumpless Transfer vs Derivative Kick Analysis ---")
    # Compare with a naive PID implementation that DOES NOT calculate during grace period
    class NaivePIDController:
        def __init__(self, kp=KP, kd=KD):
            self.kp = kp
            self.kd = kd
            self.error_anterior = 0

        def calcular(self, dist_izq, dist_der):
            error = int(dist_der) - int(dist_izq)
            d_term = self.kd * (error - self.error_anterior)
            p_term = self.kp * error
            self.error_anterior = error
            return p_term + d_term, d_term

    naive = NaivePIDController()
    # At pulse 100, if naive is called for the FIRST time with error=20:
    naive_corr, naive_d_term = naive.calcular(40, 60)
    # With worker's implementation:
    worker_rec_100 = pulse_records[100]
    worker_rec_101 = pulse_records[101]

    print(f"At activation: Raw Error = {worker_rec_100['raw_err']}")
    print(f"Naive PID (stale errorAnterior=0): d_term = {naive_d_term:.2f} (DERIVATIVE KICK!)")
    print(f"Worker PID at pulse 100: d_term = {worker_rec_100['d_term']:.2f} (Continuous bumpless tracking)")
    print(f"Worker PID at pulse 101: d_term = {worker_rec_101['d_term']:.2f}")

    assert abs(worker_rec_100['d_term']) < abs(naive_d_term), "Derivative kick occurred in worker PID!"
    print("TEST 2 RESULT: PASS (Bumpless transfer confirmed, zero derivative kick).")

    # --------------------------------------------------------------------------
    # TEST 3: Encoder reset at pulse 250 by R1
    # --------------------------------------------------------------------------
    print("\n--- TEST 3: R1 Flank Reset at Pulse 250 Interaction with R3 PID ---")
    fsm3 = MicroMouseFSM()
    fsm3.iniciar_avance_celda()

    # Move from 0 to 249 with both walls present
    for p in range(249):
        res = fsm3.step(dist_izq=40, dist_cent=200, dist_der=45, encoder_increment=1, dt_ms=5)

    assert fsm3.pulsos_actuales == 249
    assert fsm3.gracia_pid_finalizada is True

    # At pulse 250, right wall drops to 160 mm (> UMBRAL_PARED_ESTADO_NORMAL 130)
    # This should trigger R1 flank detection
    res_at_250 = fsm3.step(dist_izq=40, dist_cent=200, dist_der=160, encoder_increment=1, dt_ms=5)

    print(f"At pulse 250 trigger:")
    print(f"  flanco_detectado = {fsm3.flanco_detectado}")
    print(f"  pulsos_base_celda = {fsm3.pulsos_base_celda}")
    print(f"  pulsos_actuales = {fsm3.pulsos_actuales}")
    print(f"  limite_pulsos_actual = {fsm3.limite_pulsos_actual}")
    print(f"  gracia_pid_finalizada = {fsm3.gracia_pid_finalizada}")

    assert fsm3.flanco_detectado is True, "Flank was not detected!"
    assert fsm3.pulsos_base_celda == 250, f"Expected pulsos_base_celda=250, got {fsm3.pulsos_base_celda}"
    assert fsm3.limite_pulsos_actual == 400, f"Expected limit=400, got {fsm3.limite_pulsos_actual}"

    # Step at pulse 251 (which is pulsos_actuales = 1, pulsos_totales = 251)
    res_at_251 = fsm3.step(dist_izq=40, dist_cent=200, dist_der=160, encoder_increment=1, dt_ms=5)
    print(f"At pulse 251 (post-reset pulse 1):")
    print(f"  pulsos_totales = {res_at_251['pulsos_totales']}")
    print(f"  pid_blind_active = {res_at_251['pid_blind_active']}")
    print(f"  correccion = {res_at_251['correccion']}")

    assert res_at_251['pid_blind_active'] is False, "CRITICAL BUG: PID reverted to blindness after R1 reset!"
    assert res_at_251['correccion'] != 0 or res_at_251['raw_err'] == 0, "PID correction unexpectedly zeroed!"

    # Now verify stopping distance: should advance 400 pulses from reset point (total 650 pulses)
    stopped = False
    for step_i in range(450):
        res = fsm3.step(dist_izq=40, dist_cent=200, dist_der=160, encoder_increment=1, dt_ms=5)
        if fsm3.estado == "FRENANDO":
            stopped = True
            print(f"Robot stopped at pulsos_actuales={fsm3.pulsos_actuales}, total pulses={fsm3.pulsos_base_celda + fsm3.pulsos_actuales}")
            break

    assert stopped, "Robot did not stop after 400 pulses!"
    assert fsm3.pulsos_actuales == 400, f"Expected stop at 400 pulses post-flank, got {fsm3.pulsos_actuales}"
    assert fsm3.pulsos_base_celda + fsm3.pulsos_actuales == 650, f"Expected total 650 pulses, got {fsm3.pulsos_base_celda + fsm3.pulsos_actuales}"
    print("TEST 3 RESULT: PASS (PID does NOT revert to zero; target resets cleanly to 400 pulses).")

    # --------------------------------------------------------------------------
    # TEST 4: Multi-Cycle State Machine Transitions and Invariant Verification
    # --------------------------------------------------------------------------
    print("\n--- TEST 4: Multi-Cycle FSM Transitions and Invariant Check ---")
    fsm4 = MicroMouseFSM()

    # Define a sequence of maze cells:
    # Cell 1: Normal cell, right wall opens at end -> turn right
    # Cell 2: Straight corridor, no turn -> continue advancing
    # Cell 3: Dead end with front wall at 50mm -> turn left
    # Cell 4: Dead end on all sides -> 180 turn
    # Cell 5: Corridor with sensor failure -> watchdog stop

    cycles = [
        {"name": "Cell 1 (Right turn)", "action": "TURN_RIGHT", "wall_drop": 250},
        {"name": "Cell 2 (Straight)", "action": "STRAIGHT", "wall_drop": None},
        {"name": "Cell 3 (Front wall stop & Left turn)", "action": "TURN_LEFT", "front_stop": True},
        {"name": "Cell 4 (Dead end 180 turn)", "action": "U_TURN", "dead_end": True},
        {"name": "Cell 5 (Watchdog stop)", "action": "WATCHDOG", "watchdog": True}
    ]

    fsm4.iniciar_avance_celda()

    for idx, cycle in enumerate(cycles):
        print(f"\nExecuting Cycle {idx+1}: {cycle['name']}")
        start_state = fsm4.estado
        assert start_state == "AVANZANDO", f"Expected AVANZANDO at cycle start, got {start_state}"

        # Invariant checks at start of cell
        assert fsm4.pulsos_actuales == 0, "pulsos_actuales not zero at cell start"
        assert fsm4.pulsos_base_celda == 0, "pulsos_base_celda not zero at cell start"
        assert fsm4.flanco_detectado is False, "flanco_detectado not reset"
        assert fsm4.gracia_pid_finalizada is False, "gracia_pid_finalizada not reset"
        assert fsm4.limite_pulsos_actual == 800, "limite_pulsos_actual not reset to 800"

        # Advance through cell
        if cycle.get("watchdog"):
            # Blind front sensor reading 200mm, run until watchdog (1050 pulses)
            for _ in range(1200):
                if fsm4.estado != "AVANZANDO":
                    break
                fsm4.step(dist_izq=40, dist_cent=200, dist_der=45, encoder_increment=1, dt_ms=2)
            assert fsm4.estado == "FRENANDO", f"Expected FRENANDO on watchdog, got {fsm4.estado}"
            assert fsm4.pulsos_actuales == 1050, f"Expected watchdog at 1050, got {fsm4.pulsos_actuales}"

        elif cycle.get("front_stop"):
            # Move until front wall reaches <= 50 mm at pulse 500
            for p in range(500):
                fsm4.step(dist_izq=40, dist_cent=200, dist_der=45, encoder_increment=1, dt_ms=2)
            # Front wall detected <= 50mm
            fsm4.step(dist_izq=40, dist_cent=48, dist_der=45, encoder_increment=1, dt_ms=2)
            assert fsm4.estado == "FRENANDO", f"Expected FRENANDO on front stop, got {fsm4.estado}"

        elif cycle.get("wall_drop"):
            # Drop right wall at specified pulse
            drop_p = cycle["wall_drop"]
            for p in range(drop_p):
                fsm4.step(dist_izq=40, dist_cent=200, dist_der=45, encoder_increment=1, dt_ms=2)
            # Right wall opens
            for _ in range(500):
                if fsm4.estado != "AVANZANDO":
                    break
                fsm4.step(dist_izq=40, dist_cent=200, dist_der=180, encoder_increment=1, dt_ms=2)
            assert fsm4.estado == "FRENANDO", f"Expected FRENANDO, got {fsm4.estado}"

        else:
            # Normal advance to 800 pulses
            for _ in range(900):
                if fsm4.estado != "AVANZANDO":
                    break
                fsm4.step(dist_izq=40, dist_cent=200, dist_der=45, encoder_increment=1, dt_ms=2)
            assert fsm4.estado == "FRENANDO", f"Expected FRENANDO, got {fsm4.estado}"

        # Brake for 150ms
        fsm4.step(dist_izq=40, dist_cent=200, dist_der=45, encoder_increment=0, dt_ms=160)
        assert fsm4.estado == "DECISION", f"Expected DECISION after FRENANDO, got {fsm4.estado}"

        # Decision step
        if cycle["action"] == "TURN_RIGHT":
            # Right side open: dist_der > 130
            fsm4.step(dist_izq=40, dist_cent=50, dist_der=180, encoder_increment=0, dt_ms=10)
            assert fsm4.estado == "GIRANDO_DER", f"Expected GIRANDO_DER, got {fsm4.estado}"
            # Execute turn (300 pulses)
            for _ in range(305):
                fsm4.step(dist_izq=40, dist_cent=50, dist_der=180, encoder_increment=1, dt_ms=2)
            assert fsm4.estado == "FRENANDO", f"Expected FRENANDO post-turn, got {fsm4.estado}"
            assert fsm4.estado_post_freno == "AVANZANDO"
            # Brake 150ms post-turn
            fsm4.step(dist_izq=40, dist_cent=200, dist_der=45, encoder_increment=0, dt_ms=160)
            assert fsm4.estado == "AVANZANDO", f"Expected AVANZANDO after post-turn brake, got {fsm4.estado}"

        elif cycle["action"] == "STRAIGHT":
            # Right wall present (<=130), front clear (>130)
            fsm4.step(dist_izq=40, dist_cent=200, dist_der=45, encoder_increment=0, dt_ms=10)
            assert fsm4.estado == "AVANZANDO", f"Expected direct transition to AVANZANDO, got {fsm4.estado}"

        elif cycle["action"] == "TURN_LEFT":
            # Right wall present (<=130), front wall present (<=130), left clear (>130)
            fsm4.step(dist_izq=180, dist_cent=45, dist_der=45, encoder_increment=0, dt_ms=10)
            assert fsm4.estado == "GIRANDO_IZQ", f"Expected GIRANDO_IZQ, got {fsm4.estado}"
            # Execute turn (280 pulses)
            for _ in range(285):
                fsm4.step(dist_izq=180, dist_cent=45, dist_der=45, encoder_increment=1, dt_ms=2)
            assert fsm4.estado == "FRENANDO", f"Expected FRENANDO post-turn, got {fsm4.estado}"
            assert fsm4.estado_post_freno == "AVANZANDO"
            fsm4.step(dist_izq=40, dist_cent=200, dist_der=45, encoder_increment=0, dt_ms=160)
            assert fsm4.estado == "AVANZANDO", f"Expected AVANZANDO, got {fsm4.estado}"

        elif cycle["action"] == "U_TURN":
            # Right <= 130, front <= 130, left <= 130
            fsm4.step(dist_izq=45, dist_cent=45, dist_der=45, encoder_increment=0, dt_ms=10)
            assert fsm4.estado == "GIRANDO_180", f"Expected GIRANDO_180, got {fsm4.estado}"
            # Execute turn (300 pulses)
            for _ in range(305):
                fsm4.step(dist_izq=45, dist_cent=45, dist_der=45, encoder_increment=1, dt_ms=2)
            assert fsm4.estado == "FRENANDO", f"Expected FRENANDO post-turn, got {fsm4.estado}"
            assert fsm4.estado_post_freno == "AVANZANDO"
            fsm4.step(dist_izq=40, dist_cent=200, dist_der=45, encoder_increment=0, dt_ms=160)
            assert fsm4.estado == "AVANZANDO", f"Expected AVANZANDO, got {fsm4.estado}"

        elif cycle["action"] == "WATCHDOG":
            # Just verify brake to decision
            fsm4.step(dist_izq=40, dist_cent=200, dist_der=45, encoder_increment=0, dt_ms=160)
            assert fsm4.estado == "DECISION", f"Expected DECISION after watchdog brake, got {fsm4.estado}"

    print("TEST 4 RESULT: PASS (All state transitions deterministically clean; no deadlocks or leaks across 5 full navigation cycles).")

    print("\n======================================================================")
    print("ALL 4 CHALLENGER TESTS PASSED RIGOROUSLY!")
    print("======================================================================")

if __name__ == "__main__":
    run_all_tests()
