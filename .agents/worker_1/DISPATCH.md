## 2026-09-20T00:48:00Z

DO NOT CHEAT. All implementations must be genuine. DO NOT hardcode test results, create dummy/facade implementations, or circumvent the intended task. A teamwork_preview_auditor will independently verify your work. Integrity violations WILL be detected and your work WILL be rejected.

You are the Lead Embedded C++ Worker for the Micromouse Maze Solver Refactoring project.
Your identity: teamwork_preview_worker (worker_1)
Your working directory for reports/handoffs: .agents/worker_1/
Project target directory: c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\bahiaBlanca

MANDATORY INPUT SPECIFICATIONS:
You MUST read and adhere to the following specification documents before making changes:
1. ORIGINAL_REQUEST.md: c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\.agents\ORIGINAL_REQUEST.md
2. PROJECT.md: C:\Users\marti\.gemini\antigravity\brain\c4ffffd0-ef10-4b0d-9f69-3912147be295\PROJECT.md
3. Spec Miner Report: C:\Users\marti\.gemini\antigravity\brain\1d24ed35-8a99-4784-8815-f6078b5047d3\spec_report.md
4. Sensor Specialist Handoff: c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\.agents\sensor_specialist_1\handoff.md
5. Navigation Specialist Analysis: c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\.agents\teamwork_preview_explorer_1\analysis.md

WRITE OWNERSHIP:
You have exclusive write ownership over these 5 source files in `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\bahiaBlanca`:
- `src/config.h`
- `src/hardware/sensoresDistancia/sensoresDistancia.cpp`
- `src/hardware/movimiento/PID.cpp` (and `src/hardware/movimiento/PID.h` if needed)
- `src/maquinaEstados/rightHand.h`
- `src/maquinaEstados/rightHand.cpp`
DO NOT modify any other files outside this boundary.

TASK REQUIREMENTS:

1. R1: Filtrado de Hardware (Filtro de Mediana en `sensoresDistancia.cpp`)
   - Implement a running circular buffer Median Filter with window size `FILTRO_MEDIANA_N` (5) for each sensor (`filtroIzq`, `filtroCent`, `filtroDer`).
   - Insertion sort on a small stack array (`temp[5]`) to compute median without dynamic allocation.
   - Crucial: Only insert new raw sample when `(sensor.readReg(VL53L0X::RESULT_INTERRUPT_STATUS) & 0x07) != 0` (genuine new physical measurement).
   - Clamp raw reading to `DISTANCIA_MAX_VALIDA` (2000 mm).
   - In `inicializacionSensoresDist()`, prime/pre-fill the filter with the first valid reading across all 5 slots to prevent cold-start zero readings.
   - Apply physical offsets (`OFSET_IZQ`, `OFSET_CENT`, `OFSET_DER`) to the filtered median, clamping to 0 minimum.
   - Consumers (`rightHand`, `PID`) consume filtered values via `struct sensado` returned by `actualizarSensado()`.

2. R2: Algoritmo de Navegación y Transiciones (Debounce en `rightHand.cpp` & `rightHand.h`)
   - Rewrite `right_hand()` as a strictly non-blocking state machine returning `ESTADOS`.
   - Prevent symbol collision with `main.cpp`: declare `static SUBESTADOS subestadoActual;` (or use internal static state).
   - Debounce mechanism: Independent counters for transition conditions (`apertDer`, `apertIzq`, `paredFrente`, `callejon`).
   - MANDATORY DEBOUNCE CONTRACT: If the condition is true, increment its counter. If the condition is false, **immediately reset its counter to 0**.
   - Transition to next state only when the corresponding counter reaches `DEBOUNCE_LECTURAS` (5), then reset debounce counters.
   - No blocking `delay()` calls inside `right_hand()`.

3. R3: Detección de Callejón Sin Salida (180 Grados)
   - Simultaneously evaluate all 3 walls:
     `bool hayParedDer = (sensadoActual.distanciaDer <= UMBRAL_PARED_ESTADO_NORMAL);`
     `bool hayParedCent = (sensadoActual.distanciaCent <= UMBRAL_PARED_FRENTE);`
     `bool hayParedIzq = (sensadoActual.distanciaIzq <= UMBRAL_PARED_ESTADO_NORMAL);`
     `bool esCallejon = hayParedDer && hayParedCent && hayParedIzq;`
   - Debounce `esCallejon`. When confirmed, transition to `GIRANDO_180`.
   - Add `GIRANDO_180` to `SUBESTADOS` in `rightHand.h`.
   - In `GIRANDO_180`, command `movimiento(GIRAR_DER, {VEL_GIRO_IZQ, VEL_GIRO_DER})` until `abs(verPulsosEncoderA()) >= PULSOS_180_GRADOS` (or `TIEMPO_180_GRADOS`). Then brake, reset encoders, call `resetearErrorAnterior()`, and transition to `AVANZANDO`.

