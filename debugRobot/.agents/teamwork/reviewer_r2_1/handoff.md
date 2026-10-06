# Handoff Report — Reviewer 1 (Milestone M1 Iteration 2)

**Agent:** Reviewer 1 (`teamwork_preview_reviewer` / `reviewer_r2_1`)  
**Roles:** Reviewer, Critic  
**Date:** 2026-10-06  
**Working Directory:** `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\.agents\teamwork\reviewer_r2_1`  
**Recipient:** Orchestrator (`eeb17d92-d8a4-4a39-a22e-d0c4d184202b`)  
**Verdict:** **APPROVE**

---

## 1. Observation

### 1.1 Inspected Files and Lines
Authoritative source files examined directly:
1. `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\src\config.h`:
   - Line 69:
     ```cpp
     #define DISTANCIA_MIN_VALIDA -20
     ```
   - Lines 66-72:
     ```cpp
     #define PULSOS_CELDA 800
     #define PULSOS_CELDA_MEDIA 400
     #define DISTANCIA_PARADA_FRENTE 50
     #define DISTANCIA_MIN_VALIDA -20
     #define PULSOS_GRACIA_PID 100
     #define PULSOS_MIN_DETECCION_FLANCO 150
     #define PULSOS_WATCHDOG_SEGURIDAD 1050
     ```
