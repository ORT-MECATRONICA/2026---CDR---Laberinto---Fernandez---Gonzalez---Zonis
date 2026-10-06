# Handoff Report — Reviewer 2 (Milestone M1 Iteration 2)

**Agent:** Reviewer 2 (`teamwork_preview_reviewer`)  
**Roles:** Reviewer, Adversarial Critic  
**Working Directory:** `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\.agents\teamwork\reviewer_r2_2`  
**Parent Conversation ID:** `eeb17d92-d8a4-4a39-a22e-d0c4d184202b`  
**Milestone:** M1 Iteration 2 — Micromouse Odometry Correction Fixes  
**Verdict:** **APPROVE**  

---

## 1. Observation

Direct examination of `src/main.cpp`, `src/config.h`, `src/hardware/movimiento/PID.cpp`, `src/hardware/movimiento/PID.h`, and `src/hardware/sensoresDistancia/sensoresDistancia.cpp` yielded the following verified observations:

### 1.1 Integrity & Anti-Cheating Verification
- **Hardcoded test fixtures / mocks:** None found. No mocked distance values, dummy encoder increments, or simulated sensor loops exist in `src/`.
- **Facade implementations:** None. Closed-loop control runs real hardware peripherals (VL53L0X via I2C, PCNT hardware encoders, LEDC PWM motor channels).
- **Shortcut bypasses:** No shortcuts. All odometry mechanics and state transitions execute deterministically according to requirements.
- **Fabricated verification outputs:** None detected.

### 1.2 Observation: Requirement R3 (PID Blindness & Bumpless Transfer)
In `src/main.cpp` lines 156–173:
```cpp
        // R3: Corrección PID y gracia post-giro (ceguera en los primeros 100 pulsos con bumpless transfer)
        int16_t correccion = calcularCorreccion(sensadoActual);
        if (!graciaPIDFinalizada) {
          if (pulsosTotalesCelda < PULSOS_GRACIA_PID) {
            correccion = 0;
          } else {
            graciaPIDFinalizada = true;
          }
        }

        // FIX PID: El Motor A (velocidadActual.izquierda) está mecánicamente en la rueda DERECHA.
        // Por eso invertimos los signos de la corrección y asignamos cruzadas las velocidades base.
        velocidadActual.izquierda = constrain(VEL_BASE_DER - correccion, 0, 255);
        velocidadActual.derecha = constrain(VEL_BASE_IZQ + correccion, 0, 255);

        movimiento(AVANZAR, {velocidadActual.izquierda, velocidadActual.derecha});
```
- In `src/config.h` line 70: `#define PULSOS_GRACIA_PID 100`.
- During pulses 0 to 99 (`pulsosTotalesCelda < 100`), `correccion` is unconditionally clamped to `0`. Both wheels run at their base PWM speeds (`VEL_BASE_DER` and `VEL_BASE_IZQ`).
- `calcularCorreccion(sensadoActual)` runs every loop cycle, updating `errorAnterior` in `src/hardware/movimiento/PID.cpp:30` (`errorAnterior = error;`).
- When `pulsosTotalesCelda >= 100`, `graciaPIDFinalizada` is set to `true`. Because `errorAnterior` tracked sensor error during the grace period, the derivative term `KD * (error - errorAnterior)` does not experience a step change from zero (bumpless transfer).

### 1.3 Observation: Decoupling of R3 Blindness from R1 Dynamic Resets
In `src/main.cpp` lines 107–126:
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
- R1 edge detection requires `pulsosTotalesCelda > PULSOS_MIN_DETECCION_FLANCO` (150 pulses).
- When triggered, `pulsosBaseCelda += pulsosActuales` preserves the cumulative advance ($\ge 150$), and `flancoDetectado = true` prevents re-arming.
- `pulsosTotalesCelda = pulsosBaseCelda + pulsosActuales` (line 102) remains $\ge 150$.
- `graciaPIDFinalizada` is already `true` and is NOT modified by the R1 reset block.
- Therefore, the PID is never re-blinded mid-cell.

### 1.4 Observation: Requirement R4 (Stateless Reactive Architecture & Absence of Mapping)
- Global search for 2D arrays, graph grids, coordinate structs, or history buffers across `src/` yielded 0 results.
- Maze navigation is strictly reactive based on instantaneous sensor readings in `DECISION` (lines 180–185) executing the Right-Hand Rule.
- Requirement R4 is 100% satisfied.

