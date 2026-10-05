# Handoff Report — Explorer Survey 2 (Algorithmic, Logical, and Control Flow Analysis)

## 1. Observation

Direct observations from code review and environment commands:

1. **Compilation failure from `src/CITÉ.cpp`:**
   Running command `& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run` produced:
   ```
   Compiling .pio\build\esp32doit-devkit-v1\src\CIT.cpp.o
   xtensa-esp32-elf-g++: error: src/CIT.cpp: No such file or directory
   xtensa-esp32-elf-g++: fatal error: no input files
   compilation terminated.
   *** [.pio\build\esp32doit-devkit-v1\src\CIT.cpp.o] Error 1
   ```
   Inspection of `src/CITÉ.cpp` confirms duplicate `setup()` (lines 23-30), `loop()` (lines 36-147), duplicate global variables (`sensadoActual`, `velocidadActual`, `estado`, `pulsosActuales`), and reference to `MAQUINA_ESTADOS` which is commented out in `src/main.h` (lines 3-10).

2. **Missing `break;` in `case AVANZANDO:` (`src/main.cpp:54-87`):**
   ```cpp
   54:     case AVANZANDO: {
   ...
   85:      
   86:     }
   87: 
   88:     case  PREGIRO_DER : {
   ```
   No `break;` statement exists between lines 85 and 88. Control unconditionally falls through into `case PREGIRO_DER:` on every iteration of `loop()` when `estado == AVANZANDO`.

3. **Repetitive `resetearEncoders()` in `AVANZANDO` (`src/main.cpp:71-75`):**
   ```cpp
   71:       } else if (condicionAvanzar) {
   72:         resetearEncoders();
   73:         resetearErrorAnterior();
   74:         estado = AVANZANDO;
   75:         enviarString (">>> AVANZANDO <<<");
   76:       }
   ```
   Encoders are reset to 0 every loop iteration when moving forward, preventing distance measurement and odometry.

4. **180-Degree turn pulse misconfiguration (`src/config.h:69-71`):**
   ```cpp
   69: #define PULSOS_GIRO_90_DER 300
   70: #define PULSOS_GIRO_90_IZQ 280
   71: #define PULSOS_GIRO_180 300
   ```
   `PULSOS_GIRO_180` is defined as 300, identical to `PULSOS_GIRO_90_DER` (300).

5. **Asymmetric encoder arithmetic (`src/main.cpp:89, 120` vs `104, 135`):**
   - Line 89: `pulsosActuales = (abs(verPulsosEncoderA())) / 2;`
   - Line 104: `pulsosActuales = abs(verPulsosEncoderB());`
   - Line 120: `pulsosActuales = (abs(verPulsosEncoderA())) / 2;`
   - Line 135: `pulsosActuales = abs(verPulsosEncoderB());`
   Encoder A count is halved for right turns and right pre-turns, requiring 600 pulses for 300 target pulses, while left turns require only 280 pulses.

6. **PID centering offset bias (`src/hardware/sensoresDistancia/sensoresDistancia.cpp:92, 102` and `src/hardware/movimiento/PID.cpp:16`):**
   In `sensoresDistancia.cpp`:
   `lecturaAct.distanciaIzq = rawIzq - OFSET_IZQ;` (where `OFSET_IZQ = 40`)
   `lecturaAct.distanciaDer = rawDer - OFSET_DER;` (where `OFSET_DER = 47`)
   In `PID.cpp`:
   `error = (int16_t)mediciones.distanciaDer - (int16_t)mediciones.distanciaIzq;`
   When centered (`rawDer == rawIzq`), `error = (raw - 47) - (raw - 40) = -7 mm`.

7. **Sensor reading initial zero state (`src/hardware/sensoresDistancia/sensoresDistancia.cpp:83`):**
   `static sensado lecturaAct = {0,0,0};`
   Before the VL53L0X sensors complete their first reading, all distances return 0, satisfying `condicionGiro180` (`0 < 130 && 0 < 130 && 0 < 130`) in `main.cpp:66` upon start.

8. **Inverted differential drive rotation commands (`src/hardware/movimiento/puenteH.cpp:39-56`):**
   In `GIRAR_DER`: `AIN1=LOW, AIN2=HIGH` (reverse left motor), `BIN1=HIGH, BIN2=LOW` (forward right motor). Geometrically rotates counter-clockwise (Left).
   In `GIRAR_IZQ`: `AIN1=HIGH, AIN2=LOW` (forward left motor), `BIN1=LOW, BIN2=HIGH` (reverse right motor). Geometrically rotates clockwise (Right).