2. `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\src\main.cpp`:
   - Line 34: Global latch variable declared:
     ```cpp
     bool aproximandoParedFrontal = false;
     ```
   - Line 49: State entry reset in `iniciarAvanceCelda()`:
     ```cpp
     aproximandoParedFrontal = false;
     ```
   - Lines 128-148 (`case AVANZANDO:` stopping and latching logic):
     ```cpp
     // R2: Condiciones de parada
     // 1. Parada por pared frontal a <= 50 mm (con guarda de validez física >= DISTANCIA_MIN_VALIDA)
     bool stopPorParedFrontal = (sensadoActual.distanciaCent >= DISTANCIA_MIN_VALIDA &&
                                 sensadoActual.distanciaCent <= DISTANCIA_PARADA_FRENTE);

     // 2. Detección de pared frontal en aproximación
     bool paredAlFrente = (sensadoActual.distanciaCent >= DISTANCIA_MIN_VALIDA &&
                           sensadoActual.distanciaCent <= UMBRAL_PARED_FRENTE);

     // Enclavar aproximación a pared frontal cerca o más allá del límite de celda
     if (paredAlFrente && pulsosActuales >= (limitePulsosActual - 100)) {
       aproximandoParedFrontal = true;
     }

     // 3. Parada por encoders (400 pulsos si hubo flanco, o 800 fallback si no hubo flanco).
     // R2 sobreescribe la parada por encoders si hay pared al frente o si se enclavó aproximación.
     bool stopPorEncoders = (pulsosActuales >= limitePulsosActual) && !paredAlFrente && !aproximandoParedFrontal;

     // 4. Watchdog de seguridad (anti-colisión ante fallo de sensor frontal)
     bool stopPorWatchdog = (pulsosTotalesCelda >= PULSOS_WATCHDOG_SEGURIDAD);
     ```
   - Lines 106-126 (`case AVANZANDO:` R1 falling edge detection):
     ```cpp
     // R1: Detección y reseteo por flanco lateral
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
   - Line 178 (`case DECISION:` logging telemetry string formatting):
     ```cpp
     sensadoActual = actualizarSensado();
     enviarString(String(sensadoActual.distanciaDer) + " | " + String(sensadoActual.distanciaCent) + " | " + String(sensadoActual.distanciaIzq));
     ```
3. `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\src\hardware\sensoresDistancia\sensoresDistancia.cpp`:
   - Lines 94-98: `distanciaCent = rawCent - OFSET_CENT` where `OFSET_CENT = 50`. Minimum physical reading before saturation is ~30 mm raw, yielding `distanciaCent` in range $[-20, 0]\text{ mm}$.

---

## 2. Logic Chain

### 2.1 Resolution of Challenger 1 Vulnerability 1 (Early Front Obstacle Stopping)
- **Prior Flaw:** In Iteration 1, `stopPorParedFrontal` required `pulsosTotalesCelda > 100`. An obstacle $\le 50\text{ mm}$ at the start of a cell was ignored for 100 pulses (~22.5 mm travel), leading to collisions.
- **Verification of Fix:** The conjunction `pulsosTotalesCelda > 100` was completely removed. `stopPorParedFrontal` is now evaluated unconditionally:
  $$\text{stopPorParedFrontal} = (\text{distanciaCent} \ge -20 \land \text{distanciaCent} \le 50)$$
- **Deduction:** If a front wall is within $50\text{ mm}$ at pulse $0$, the robot halts on the initial iteration and transitions to `FRENANDO`. No blind movement occurs.

### 2.2 Resolution of Challenger 1 Vulnerability 2 (Offset Underflow Down to -20 mm)
- **Prior Flaw:** The guard `distanciaCent > 0` rejected readings in $[-20, 0]\text{ mm}$, which naturally occur in mechanical bumper contact or tight calibration ($rawCent \in [30, 50]$ with $OFSET\_CENT = 50$).
- **Verification of Fix:** Replaced with `>= DISTANCIA_MIN_VALIDA` ($-20\text{ mm}$) across both `stopPorParedFrontal` and `paredAlFrente`.
- **Deduction:** Physical contact or near-zero readings are properly classified as active front obstacles. The lower bound of $-20\text{ mm}$ simultaneously guards against invalid bus noise or disconnected sensor readings ($rawCent = 0 \implies -50\text{ mm} < -20\text{ mm}$).

### 2.3 Resolution of Challenger 1 Vulnerability 3 (Latching Noise Immunity Past 800 Pulses)
- **Prior Flaw:** Past 800 pulses, `stopPorEncoders` was suppressed solely by the instantaneous condition `!paredAlFrente`. A single ToF optical spike $> 120\text{ mm}$ caused an immediate premature stop at 70–90 mm from the front wall.
- **Verification of Fix:**
  1. When approaching a front wall near the cell limit ($\text{pulsosActuales} \ge \text{limitePulsosActual} - 100$) and $\text{paredAlFrente}$ is true, `aproximandoParedFrontal = true` latches.
  2. `stopPorEncoders` requires `!aproximandoParedFrontal`.
- **Deduction:** Once latched, even if the sensor returns a single spike $> 120\text{ mm}$ at pulse 820, `!aproximandoParedFrontal` evaluates to `false`, preventing premature encoder stopping. The robot completes its approach until $\le 50\text{ mm}$ or until the 1050-pulse safety watchdog fires.

### 2.4 Integrity and Operational Verification of R1, R3, R4
- **R1 (Falling Edge & Fallback):**
  - Requires prior presence (`habiaParedIzq` / `habiaParedDer`) before triggering on transition $> 130\text{ mm}$.
  - Filter `pulsosTotalesCelda > 150` prevents entry false positives.
  - One-shot latch `flancoDetectado = true` ensures exactly one reset per cell.
  - Encoder reset targets exactly 400 pulses to cell center.
  - If no lateral walls exist, `limitePulsosActual` remains at 800 pulses (fallback).
  - Pure reactive execution: non-blocking, zero delays.
- **R3 (PID Grace & Decoupling):**
  - Forced to 0 for pulses 0..99 (`pulsosTotalesCelda < PULSOS_GRACIA_PID`).
  - Continuous tracking in `calcularCorreccion()` prevents derivative kick at pulse 100 (bumpless transfer).
  - R1 reset increments `pulsosBaseCelda` monotonically and `graciaPIDFinalizada` is latched; PID remains active post-flank without secondary silence.
- **R4 & FSM Architecture:**
  - Absolute absence of mapping structures, coordinate tracking $(x, y)$, or grids.
  - Unified stopping path through `FRENANDO` (150 ms stabilizing brake).
  - `DECISION` state string logging uses safe Arduino `String` objects, eliminating prior pointer arithmetic bug.

### 2.5 Integrity Assessment
- Checked for hardcoded test outputs or shortcuts: **None found**.
- Checked for dummy / facade logic: **None found**. Real embedded logic.
- Checked for external task delegation bypasses: **None found**.
- Self-certification vs independent verification: Reviewed through independent static analysis, mathematical state verification, and boundary challenge modeling.
- **Integrity Tag:** **CLEAN — NO INTEGRITY VIOLATION**.

---

## 3. Caveats

1. **Hardware-in-the-Loop:** Analysis and verification are based on complete static inspection of the ESP32 C++ firmware, hardware driver source code, and mathematical state transitions. Physical execution on target silicon was not performed due to absence of hardware probe in the environment.
2. **Latent Peripheral Behavior (PID asymmetry):** As noted by Challenger 2, `PID.cpp` computes single-wall error using raw millimeters rather than offset deviation. This was identified as pre-existing code outside Milestone M1 scope and does not impact M1 odometry acceptance criteria.
3. No other caveats.

---

## 4. Conclusion

**Verdict:** **APPROVE**

All three vulnerabilities identified by Challenger 1 in Iteration 1 have been completely and cleanly resolved in `src/main.cpp` and `src/config.h`:
1. Immediate front stop $\le 50\text{ mm}$ operates from pulse 0 without delay.
2. Negative sensor readings down to $-20\text{ mm}$ are recognized as valid front obstacles.
3. Latch `aproximandoParedFrontal` provides immunity against optical noise spikes past 800 pulses.
4. R1 lateral flank detection, 400-pulse center stop, and 800-pulse fallback remain fully operational and non-blocking.
5. Telemetry string construction in `DECISION` is clean and type-safe.
6. Zero integrity violations detected.

Milestone M1 is ready for final sign-off.

---

## 5. Verification Method

### 5.1 Static Code Inspection Points
1. Inspect `src/config.h`:
   - Line 69: Confirm `#define DISTANCIA_MIN_VALIDA -20`.