### 1.5 Observation: Stopping Trigger Convergence into FRENANDO (150 ms) -> DECISION
In `src/main.cpp` lines 128–155:
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

      if (stopPorParedFrontal || stopPorEncoders || stopPorWatchdog) {
        enviarString(">>> INGRESO A DECISIÓN (FRENANDO) <<<");
        movimiento(FRENO_F, {0,0});
        tiempoInicioFreno = millis();
        estadoPostFreno = DECISION;
        estado = FRENANDO;
      }
```
And in `src/main.cpp` lines 248–260:
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
- Every stop condition (`stopPorParedFrontal`, `stopPorEncoders`, `stopPorWatchdog`) sets:
  - `movimiento(FRENO_F, {0,0});`
  - `tiempoInicioFreno = millis();`
  - `estadoPostFreno = DECISION;`
  - `estado = FRENANDO;`
- In `FRENANDO`, active counter-EMF dynamic braking (`FRENO_F`) is held for 150 ms (`millis() - tiempoInicioFreno >= 150`).
- Because `estadoPostFreno == DECISION`, lines 254–256 execute:
  - `resetearErrorAnterior();`
  - `resetearEncoders();`
  - `estado = DECISION;`
- Convergence is unified, clean, and complete.

### 1.6 Observation: Cleanliness of `iniciarAvanceCelda()`
In `src/main.cpp` lines 40–53:
```cpp
void iniciarAvanceCelda() {
  resetearEncoders();
  resetearErrorAnterior();
  limitePulsosActual = PULSOS_CELDA;
  flancoDetectado = false;
  habiaParedIzq = false;
  habiaParedDer = false;
  pulsosBaseCelda = 0;
  graciaPIDFinalizada = false;
  aproximandoParedFrontal = false;
  sensadoActual = actualizarSensado();
  enviarString(">>> AVANZANDO <<<");
  estado = AVANZANDO;
}
```
- Resets hardware encoders via `resetearEncoders()`.
- Resets derivative history via `resetearErrorAnterior()`.
- Resets target pulse threshold: `limitePulsosActual = 800` (`PULSOS_CELDA`).
- Resets R1 edge detection latch: `flancoDetectado = false`.
- Resets R1 side wall history flags: `habiaParedIzq = false`, `habiaParedDer = false`.
- Resets cell base distance counter: `pulsosBaseCelda = 0`.
- Resets R3 PID grace period latch: `graciaPIDFinalizada = false`.
- Resets front wall approach noise latch: `aproximandoParedFrontal = false`.
- Flushes sensor buffer: `sensadoActual = actualizarSensado()`.
- Sets state: `estado = AVANZANDO`.
- Call site coverage:
  - From `LISTO` (button launch, line 87): calls `iniciarAvanceCelda()`.
  - From `DECISION` (straight advance, line 191): calls `iniciarAvanceCelda()`.
  - From `FRENANDO` post-turn (`GIRANDO_DER`, `GIRANDO_IZQ`, `GIRANDO_180`, line 252): calls `iniciarAvanceCelda()`.
- No entry into `AVANZANDO` bypasses `iniciarAvanceCelda()`.

### 1.7 Observation: Iteration 2 Fixes for Challenger 1 Findings
1. **Unsafe 100-pulse guard removed from `stopPorParedFrontal`:** Line 130 no longer requires `pulsosTotalesCelda > 100`. Early front walls ($\le 50\text{ mm}$) trigger immediate stop at pulse 0 without collisions.
2. **Contact tolerance added:** `src/config.h:69` defines `#define DISTANCIA_MIN_VALIDA -20`. Lines 130 and 134 use `>= DISTANCIA_MIN_VALIDA`, properly recognizing obstacles down to mechanical bumper contact ($rawCent \in [30, 50]\text{ mm}$).
3. **ToF noise immunity latch added:** Lines 138–144 latch `aproximandoParedFrontal = true` when approaching a front wall near cell limit (`pulsosActuales >= limitePulsosActual - 100`). An optical noise spike $> 120\text{ mm}$ does not cause premature encoder stop.
4. **Bluetooth telemetry sanitized:** Line 178 now uses `String` objects instead of unsafe pointer arithmetic on literal strings.

---

## 2. Logic Chain

1. **Integrity Chain:**
   Code review confirms genuine hardware driver calls and state transitions. No hardcoded fixtures, mocks, or facades exist.

