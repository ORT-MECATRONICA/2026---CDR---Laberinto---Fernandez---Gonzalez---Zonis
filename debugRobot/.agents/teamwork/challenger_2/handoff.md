# Challenger 2 Report — Milestone M1: Empirical & Adversarial Stress Testing

- **Agent:** Challenger 2 (`teamwork_preview_challenger`)
- **Working Directory:** `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\.agents\teamwork\challenger_2`
- **Scope:** Empirical and adversarial stress testing of R3 (PID blindness & bumpless transfer), R4 (FSM stability & absence of mapping), and PID control stability across cycles.
- **Verdict:** **APPROVE** (Milestone M1 logic is robust and mathematically sound; two pre-existing latent bugs noted for future milestones).

---

## 1. Observation

### 1.1 Source Code Under Review
Inspected the authoritative source files:
- `src/config.h` (lines 66–73):
  ```cpp
  #define PULSOS_CELDA 800
  #define PULSOS_CELDA_MEDIA 400
  #define DISTANCIA_PARADA_FRENTE 50
  #define PULSOS_GRACIA_PID 100
  #define PULSOS_MIN_DETECCION_FLANCO 150
  #define PULSOS_WATCHDOG_SEGURIDAD 1050
  #define PULSOS_PREGIRO_90_DER 300
  #define PULSOS_PREGIRO_90_IZQ 280
  ```
- `src/main.cpp` (lines 27–51):
  ```cpp
  uint16_t limitePulsosActual = PULSOS_CELDA;
  bool flancoDetectado = false;
  bool habiaParedIzq = false;
  bool habiaParedDer = false;
  uint32_t pulsosBaseCelda = 0;
  bool graciaPIDFinalizada = false;

  void iniciarAvanceCelda() {
    resetearEncoders();
    resetearErrorAnterior();
    limitePulsosActual = PULSOS_CELDA;
    flancoDetectado = false;
    habiaParedIzq = false;
    habiaParedDer = false;
    pulsosBaseCelda = 0;
    graciaPIDFinalizada = false;
    sensadoActual = actualizarSensado();
    enviarString(">>> AVANZANDO <<<");
    estado = AVANZANDO;
  }
  ```
- `src/main.cpp` (lines 96–124) [Odometry & R1 Flank Reset]:
  ```cpp
  int32_t pulsosA = abs(verPulsosEncoderA());
  int32_t pulsosB = abs(verPulsosEncoderB());
  pulsosActuales = (pulsosA + pulsosB) / 2;
  uint32_t pulsosTotalesCelda = pulsosBaseCelda + pulsosActuales;

  sensadoActual = actualizarSensado();

  if (!flancoDetectado) {
    if (sensadoActual.distanciaIzq > 0 && sensadoActual.distanciaIzq <= UMBRAL_PARED_ESTADO_NORMAL) {
      habiaParedIzq = true;
    }
    if (sensadoActual.distanciaDer > 0 && sensadoActual.distanciaDer <= UMBRAL_PARED_ESTADO_NORMAL) {
      habiaParedDer = true;
    }

    bool flancoIzq = habiaParedIzq && (sensadoActual.distanciaIzq > UMBRAL_PARED_ESTADO_NORMAL);
    bool flancoDer = habiaParedDer && (sensadoActual.distanciaDer > UMBRAL_PARED_ESTADO_NORMAL);

    if ((flancoIzq || flancoDer) && pulsosTotalesCelda > PULSOS_MIN_DETECCION_FLANCO) {
      flancoDetectado = true;
      pulsosBaseCelda += pulsosActuales;
      resetearEncoders();
      pulsosActuales = 0;
      limitePulsosActual = PULSOS_CELDA_MEDIA;
      enviarString(">>> FLANCO DETECTADO: RESET A 400 PULSOS <<<");
    }
  }
  ```
- `src/main.cpp` (lines 150–166) [R3 PID Grace & Bumpless Transfer]:
  ```cpp
  int16_t correccion = calcularCorreccion(sensadoActual);
  if (!graciaPIDFinalizada) {
    if (pulsosTotalesCelda < PULSOS_GRACIA_PID) {
      correccion = 0;
    } else {
      graciaPIDFinalizada = true;
    }
  }

  velocidadActual.izquierda = constrain(VEL_BASE_DER - correccion, 0, 255);
  velocidadActual.derecha = constrain(VEL_BASE_IZQ + correccion, 0, 255);

  movimiento(AVANZAR, {velocidadActual.izquierda, velocidadActual.derecha});
  ```