4. R4: Centrado en Intersecciones (Estados Previos al Giro)
   - Implement all missing substates in `rightHand.cpp`:
     `AVANZANDO`, `PREPARANDOME_PARA_GIRAR_DER`, `GIRANDO_DER`, `PREPARANDOME_PARA_GIRAR_IZQ`, `GIRANDO_IZQ`, `GIRANDO_180`, `POST_GIRO_AVANZAR` (or equivalent), `FIN`.
   - When right opening confirmed: transition to `PREPARANDOME_PARA_GIRAR_DER`. Advance straight (`movimiento(AVANZAR, {VEL_BASE_IZQ, VEL_BASE_DER})`) until `abs(verPulsosEncoderA()) >= PULSOS_AVANCE_PREGIRO` to align rotation axis with intersection center. Then brake, reset encoders, and transition to `GIRANDO_DER`.
   - In `GIRANDO_DER`, turn until `abs(verPulsosEncoderA()) >= PULSOS_90_GRADOS`. Then brake, reset encoders, reset PID error, and transition to `POST_GIRO_AVANZAR` or `AVANZANDO`.
   - Mirror logic for `PREPARANDOME_PARA_GIRAR_IZQ` / `GIRANDO_IZQ`.

5. R5: Protección del PID ante Ausencias de Pared (`PID.cpp` & `PID.h`)
   - Mathematical guard clause:
     `bool paredIzqValida = (mediciones.distanciaIzq > 0) && (mediciones.distanciaIzq <= UMBRAL_PARED_VALIDA_PID);`
     `bool paredDerValida = (mediciones.distanciaDer > 0) && (mediciones.distanciaDer <= UMBRAL_PARED_VALIDA_PID);`
   - If both valid: `error = (int16_t)mediciones.distanciaDer - (int16_t)mediciones.distanciaIzq;` (NOTE STEERING SIGN: positive error must steer away from right wall!).
   - If only Left valid (right gap/opening): `error = (int16_t)DISTANCIA_OBJETIVO_PARED_IZQ - (int16_t)mediciones.distanciaIzq;` (strictly ignore right sensor!).
   - If only Right valid (left gap/opening): `error = (int16_t)mediciones.distanciaDer - (int16_t)DISTANCIA_OBJETIVO_PARED_DER;` (strictly ignore left sensor!).
   - If neither valid: `error = 0;` and reset `errorAnterior = 0;`.
   - Constrain PID correction to `[-MAX_CORRECCION_PID, MAX_CORRECCION_PID]`.
   - Ensure `void resetearErrorAnterior()` is exported in `PID.h` and implemented in `PID.cpp`.

6. Zero Magic Numbers (`config.h`):
   - All constants must be defined in `config.h`:
     - `FILTRO_MEDIANA_N` (5)
     - `DISTANCIA_MAX_VALIDA` (2000)
     - `TIMEOUT_SENSOR_MS` (500)
     - `DELAY_BOOT_SENSOR_MS` (10)
     - `DEBOUNCE_LECTURAS` (5)
     - `UMBRAL_PARED_VALIDA_PID` (110)
     - `DISTANCIA_OBJETIVO_PARED_IZQ` (40)
     - `DISTANCIA_OBJETIVO_PARED_DER` (47)
     - `MAX_CORRECCION_PID` (50)
     - `PULSOS_180_GRADOS` (220)
     - `TIEMPO_180_GRADOS` (700)
     - `PULSOS_AVANZAR_POST_GIRO` (100)
   - Fix Pin Collision in `config.h`:
     In `config.h:59`, `ENC_B_2` is defined as 18, and in line 64 `xshutPinIzq` is also 18! Reassign `ENC_B_2` to a free GPIO (e.g. 19, or check unused pins in config.h like 4 or 23, or reassign `xshutPinIzq` to 4/23) so there is no GPIO conflict.

7. COMPILATION VERIFICATION:
   - Run `pio run` in `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\bahiaBlanca`.
   - The build MUST succeed with exit code 0, 0 compilation errors, and 0 redefinition errors.
   - If there are any errors or warnings, resolve them until `pio run` is 100% clean.

DELIVERABLE:
- Write handoff report with build logs and verification results to `.agents/worker_1/handoff.md`.
- Report completion and summary back to the parent orchestrator via `send_message`.
