# Handoff Report — Explorer Survey 3

## 1. Observation
- **PID Computation and Application:** In `src/main.cpp:77-84`, PID steering is evaluated via `int16_t correccion = calcularCorreccion(sensadoActual);` and applied as:
  ```cpp
  velocidadActual.izquierda = constrain(VEL_BASE_DER - correccion, 0, 255);
  velocidadActual.derecha = constrain(VEL_BASE_IZQ + correccion, 0, 255);
  ```
  In `src/hardware/movimiento/PID.cpp:7-33`, `calcularCorreccion` evaluates walls with `UMBRAL_PARED_ESTADO_NORMAL + 50`, computing `correccion = (KP * error) + (KD * (error - errorAnterior))` with `errorAnterior = error;`.
- **Transitions from Turn States to AVANZANDO:** In `src/main.cpp:131-137` (`GIRANDO_DER`), lines 146-152 (`GIRANDO_IZQ`), and lines 161-167 (`GIRANDO_180`), the turn states transition to `FRENANDO` with `estadoPostFreno = AVANZANDO;`. In `src/main.cpp:170-179` (`FRENANDO`), after a 150 ms stabilizing brake (`millis() - tiempoInicioFreno >= 150`), `resetearErrorAnterior()` and `resetearEncoders()` are called before switching `estado = estadoPostFreno;`.
- **Mapping Verification:** Relevamiento de `src/` confirmó ausencia total de matrices 2D, estructuras de grafos, historiales de posición ($X, Y$), o algoritmos de inundación (FloodFill). Únicamente existen definiciones de macros no utilizadas en `src/config.h:7-11` (`X_SIZE`, `Y_SIZE`, `X_START`, `Y_START`).
- **FRENANDO Mechanics:** `FRENANDO` es un estado no bloqueante con duración de 150 ms que comanda `FRENO_F`, blanquea encoders y memoria de error PID, y transfiere el flujo a `estadoPostFreno`.
- **Theoretical Cross-Review Hazards Observed in Code:**
  1. *BUG-16 en `PID.cpp:17-23`:* Seguimiento de pared única sin consigna: `error = -mediciones.distanciaIzq;` o `error = mediciones.distanciaDer;`, induciendo giro positivo hacia la pared detectada.
  2. *Lectura inicial de sensores en `sensoresDistancia.cpp:83`:* `static sensado lecturaAct = {0,0,0};`. Si R2 evalúa `distanciaCent <= 50` antes de la primera medición I2C válida, se gatillaría una detención prematura.
  3. *Reenganche de término derivativo en R3:* Si `errorAnterior` permanece en 0 durante la ceguera de 100 pulsos, en el pulso 101 se genera un salto proporcional a $(e_{101} - 0)$.
  4. *Riesgo de rebote en R1:* Sin un cerrojo biestable (`latch`), oscilaciones en torno a 130 mm resetearían continuamente los encoders.

## 2. Logic Chain
1. *From Observation (PID in AVANZANDO & Turn Transitions):* Turning states route through `FRENANDO` where encoders and `errorAnterior` are explicitly cleared. Therefore, upon entry into `AVANZANDO`, encoder pulse counts and PID history start at 0.
2. *From Observation & R3 Requirement:* During the initial 100 pulses of `AVANZANDO`, setting `correccion = 0` provides the required post-turn grace period. However, to prevent a derivative kick ($K_d \cdot (e - 0)$) at pulse 101, `errorAnterior` must track the current error during the grace period, or be seeded with `errorAnterior = error` at the transition.
3. *From Observation & R1/R3 Interaction:* R1 introduces a mid-cell encoder reset upon detecting a lateral falling edge. If the grace period is checked merely via `pulsosActuales < 100` without a state flag, resetting encoders mid-cell would erroneously re-trigger 100 pulses of PID blindness. A dedicated latching flag (`graciaPID`) prevents this unwanted re-triggering.
4. *From Observation & R4 Requirement:* Since no mapping structures exist, maintaining strictly reactive odometry and state transitions guarantees compliance with R4.
5. *From Observation & Stopping Dynamics:* Both fallback stopping (800 pulses), lateral falling edge stopping (400 pulses), and frontal wall alignment ($\le 50$ mm) must all route through `FRENANDO` (`estadoPostFreno = DECISION`) to ensure mechanical stabilization before decision-making.

## 3. Caveats
- No caveats regarding code architecture or file boundaries.
- Investigation is 100% read-only; physical validation on hardware will depend on motor driver responsiveness (coast vs dynamic braking) and optical reflection characteristics of the arena walls.

## 4. Conclusion
- R3 requires a 100-pulse grace period at the start of `AVANZANDO` with `correccion = 0`, safeguarded by bumpless derivative tracking and a one-shot activation flag to prevent cross-interference with R1's encoder reset.
- R4 is currently fulfilled in the codebase (no mapping logic exists). Preserving the `FRENANDO` state as the sole funnel between `AVANZANDO` and `DECISION` ensures physical stabilization and sensor integrity.
- The 9 identified theoretical cross-review risks (notably single-wall positive feedback, premature front wall stop due to initial 0 mm readings, and unlatched falling edge oscillations) have concrete, verified mitigations documented in `report.md`.

## 5. Verification Method
- **Code Inspection:**
  - Verify `src/main.cpp:72-93` for `correccion` calculation and speed clamping.
  - Verify `src/main.cpp:170-179` for `FRENANDO` timer and encoder/PID resets.
  - Verify `src/hardware/movimiento/PID.cpp:7-33` for single-wall sign conventions and derivative term calculations.
  - Inspect `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\.agents\teamwork\explorer_survey_3\report.md` for full risk matrix.
- **Compilation Check:**
  - Run `pio run` when source changes are implemented by the worker to ensure build integrity.