- `src/hardware/movimiento/PID.cpp` (lines 7–33):
  ```cpp
  static int16_t errorAnterior = 0;

  int16_t calcularCorreccion(sensado mediciones){
      bool hayIzq = mediciones.distanciaIzq < (UMBRAL_PARED_ESTADO_NORMAL + 50);
      bool hayDer = mediciones.distanciaDer < (UMBRAL_PARED_ESTADO_NORMAL + 50);
      
      int16_t error = 0;
      if (hayIzq && hayDer) {
          error = (int16_t)mediciones.distanciaDer - (int16_t)mediciones.distanciaIzq;
      } else if (hayIzq) {
          error = - (int16_t)mediciones.distanciaIzq;
      } else if (hayDer) {
          error = (int16_t)mediciones.distanciaDer;
      } else {
          error = 0;
      }

      int16_t correccion = (KP * error) + (KD * (error - errorAnterior)) ;
      errorAnterior = error; 
      return constrain(correccion, -25,25);
  }
  ```
- `src/main.cpp` (lines 242–254) [FRENANDO state transition]:
  ```cpp
  case FRENANDO: {
    movimiento(FRENO_F, {0,0}); // Mantener el freno activo
    if (millis() - tiempoInicioFreno >= 150) { // 150ms de pausa estabilizadora
      if (estadoPostFreno == AVANZANDO) {
        iniciarAvanceCelda();
      } else {
        resetearErrorAnterior();
        resetearEncoders();
        estado = estadoPostFreno;
      }
    }
    break;
  }
  ```

### 1.2 Test Harness Artifact
Created standalone simulator and stress-test suite:
- File: `.agents/teamwork/challenger_2/test_m1_pid_fsm.py`
- Executable Python model implementing the exact math, state transitions, and edge cases of the firmware.

---

## 2. Logic Chain & Empirical Proof

### 2.1 Scenario 1: Post-Turn Entry to AVANZANDO (Pulses 0 through 100)
- **Premise:** When entering `AVANZANDO` following a turn, `iniciarAvanceCelda()` executes. `pulsosBaseCelda = 0`, `graciaPIDFinalizada = false`, and encoders start at 0.
- **Trace from Simulation:**
  With simulated sensor asymmetry (`distanciaIzq = 40 mm`, `distanciaDer = 60 mm`, raw error $e = +20\text{ mm}$):
  - **Pulse 0..99 ($0 \le \text{pulsosTotalesCelda} < 100$):**
    `pulsosTotalesCelda < PULSOS_GRACIA_PID` is `TRUE`.
    `correccion = 0`.
    Motors receive `vel_izq = 45`, `vel_der = 45`.
    Corrections for pulses 0 through 99 are **identically 0**.
  - **Pulse 100:**
    `pulsosTotalesCelda == 100`. The condition `100 < 100` evaluates to `FALSE`.
    Branch `else { graciaPIDFinalizada = true; }` executes.
    `correccion` is not overridden to 0; active PID correction engages.
- **Verification Result:** Pulses 0 through 99 (exactly 100 discrete encoder ticks of travel) are completely blind (`correccion == 0`). At pulse count 100, the 100-pulse grace interval is complete, and control authority is enabled.

### 2.2 Scenario 2: Behavior at Pulse 101 & Bumpless Transfer (Derivative Kick Check)
- **The Derivative Kick Problem:**
  In classical PID implementations where the controller is disabled during a blind window without updating internal state, `errorAnterior` remains `0`. When enabled at pulse 100 or 101 with an existing error $e = 20\text{ mm}$, the derivative term would compute:
  $$D_{\text{naive}} = K_d \cdot (e_k - 0) = 0.3 \cdot 20 = +6.0$$
  This causes an instantaneous voltage spike to the motors (derivative kick).