---

## 2. Logic Chain

1. **Build Failure Chain:**
   - From Observation 1: PlatformIO attempts to build every `.cpp` file in `src/`.
   - GCC on Windows fails to locate `src/CITÉ.cpp` due to non-ASCII encoding in the filename.
   - Even if renamed, multiple symbol collisions (`setup`, `loop`, globals) and undefined type `MAQUINA_ESTADOS` prevent linking.
   - **Conclusion:** The project is currently unbuildable until `src/CITÉ.cpp` is removed or excluded from the build.

2. **Control Flow Breakdown Chain:**
   - From Observation 2: In `switch (estado)`, `case AVANZANDO:` lacks `break;`.
   - In C++, falling through executes `case PREGIRO_DER:`.
   - In `PREGIRO_DER:`, line 92 issues `movimiento(AVANZAR, {VEL_BASE_IZQ, VEL_BASE_DER});`.
   - This executes immediately after PID speed adjustments on lines 58-60, overwriting the motor speeds and rendering PID steering ineffective.
   - **Conclusion:** Steering control is disabled by unconditional fallthrough in `main.cpp`.

3. **Odometry & Deadlock Chain:**
   - From Observation 3: Encoders are reset to 0 on every tick while advancing.
   - Therefore, `pulsosActuales` never accumulates distance.
   - From Observation 4: A 180° dead-end turn has the same threshold (300 pulses) as a 90° turn.
   - From Observation 5: Right turns require 600 pulses due to `/ 2` scaling, while left turns require 280.
   - From Observation 7: The robot starts with `{0,0,0}` readings, immediately entering `GIRANDO_180`.
   - In `GIRANDO_180`, it turns only 90° due to the 300 pulse limit, and faces directly into a side wall.
   - **Conclusion:** The robot cannot complete any planned navigation maneuver without colliding or thrashing.

---

## 3. Caveats

- Hardware physical wiring (whether DC motor polarities or sensor channels were swapped on the breadboard/PCB to compensate for code inversions) cannot be physically tested in a read-only environment; assessment is based strictly on standard differential drive kinematics and code definitions.
- Non-compiling scratch files (`.txt` files and `src/pruebaEncoders`) were evaluated for developer intent and historical context, but are not part of the PlatformIO build chain.

---

## 4. Conclusion

The codebase contains 29 identified bugs spanning 6 categories:
- **Build-breakers:** `src/CITÉ.cpp` must be deleted or moved.
- **State machine failures:** `main.cpp` requires `break;` in `case AVANZANDO:`, removal of repetitive encoder resets, and rotation timeouts.
- **Kinematic & Mathematical fixes:** Correct `PULSOS_GIRO_180` to ~600, unify encoder scaling, correct `GIRAR_DER`/`GIRAR_IZQ` motor polarities, and isolate sensor offsets from the raw sensor driver.
- A full analysis with exact line-by-line remedies is documented in `c:\Users\devandroid\Documents\GitHub\Laberinto\debugRobot\.agents\explorer_survey_2\logic_report.md`.

---

## 5. Verification Method

1. **Verify Build Error:**
   Execute in project root:
   ```powershell
   & "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e esp32doit-devkit-v1
   ```
   Expected result: Fails at `src/CITÉ.cpp`.

2. **Verify Code Defects:**
   - Inspect `c:\Users\devandroid\Documents\GitHub\Laberinto\debugRobot\src\main.cpp` lines 84-88 to verify missing `break;`.
   - Inspect `c:\Users\devandroid\Documents\GitHub\Laberinto\debugRobot\src\main.cpp` lines 71-75 to verify repetitive `resetearEncoders()`.
   - Inspect `c:\Users\devandroid\Documents\GitHub\Laberinto\debugRobot\src\config.h` line 71 to verify `PULSOS_GIRO_180 300`.
   - Inspect `c:\Users\devandroid\Documents\GitHub\Laberinto\debugRobot\src\hardware\movimiento\puenteH.cpp` lines 39-56 to verify motor direction commands.

3. **Invalidation Conditions:**
   This analysis would be invalidated only if `src/CITÉ.cpp` was explicitly intended to replace `main.cpp` via custom build scripts not present in the repository, or if an external microcontroller was handling odometry.
