# Handoff Report — Explorer Survey Instance 1

**Agent**: `teamwork_preview_explorer` (Instance 1: Survey & Architectural / Static Analysis)  
**Report Path**: `c:\Users\devandroid\Documents\GitHub\Laberinto\debugRobot\.agents\explorer_survey_1\handoff.md`  
**Full Survey**: `c:\Users\devandroid\Documents\GitHub\Laberinto\debugRobot\.agents\explorer_survey_1\survey_report.md`  
**Timestamp**: 2026-10-05T11:42:00Z  

---

## 1. Observation

1. **Compilation Attempt & Verbatim Tool Output**:
   - Command: `& "C:\Users\devandroid\.platformio\penv\Scripts\platformio.exe" run`
   - Output:
     ```
     Compiling .pio\build\esp32doit-devkit-v1\src\CIT.cpp.o
     xtensa-esp32-elf-g++: error: src/CIT.cpp: No such file or directory
     xtensa-esp32-elf-g++: fatal error: no input files
     compilation terminated.
     *** [.pio\build\esp32doit-devkit-v1\src\CIT.cpp.o] Error 1
     ========================= [FAILED] Took 12.19 seconds =========================
     ```
2. **Source Code Inspections**:
   - `src/main.cpp`:
     - Lines 54–88: `case AVANZANDO:` block terminates at line 86 without a `break;` statement before `case PREGIRO_DER: {` at line 88.
     - Lines 71–76: Inside `case AVANZANDO:`, if `condicionAvanzar` is true, calls `resetearEncoders()` and `enviarString(">>> AVANZANDO <<<")` unconditionally every loop cycle.
     - Lines 89 and 120: `pulsosActuales = (abs(verPulsosEncoderA())) / 2;` divides encoder pulses by 2 for right turn actions, whereas lines 104 and 135 (`PREGIRO_IZQ` / `GIRANDO_IZQ`) use `pulsosActuales = abs(verPulsosEncoderB());` without dividing by 2.
     - Lines 63–66: Strict inequalities (`>` and `<`) for `UMBRAL_PARED_ESTADO_NORMAL` leave distance `130` unhandled.
   - `src/CITÉ.cpp`:
     - Filename contains accented UTF-8 character `É`.
     - Lines 14–37: Defines `sensadoActual`, `velocidadActual`, `pulsosActuales`, `setup()`, and `loop()`, colliding with identical symbols in `src/main.cpp`.
     - Line 16: References `MAQUINA_ESTADOS`, which is commented out in `src/main.h` lines 3–10.
   - `src/hardware/movimiento/PID.cpp`:
     - Lines 17–23:
       ```cpp
       } else if (hayIzq) {
           // Solo pared izquierda: mantenerse a la distancia ideal (OFSET_IZQ representa nuestro objetivo ideal)
           error = - (int16_t)mediciones.distanciaIzq;
       } else if (hayDer) {
           // Solo pared derecha
           error = (int16_t)mediciones.distanciaDer;
       }
       ```
       Omits subtraction of target distance setpoint.
   - `src/hardware/movimiento/puenteH.cpp`:
     - Lines 39–56: `GIRAR_DER` sets Motor A (`AIN1=LOW, AIN2=HIGH` reverse) and Motor B (`BIN1=HIGH, BIN2=LOW` forward), rotating the robot counter-clockwise (Left) instead of Right. `GIRAR_IZQ` sets Motor A forward and Motor B reverse, rotating clockwise (Right) instead of Left.
   - `src/hardware/sensoresDistancia/sensoresDistancia.cpp`:
     - Lines 43–46, 57–60, 71–74: Calls `while (true) delay(1000);` if any sensor fails `.init()`.
     - Line 23: Sets I2C clock to `10000` (10 kHz).
     - Lines 90–103: Distance underflow produces negative values; sensor timeout (65535) is clamped to 2000, masking timeouts as open pathways.
   - `src/hardware/logger/logger.cpp`:
     - Lines 20–22: `cambioDeCelda()` checks `SerialBT.available() > 0` but never reads the byte.
   - `src/config.h`:
     - Line 49: `#define PWMA 12` assigns motor PWM to strapping pin MTDI (GPIO 12).
     - Line 42: `#define BOTON1 34` assigns button to input-only GPIO 34 without internal pull-ups.
   - Interface mismatches:
     - `logger.h:11, 15`: `enviarLog`, `leerAccion` declared but not defined.
     - `sensoresDistancia.h:26, 31`: `inicializacionSensoresHCSR04`, `actualizarSensadoHCSR04` declared but not defined.
     - `puenteH.h:19`: `actualizarDeltaX` declared but not defined.
   - Directory clutter:
     - 8 scratch/test files in `src/` (`bluetooth.txt`, `encodersSerial.txt`, `movimiento.txt`, `pruebaEncoders`, `pruebaMotoresAislados.txt`, `sensoresSDist.txt`, `test.txt`, `testearHardware.txt`).