- **Worker's Implementation:**
  In `src/main.cpp:151`, `calcularCorreccion(sensadoActual)` is invoked **on every single control cycle** during the blind window.
  Inside `PID.cpp:30`, `errorAnterior` continuously updates with the true tracking error:
  $$e_{99} = 20 \implies \text{errorAnterior} = 20$$
  When the blind window expires at pulse 100/101:
  $$D_{\text{worker}} = K_d \cdot (e_{100} - e_{99}) = 0.3 \cdot (20 - 20) = 0.0$$
- **Simulation Measurement:**
  - Naive Controller derivative term: $+6.0$ (Kick detected).
  - Worker Controller derivative term at activation: $+0.0$ (BUMPLESS).
- **Verification Result:** Zero derivative kick. Bumpless transfer operates as intended.

### 2.3 Scenario 3: Encoder Reset at Pulse 250 by R1
- **Premise:** At pulse 250, a side wall drops from $\le 130$ mm to $> 130$ mm, triggering R1.
- **Trace from Simulation:**
  1. Flank detects at `pulsosActuales = 250`, `pulsosBaseCelda = 0`.
  2. `flancoDetectado` is set to `true`.
  3. `pulsosBaseCelda += pulsosActuales` $\implies \text{pulsosBaseCelda} = 250$.
  4. `resetearEncoders()` sets `pulsosActuales = 0`.
  5. `limitePulsosActual` is set to `PULSOS_CELDA_MEDIA = 400`.
  6. In subsequent cycles:
     $$\text{pulsosTotalesCelda} = \text{pulsosBaseCelda} + \text{pulsosActuales} = 250 + \text{pulsosActuales} \ge 250$$
  7. PID check evaluation:
     - Safeguard 1: `graciaPIDFinalizada` was already latched to `true` at pulse 100. The check `if (!graciaPIDFinalizada)` is false and bypassed.
     - Safeguard 2: Even without the latch, $\text{pulsosTotalesCelda} \ge 250 > 100$.
  8. Continuity of Derivative History: R1 calls `resetearEncoders()`, but intentionally does **NOT** call `resetearErrorAnterior()`. Thus, `errorAnterior` retains continuity across the encoder reset.
  9. Stopping Distance: The robot advances 400 pulses from the flank (`pulsosActuales == 400`), totaling 650 pulses in the cell, stopping squarely at the cell center. Watchdog ($1050$) is not triggered.
- **Verification Result:** PID correction does **NOT** revert to 0. Control continuity is 100% preserved.

### 2.4 Scenario 4: Multi-Cycle FSM Transitions and Invariants
- **Cycles Tested in Simulation:**
  1. `AVANZANDO` -> `FRENANDO` (150ms) -> `DECISION` (Right turn) -> `GIRANDO_DER` (300 pulses) -> `FRENANDO` (150ms) -> `AVANZANDO`.
  2. `AVANZANDO` (R1 flank reset at pulse 250) -> `FRENANDO` (150ms) -> `DECISION` (Straight) -> `AVANZANDO`.
  3. `AVANZANDO` (Front wall stop at $\le 50\text{ mm}$) -> `FRENANDO` (150ms) -> `DECISION` (Left turn) -> `GIRANDO_IZQ` (280 pulses) -> `FRENANDO` (150ms) -> `AVANZANDO`.
  4. `AVANZANDO` -> `FRENANDO` (150ms) -> `DECISION` (U-Turn) -> `GIRANDO_180` (300 pulses) -> `FRENANDO` (150ms) -> `AVANZANDO`.
  5. `AVANZANDO` (Sensor failure timeout) -> Watchdog Stop at 1050 pulses -> `FRENANDO` (150ms) -> `DECISION`.
- **Invariants Checked at Start of Each `AVANZANDO` Entry:**
  - `pulsos_actuales == 0` (Confirmed)
  - `pulsos_base_celda == 0` (Confirmed)
  - `flanco_detectado == false` (Confirmed)
  - `gracia_pid_finalizada == false` (Confirmed)
  - `limite_pulsos_actual == 800` (Confirmed)
  - `errorAnterior == 0` (Confirmed via `resetearErrorAnterior()`)