2. **R3 Logic Chain (PID Grace Period):**
   - Entering `AVANZANDO` resets `graciaPIDFinalizada = false` and `pulsosBaseCelda = 0`.
   - For all loop cycles where `pulsosTotalesCelda < 100`, `correccion` is forced to `0`. Motor outputs remain balanced at base velocities.
   - `calcularCorreccion(sensadoActual)` executes continuously, keeping `errorAnterior` in sync with real tracking error.
   - At pulse 100, `graciaPIDFinalizada = true` latches, and the PID correction transitions seamlessly into the control loop with zero derivative kick.

3. **R3/R1 Decoupling Logic Chain:**
   - R1 edge detection triggers only after `pulsosTotalesCelda > 150`.
   - At trigger time, `pulsosBaseCelda += pulsosActuales` accumulates the $\ge 150$ pulses, and `flancoDetectado = true` latches.
   - Post-reset, `pulsosTotalesCelda` remains $\ge 150$. It never drops below 100.
   - `graciaPIDFinalizada` remains `true`. The PID is not re-blinded mid-cell.

4. **R4 Logic Chain (Absence of Mapping & FSM Compliance):**
   - No 2D arrays, grids, or coordinate histories exist.
   - The robot relies entirely on reactive rules and odometric cell bounds.
   - All exits from `AVANZANDO` route through `FRENANDO` for 150 ms before entering `DECISION`.

5. **Stop Condition & Noise Latch Logic Chain:**
   - In open corridors, `paredAlFrente` is false, `aproximandoParedFrontal` remains false, and `stopPorEncoders` halts the robot cleanly at `limitePulsosActual`.
   - In corridors with a front wall, `aproximandoParedFrontal` latches when `pulsosActuales >= limitePulsosActual - 100`, suppressing `stopPorEncoders` even if a single ToF sample exceeds 120 mm.
   - The robot proceeds until `distanciaCent <= 50 mm`, triggering `stopPorParedFrontal`.
   - In the event of total sensor failure, `stopPorWatchdog` at 1050 pulses provides absolute fail-safe termination.

---

## 3. Caveats

- **Hardware Execution:** Static code analysis, architectural tracing, and logic simulation were conducted. Physical bench testing with the actual ESP32 microcontroller, motors, and track requires hardware connection not present in this headless environment.
- **No other caveats.**

---

## 4. Conclusion

Worker 2 (`worker_m1_fix`) has cleanly and robustly implemented all fixes required from Iteration 1:
1. R3 PID blindness and bumpless transfer are fully preserved and decoupled from R1 resets.
2. R4 absence of mapping is strictly observed.
3. All stopping triggers converge into `FRENANDO` (150 ms) -> `DECISION`.
4. `iniciarAvanceCelda()` cleanly resets all state variables including `aproximandoParedFrontal`.
5. Early front-wall stop, optical noise immunity, and bumper contact tolerance are verified.

**Verdict: APPROVE**

---

## 5. Verification Method

To independently verify the implementation:

1. **Verify R3 Grace Period & Bumpless Transfer:**
   - Inspect `src/main.cpp` lines 157–164.
   - Confirm `calcularCorreccion(sensadoActual)` runs unconditionally before checking `pulsosTotalesCelda < PULSOS_GRACIA_PID`.
   - *Invalidation condition:* If `calcularCorreccion` were conditionally bypassed when `pulsosTotalesCelda < 100`, `errorAnterior` would not track, causing a derivative kick at pulse 100.

2. **Verify Decoupling from R1:**
   - Inspect `src/main.cpp` lines 102, 118–125, and 158.
   - Confirm `pulsosTotalesCelda = pulsosBaseCelda + pulsosActuales` and that `pulsosBaseCelda += pulsosActuales` occurs on edge detection ($\ge 150$ pulses).
   - *Invalidation condition:* If `pulsosTotalesCelda` dropped below 100 or if `graciaPIDFinalizada` were reset on R1 trigger, PID would re-blind mid-cell.

3. **Verify R4 Absence of Mapping:**
   - Run grep for 2D array declarations or coordinate tracking structs across `src/`: confirm 0 results.

4. **Verify FRENANDO Convergence:**
   - Inspect `src/main.cpp` lines 149–154 and 248–260.
   - Confirm all stop triggers transition to `FRENANDO` with `estadoPostFreno = DECISION` and active braking for 150 ms.

5. **Verify `iniciarAvanceCelda()` Clean Reset:**
   - Inspect `src/main.cpp` lines 40–53.
   - Confirm reset of `limitePulsosActual`, `flancoDetectado`, `habiaParedIzq`, `habiaParedDer`, `pulsosBaseCelda`, `graciaPIDFinalizada`, and `aproximandoParedFrontal`.
