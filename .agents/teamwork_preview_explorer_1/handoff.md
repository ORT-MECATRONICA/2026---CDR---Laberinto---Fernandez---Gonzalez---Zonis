# Handoff Report: Navigation, State Machine, and PID Controller Exploration

**Agent:** teamwork_preview_explorer (Navigation Specialist)  
**Recipient:** teamwork_preview_orchestrator (Parent ID: `c4ffffd0-ef10-4b0d-9f69-3912147be295`)  
**Timestamp:** 2026-09-20T00:48:00Z  
**Target Path:** `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\bahiaBlanca`

---

## 1. Observation

1. **Incomplete State Machine in `rightHand.cpp`**:
   - `src/maquinaEstados/rightHand.h` lines 13–20 declares:
     ```cpp
     enum SUBESTADOS {
         AVANZANDO,
         PREPARANDOME_PARA_GIRAR_DER,
         GIRANDO_DER,
         PREPARANDOME_PARA_GIRAR_IZQ,
         GIRANDO_IZQ,
         FIN,
     };
     ```
   - In `src/maquinaEstados/rightHand.cpp` lines 8–50, the `switch (estadoActual)` contains ONLY:
     ```cpp
     switch (estadoActual){
         case AVANZANDO: {
             ...
             break;
         }
     }
     ```
     Sub-states `PREPARANDOME_PARA_GIRAR_DER`, `GIRANDO_DER`, `PREPARANDOME_PARA_GIRAR_IZQ`, `GIRANDO_IZQ`, and `FIN` are unhandled.
     No 180° rotation state (`GIRANDO_180`) is declared or implemented.

2. **Debounce and Transition Defects in `rightHand.cpp`**:
   - Lines 13–19 of `rightHand.cpp`:
     ```cpp
     if(sensadoActual.distanciaDer > UMBRAL_PARED_ESTADO_NORMAL){
         filtroSensores.counterDer++;
     } else if (sensadoActual.distanciaCent > UMBRAL_PARED_ESTADO_NORMAL){
         filtroSensores.counterCent++;
     } else if (sensadoActual.distanciaIzq > UMBRAL_PARED_ESTADO_NORMAL){
         filtroSensores.counterIzq++;
     }
     ```
     If a condition evaluates to false, the counter is never reset (`counter = 0` is absent).
     The `else if` cascade prevents multiple openings or dead ends from being evaluated in the same cycle.
   - Lines 21–23 of `rightHand.cpp`:
     ```cpp
     if(filtroSensores.counterDer > 5){
         estadoActual = PREPARANDOME_PARA_GIRAR_IZQ;
     ```
     When the right sensor sees an opening, the code sets `PREPARANDOME_PARA_GIRAR_IZQ` (inverted logic).
   - Lines 25–26 and 40–41:
     `constrain(error, -50, 50);` is called without assigning its return value (`error = constrain(...)`).
   - Line 35:
     ```cpp
     } else {
         //=================== ATENCIÓN ANTIGRAVITY ===================
         //NO ENTIENDO QUE PUEDO HACER ACÁ
     }
     ```
     When surrounded by walls (dead end), no action is taken.

3. **PID Controller Implementation and Guard Clause in `PID.cpp`**:
   - In `src/hardware/movimiento/PID.cpp` lines 7–26:
     ```cpp
     int16_t calcularCorreccion(sensado mediciones){
         bool hayIzq = mediciones.distanciaIzq < (UMBRAL_PARED_ESTADO_NORMAL + 50);
         bool hayDer = mediciones.distanciaDer < (UMBRAL_PARED_ESTADO_NORMAL + 50);
         int16_t error = 0;
         if (hayIzq && hayDer) {
             error = (int16_t)mediciones.distanciaIzq - (int16_t)mediciones.distanciaDer;
         } else if (hayIzq) {
             error = (int16_t)mediciones.distanciaIzq - OFSET_IZQ;
         } else if (hayDer) {
             error = OFSET_DER - (int16_t)mediciones.distanciaDer;
         } else {
             error = 0;
         }
         ...
     ```
   - In `src/hardware/movimiento/PID.cpp` lines 35–41:
     `calcularCorreccionRightHand(int16_t error)` exists but is never invoked anywhere.
   - In `src/config.h`:
     `KP 0.5`, `KD 0.3`, `UMBRAL_PARED_ESTADO_NORMAL 100`, `UMBRAL_PARED_FRENTE 120`.
     Constants like `+ 50`, `5`, `-50, 50` are magic numbers not in `config.h`.

4. **Sign Inversion in Steering**:
   - In `rightHand.cpp` lines 27–30 & 42–45:
     `izquierda = VEL_BASE_IZQ + error;`
     `derecha   = VEL_BASE_DER - error;`
   - When closer to the right wall (`distanciaDer < distanciaIzq`), `PID.cpp` computes:
     `error = distanciaIzq - distanciaDer > 0`.
     Since `error > 0`, left motor accelerates and right motor decelerates, turning the robot to the RIGHT (into the right wall).

5. **Non-blocking Hardware Primitives**:
   - `src/hardware/encoders/encoders.h` provides `verPulsosEncoderA()`, `verPulsosEncoderB()`, `resetearEncoders()`.
   - `src/hardware/movimiento/puenteH.h` provides `movimiento(MOVIMIENTOS, VELOCIDAD)`.
   - `src/config.h` already defines `PULSOS_90_GRADOS 110`, `PULSOS_AVANCE_PREGIRO 250`, `PULSOS_AVANCE_PREGIRO_IZQ 100`, `TIEMPO_90_GRADOS 350`, `TIEMPO_AVANCE_PREGIRO 400`.

---