- **Deadlock Analysis in `DECISION`:**
  The decision conditions partition the state space completely:
  $$(D) \lor (\neg D \land C) \lor (\neg D \land \neg C \land I) \lor (\neg D \land \neg C \land \neg I) \equiv \mathbf{True}$$
  Every possible combination of sensor readings triggers exactly one turn or advance transition.
- **Verification Result:** Zero deadlocks, zero accumulator drift across cycles, zero variable leakage.

---

## 3. Caveats & Latent Hazards (Pre-Existing Codebase)

While the implementation for Milestone M1 is verified and sound, adversarial analysis revealed two pre-existing bugs in adjacent firmware modules that should be addressed in subsequent milestones:

1. **Latent Pointer Arithmetic Bug in `src/main.cpp:172` (Pre-existing):**
   ```cpp
   enviarString(sensadoActual.distanciaDer + " | " + sensadoActual.distanciaCent + sensadoActual.distanciaIzq);
   ```
   `sensadoActual.distanciaDer` is an `int16_t`. In C++, adding an integer to a string literal (`" | "`) executes pointer arithmetic, creating a pointer into random memory.
   *Impact:* Bluetooth telemetry string in `DECISION` will send garbage characters or cause undefined memory reads.
   *Recommendation for next milestone:* Replace with explicit Arduino `String` conversions: `String(sensadoActual.distanciaDer) + " | " + ...`.

2. **Asymmetric Single-Wall Error Bias in `src/hardware/movimiento/PID.cpp:18–22` (Pre-existing):**
   ```cpp
   } else if (hayIzq) {
       // Solo pared izquierda: mantenerse a la distancia ideal (OFSET_IZQ representa nuestro objetivo ideal)
       error = - (int16_t)mediciones.distanciaIzq;
   } else if (hayDer) {
       // Solo pared derecha
       error = (int16_t)mediciones.distanciaDer;
   ```
   When only one wall is present, the formula assigns raw distance rather than the difference `-(mediciones.distanciaIzq - OFSET_IZQ)`.
   *Impact:* When a wall drops (e.g. at pulse 250), raw error jumps from $\approx 0$ to $-40$, producing a proportional steering bias towards the open wall.
   *Scope note:* `PID.cpp` was outside M1 scope and must be tuned in a dedicated PID milestone.

---

## 4. Conclusion & Verdict

**Verdict:** **APPROVE**

1. **R3 (PID Blindness):** Correctly maintained at 0 for pulses 0 through 99 (100 elapsed pulses of grace).
2. **R3 (Bumpless Transfer):** Derivative kick is verified to be 0 at pulse 100/101 due to continuous state tracking in `calcularCorreccion`.
3. **R1 / R3 Decoupling:** R1 encoder reset at pulse 250 is protected by both `pulsosBaseCelda` accumulation and the `graciaPIDFinalizada` one-shot latch. PID correction does not revert to 0.
4. **R4 (State Machine & Mapping Absence):** FSM cycles cleanly through `AVANZANDO -> FRENANDO -> DECISION -> GIRANDO -> FRENANDO -> AVANZANDO` across all permutations without variable leaks, accumulator drift, or deadlocks. Zero mapping structures are present.

---

## 5. Verification Method

### 5.1 Test Harness Execution
Run the standalone verification suite:
```powershell
python .agents/teamwork/challenger_2/test_m1_pid_fsm.py
```
Expected output:
- `TEST 1 RESULT: PASS` (Pulses 0..99 zero correction).
- `TEST 2 RESULT: PASS` (Bumpless transfer confirmed, kick = 0).
- `TEST 3 RESULT: PASS` (PID does not revert to 0 on pulse 250 reset).
- `TEST 4 RESULT: PASS` (All 5 multi-cycle transitions pass).
- `ALL 4 CHALLENGER TESTS PASSED RIGOROUSLY!`

### 5.2 Invalidation Conditions
This verdict would be invalidated if:
1. `iniciarAvanceCelda()` failed to reset `pulsosBaseCelda` or `graciaPIDFinalizada` between cells.
2. `PID.cpp` did not update `errorAnterior` during blind cycles.
3. R1 reset logic set `pulsosBaseCelda = 0` instead of accumulating `pulsosBaseCelda += pulsosActuales`.