2. Inspect `src/main.cpp`:
   - Line 34 & 49: Confirm `aproximandoParedFrontal` declaration and reset in `iniciarAvanceCelda()`.
   - Lines 129-147: Confirm `stopPorParedFrontal` has no `> 100` pulse check, uses `>= DISTANCIA_MIN_VALIDA`, and `stopPorEncoders` includes `!aproximandoParedFrontal`.
   - Line 178: Confirm `enviarString(String(...) + ...)` format.

### 5.2 Deterministic Simulation Suite
Inspect `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\.agents\teamwork\reviewer_r2_1\verify_m1_iteration2.py` which models the exact C++ logic and validates all 10 adversarial scenarios:
- V1 / V1b: Immediate front stop at $\le 50\text{ mm}$ (pulses 0 and 10).
- V2 / V2b / V2c: Underflow tolerance ($-5\text{ mm}$ and $-20\text{ mm}$ pass; $-25\text{ mm}$ rejected).
- V3: Optical spike at pulse 820 ignored via latch; stop at 870 pulses ($\le 50\text{ mm}$).
- R1.1 / R1.2: Left/Right wall falling edges reset to 400 pulses.
- R1.3: Absence of lateral walls falls back to 800 pulses.
- R1.4: Notch $< 150$ pulses ignored.
- R2+R1: Front wall overrides 400-pulse limit post-flank.
- Watchdog: Fires at 1050 pulses.
- R3.1 / R3.2: 100-pulse grace period and decoupling from R1.

### 5.3 Invalidation Conditions
This approval would be invalidated if:
1. `stopPorParedFrontal` retained any minimum pulse requirement.
2. A sensor reading of $-5\text{ mm}$ was treated as open corridor.
3. A single optical reading spike $> 120\text{ mm}$ at pulse 820 triggered immediate encoder stop.
Because the verified source code in `src/main.cpp` prevents all three failure modes, the approval stands.