---

## 2. Logic Chain

1. **Build Failure Chain**:
   - `xtensa-esp32-elf-g++` does not resolve non-ASCII Windows paths (`src/CITÉ.cpp`) → The build fails immediately with exit code 1.
   - If the file is renamed to ASCII, both `src/CITÉ.cpp` and `src/main.cpp` are compiled because `platformio.ini` has no `build_src_filter` → GCC attempts to compile both → `CITÉ.cpp` fails on undefined `MAQUINA_ESTADOS` → If defined, the linker fails due to duplicate definitions of `setup`, `loop`, and global telemetry structs.

2. **Control Loop Corruption Chain**:
   - `main.cpp` enters `case AVANZANDO:` → Evaluates sensor distances and PID → Executes motor movement → Reaches line 86 without a `break;` statement → Unconditionally enters `case PREGIRO_DER:` → Reads `verPulsosEncoderA() / 2` and prematurely switches motion states or triggers turns.
   - In `AVANZANDO`, if distance indicates straight movement, `resetearEncoders()` is called every loop cycle → Encoder counts never accumulate towards cell traversal thresholds → Robot loses odometry awareness.

3. **Wall-Following Oscillation Chain**:
   - In `PID.cpp`, `calcularCorreccion` evaluates `hayIzq` (only left wall present) → `error = -mediciones.distanciaIzq` → For a 60 mm distance, `error = -60` → Proportional term `KP * (-60) = -30` → Saturated correction `-25` applied to motors → Robot swerves violently away from wall despite being at normal distance.

4. **Odometry & Steering Inversion Chain**:
   - Right turn pulses are divided by 2 in `PREGIRO_DER` and `GIRANDO_DER` while left turns are not → Right turns require twice as many physical pulses as configured.
   - In `puenteH.cpp`, `GIRAR_DER` reverses left wheel and drives right wheel forward → Differential robot turns Left instead of Right.

5. **System Robustness & Boot Risks**:
   - GPIO 12 pulled high during boot alters ESP32 VDD_SDIO to 1.8V → Flash read fails, boot loops.
   - If any ToF sensor disconnects → System hangs permanently in `while (true) delay(1000)`.

---

## 3. Caveats

- **Physical Hardware Testing**: No physical ESP32 or robot hardware was flashed or connected; all findings are based on static code inspection, dependency analysis, and PlatformIO compiler toolchain diagnostic execution.
- **Circuit Schematic Assumptions**: Pin strapping vulnerability for GPIO 12 and floating input on GPIO 34 assume the external PCB does not have dedicated hardware pull-down/pull-up resistors mitigating the behavior.
- **Historical Context**: The test text files (`*.txt`) indicate multiple prior iterations of motor tests; some logic inversions may have been temporary workarounds for crossed motor wiring.

---

## 4. Conclusion

The `debugRobot` firmware cannot be built or run in its current state. The repository requires:
1. **Build Fixes**: Exclusion or removal of `src/CITÉ.cpp` and configuration of `build_src_filter` in `platformio.ini`.
2. **Critical Logic Fixes**: Adding `break;` to `case AVANZANDO:` in `src/main.cpp`, fixing the PID error setpoint calculation in `PID.cpp`, removing the continuous encoder resets in `AVANZANDO`, and fixing the `/ 2` turn pulse calculation asymmetry.
3. **Hardware Driver Fixes**: Correcting the motor polarity in `puenteH.cpp`, eliminating infinite blocking loops in `sensoresDistancia.cpp`, increasing I2C clock speed from 10 kHz to 100 kHz, and relocating test sketches out of `src/`.

---

## 5. Verification Method

1. **Verify Build Blocker**:
   ```powershell
   & "C:\Users\devandroid\.platformio\penv\Scripts\platformio.exe" run
   ```
   *Expected Result*: Fails on `src/CIT.cpp`.
2. **Verify Code Locations**:
   - Inspect `src/main.cpp` line 86 to verify missing `break;`.
   - Inspect `src/hardware/movimiento/PID.cpp` lines 17–23 to verify raw distance assigned directly to `error`.
   - Inspect `src/hardware/movimiento/puenteH.cpp` lines 39–56 to verify motor direction polarity.
   - Inspect `src/hardware/sensoresDistancia/sensoresDistancia.cpp` lines 45, 59, 73 to verify `while (true) delay(1000);`.
3. **Invalidation Conditions**:
   - If `src/CITÉ.cpp` is removed and compilation succeeds, B1/B2 are verified.
   - If `break;` is added to `case AVANZANDO:`, fallthrough behavior is eliminated.