## 2. Logic Chain

1. **R2 (Non-blocking state machine with debounce)**:
   - Observation 1 shows that `right_hand()` returns to `main.cpp`'s `loop()` every cycle, but only handles `AVANZANDO`.
   - Observation 2 shows that debounce counters do not reset when conditions break, accumulating noise across non-consecutive frames.
   - Observation 2 shows right-turn logic was inverted to `PREPARANDOME_PARA_GIRAR_IZQ`.
   - *Deduction*: `rightHand.cpp` must implement all sub-states non-blockingly using encoders or `millis()`. Debounce counters must evaluate each sensor condition independently and reset (`counter = 0`) whenever the condition is false. Once $N$ consecutive readings occur, it triggers the transition and resets the debounce struct.

2. **R3 (Dead-end detection)**:
   - Observation 2 shows that if all 3 directions are blocked by walls (`distanciaDer <= UMBRAL`, `distanciaCent <= UMBRAL_PARED_FRENTE`, `distanciaIzq <= UMBRAL`), none of the existing `if` branches trigger.
   - Observation 1 shows no 180° rotation sub-state exists.
   - *Deduction*: Simultaneous evaluation `(hayParedDer && hayParedCent && hayParedIzq)` with debounce counter `callejon` is required. When confirmed, the robot transitions to `GIRANDO_180`, turning in place for `PULSOS_180_GRADOS` (or `TIEMPO_180_GRADOS`) non-blockingly.

3. **R4 (Centering in intersections)**:
   - Observation 5 shows that pre-turn constants (`PULSOS_AVANCE_PREGIRO 250`, `TIEMPO_AVANCE_PREGIRO 400`) are already defined in `config.h`.
   - Observation 1 shows that `PREPARANDOME_PARA_GIRAR_DER` was defined in `rightHand.h` but unhandled in `rightHand.cpp`.
   - *Deduction*: When an opening is confirmed, entering `PREPARANDOME_PARA_GIRAR_DER` advances the robot until `abs(verPulsosEncoderA()) >= PULSOS_AVANCE_PREGIRO`. This shifts the rotation axis to the cell intersection center before switching to `GIRANDO_DER`, preventing rear-chassis collision with the wall corner.

4. **R5 (PID protection against open gaps)**:
   - Observation 3 shows the magic number `(UMBRAL_PARED_ESTADO_NORMAL + 50)` allows readings up to 150 mm to be treated as walls.
   - Observation 4 demonstrates a critical sign inversion causing steering into walls.
   - *Deduction*: A strict mathematical guard clause is needed in `calcularCorreccion`:
     - If `distancia > UMBRAL_PARED_VALIDA_PID`, that wall is discarded as an open void.
     - If one side is void, PID regulates single-wall distance against the remaining wall only.
     - If both sides are void, error is set to 0.
     - Inverting the error formula to `error = distanciaDer - distanciaIzq` aligns motor steering away from obstacles.
     - Move all thresholds and constants to `config.h`.

---

## 3. Caveats

- Hardware encoder calibration values (`PULSOS_90_GRADOS 110`, `PULSOS_AVANCE_PREGIRO 250`) may require empirical tuning on the physical track depending on surface friction and battery voltage.
- File `src/hardware/movimiento/PID.cpp` contains a non-UTF8 byte (line 8 comment with `están`), which caused encoding warnings on some tools; this should be sanitized to standard UTF-8 / ASCII.
- Exploration was strictly read-only as per instructions; no code modifications were applied.

---

## 4. Conclusion

1. **Architecture**: `rightHand.cpp` must be rewritten as a complete non-blocking state machine with sub-states: `AVANZANDO`, `PREPARANDOME_PARA_GIRAR_DER`, `GIRANDO_DER`, `PREPARANDOME_PARA_GIRAR_IZQ`, `GIRANDO_IZQ`, `GIRANDO_180`, `POST_GIRO_AVANZAR`, and `FIN`.
2. **Debounce**: Implement an independent counter per transition event with explicit reset to 0 upon condition disruption, comparing against `DEBOUNCE_LECTURAS` (defined in `config.h`).
3. **Dead-End**: Evaluate `(distanciaDer <= UMBRAL_PARED_ESTADO_NORMAL && distanciaCent <= UMBRAL_PARED_FRENTE && distanciaIzq <= UMBRAL_PARED_ESTADO_NORMAL)` simultaneously with debounce, triggering `GIRANDO_180`.
4. **Centering**: Use encoder counts in `PREPARANDOME_PARA_GIRAR_DER` and `PREPARANDOME_PARA_GIRAR_IZQ` to advance the robot to the intersection center before rotating.
5. **PID Guard**: Implement mathematical guard clause ignoring distances $> \text{UMBRAL\_PARED\_VALIDA\_PID}$, fix steering sign inversion, and eliminate all magic numbers.

---

## 5. Verification Method

To verify these findings and subsequent implementation:
1. **Inspection**:
   - Inspect `src/maquinaEstados/rightHand.cpp` lines 8–50 and compare with `rightHand.h`.
   - Inspect `src/hardware/movimiento/PID.cpp` lines 7–33 for error calculation and magic numbers.
   - Inspect `src/config.h` lines 13–42 for existing pulse and threshold constants.
2. **Compilation**:
   Run PlatformIO build:
   `pio run -d c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\bahiaBlanca`
   Expected: Clean compilation with no syntax errors.
3. **Unit Logic Invalidation Condition**:
   - If debounce counter does not reset on condition break, verification fails.
   - If dead-end does not test all 3 walls simultaneously, verification fails.
   - If PID error is not 0 when both walls are $> \text{UMBRAL}$, verification fails.
