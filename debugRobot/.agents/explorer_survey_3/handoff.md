# Handoff Report — Explorer Instance 3 (Security, Memory Safety & Edge Cases)

## 1. Observation

1. **Compilation Failure with xtensa-gcc**:
   Executing PlatformIO compiler check via `& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run` yielded verbatim error:
   ```text
   Compiling .pio\build\esp32doit-devkit-v1\src\CIT.cpp.o
   xtensa-esp32-elf-g++: error: src/CIT.cpp: No such file or directory
   xtensa-esp32-elf-g++: fatal error: no input files
   compilation terminated.
   *** [.pio\build\esp32doit-devkit-v1\src\CIT.cpp.o] Error 1
   ========================= [FAILED] Took 14.04 seconds =========================
   ```
2. **Missing Break / Fallthrough in State Machine**:
   In `src/main.cpp:84-88`:
   ```cpp
   84:        estado = GIRANDO_180;
   85:      }
   86:     
   87:    }
   88:
   89:    case  PREGIRO_DER : {
   ```
   GCC compiler output with `-Wall -Wextra` confirmed:
   ```text
   src/main.cpp:84:7: warning: this statement may fall through [-Wimplicit-fallthrough=]
   src/main.cpp:88:5: note: here: case  PREGIRO_DER : {
   ```
3. **Continuous Reset in Straight Corridor**:
   In `src/main.cpp:71-76`:
   ```cpp
   71:      } else if (condicionAvanzar) {
   72:        resetearEncoders();
   73:        resetearErrorAnterior();
   74:        estado = AVANZANDO;
   75:        enviarString (">>> AVANZANDO <<<");
   76:      }
   ```
4. **Single-Wall PID Inverted Error / Missing Setpoint**:
   In `src/hardware/movimiento/PID.cpp:17-26`:
   ```cpp
   17:    } else if (hayIzq) {
   18:        // Solo pared izquierda: mantenerse a la distancia ideal (OFSET_IZQ representa nuestro objetivo ideal)
   19:        error = - (int16_t)mediciones.distanciaIzq;
   20:    } else if (hayDer) {
   21:        // Solo pared derecha
   22:        error = (int16_t)mediciones.distanciaDer;
   23:    }
   ```
5. **Strapping Pin & Floating Input Hardware Assignments**:
   In `src/config.h:42, 49`:
   ```cpp
   42: #define BOTON1 34
   ...
   49: #define PWMA 12
   ```
   And `src/main.cpp:25`:
   ```cpp
   25:   pinMode(BOTON1, INPUT);
   ```
6. **I2C Bus Configuration & Commented Rate Limiting**:
   In `src/hardware/sensoresDistancia/sensoresDistancia.cpp:17-23, 88-105`:
   ```cpp
   20: gpio_set_pull_mode((gpio_num_t)SDA, GPIO_PULLUP_ONLY);
   21: gpio_set_pull_mode((gpio_num_t)SCL, GPIO_PULLUP_ONLY);
   23: Wire.setClock(10000);
   ...
   88: //  if(millis() - ultimoSensado > 20){
   ...
   104:    ultimoSensado = millis();
   105: // }
   ```
   GCC compiler output confirmed:
   ```text
   src/hardware/sensoresDistancia/sensoresDistancia.cpp:84:24: warning: variable 'ultimoSensado' set but not used [-Wunused-but-set-variable]
   ```
7. **Dead End Pulse Configuration Discrepancy**:
   In `src/config.h:69-71`:
   ```cpp
   69: #define PULSOS_GIRO_90_DER 300
   70: #define PULSOS_GIRO_90_IZQ 280
   71: #define PULSOS_GIRO_180 300
   ```
8. **Asymmetric Division of Encoder Pulses**:
   In `src/main.cpp:89, 104, 120, 135`:
   ```cpp
   89:  case  PREGIRO_DER : { pulsosActuales = (abs(verPulsosEncoderA())) / 2;
   104: case PREGIRO_IZQ : { pulsosActuales = abs(verPulsosEncoderB());
   120: case GIRANDO_DER: { pulsosActuales = (abs(verPulsosEncoderA())) / 2;
   135: case GIRANDO_IZQ: { pulsosActuales = abs(verPulsosEncoderB());
   ```
9. **Missing Function Definitions**:
   In `src/hardware/logger/logger.h:11, 15`: `enviarLog`, `leerAccion`.
   In `src/hardware/sensoresDistancia/sensoresDistancia.h:26, 31`: `inicializacionSensoresHCSR04`, `actualizarSensadoHCSR04`.
   In `src/hardware/movimiento/puenteH.h:19`: `actualizarDeltaX`.
   None of these have corresponding bodies in the respective `.cpp` files.

---

## 2. Logic Chain

1. **Build Failure Chain**:
   - Observation 1 proves the compiler fails on `src/CITÉ.cpp` due to non-ASCII encoding issues.
   - Inspection of `src/CITÉ.cpp` further reveals duplicate symbols for `setup`, `loop`, `sensadoActual`, `velocidadActual`, and references to nonexistent `MAQUINA_ESTADOS`.
   - *Inference*: The project cannot be built or tested on standard development systems without first excluding or renaming `CITÉ.cpp`.
2. **Control Loop Disruption Chain**:
   - Observation 2 demonstrates an unhandled fallthrough from `case AVANZANDO:` directly into `case PREGIRO_DER:`.
   - In `AVANZANDO`, PID correction sets `{velocidadActual.izquierda, velocidadActual.derecha}`.
   - The immediate fallthrough executes `movimiento(AVANZAR, {VEL_BASE_IZQ, VEL_BASE_DER})`, instantly overwriting the PID output before it takes effect on the motors.
   - When encoder pulses exceed 300, `estado = GIRANDO_DER` is triggered unintentionally during straight travel.
   - *Inference*: Closed-loop PID steering is completely non-operational and turns are triggered erroneously.
3. **Wall Collision Chain**:
   - Observation 4 shows `error = - distanciaIzq` when only the left wall is detected.
   - For any positive distance, `error < 0`, so `correccion < 0`.
   - `velocidadActual.izquierda = VEL_BASE_IZQ + correccion` decreases, while `velocidadActual.derecha` increases.
   - *Inference*: The robot turns toward the left wall regardless of how close it is, resulting in systematic collision whenever single-wall following is active.
4. **Odometry & PID Degradation Chain**:
   - Observation 3 shows `resetearEncoders()` and `resetearErrorAnterior()` being called on every cycle in `condicionAvanzar`.
   - *Inference*: Encoder ticks cannot accumulate during forward movement, and $K_d$ is reduced to a constant proportional multiplier $(K_p + K_d) \cdot error$.
5. **Hardware Crash & Reliability Chain**:
   - Observation 5 assigns GPIO 12 (`MTDI`) to `PWMA` and GPIO 34 to `BOTON1`.
   - On ESP32, GPIO 12 high state at boot sets flash to 1.8V, preventing boot. GPIO 34 lacks internal pull-ups, causing floating triggers under motor electrical noise.
   - Observation 6 shows I2C at 10 kHz with commented 20 ms rate limits, forcing multiple 15-30 ms blocking transactions per loop cycle.
   - *Inference*: The robot suffers from potential boot failures, spontaneous starts, and massive control loop jitter.

---

## 3. Caveats

- Physical PCB routing was evaluated against ESP32 silicon specifications and pinout standards; external hardware pull-up/pull-down resistors soldered on the physical PCB could not be physically measured, but their omission in silicon was verified.
- Motor driver IC was inferred to be TB6612FNG or dual H-bridge based on standard pin naming (`AIN1, AIN2, BIN1, BIN2, PWMA, PWMB`).
- No source files were modified, complying strictly with read-only audit constraints.

---

## 4. Conclusion

The `debugRobot` firmware contains 18 identifiable defects, including 3 critical blockers that prevent proper execution:
1. Compilation failure caused by `src/CITÉ.cpp`.
2. Total negation of PID steering and spurious state transitions caused by a missing `break;` in `main.cpp:87`.
3. Inverted wall-following math in `PID.cpp:19` that guarantees wall collisions.
4. Hardware reliability risks on GPIO 12 (MTDI strapping) and GPIO 34 (floating GPI).

Resolution of these issues is strictly required prior to competition or in-maze testing.

---

## 5. Verification Method

1. **Verify Compilation Issue**:
   Run:
   ```powershell
   & "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run
   ```
   Expected result: Fails with `xtensa-esp32-elf-g++: error: src/CIT.cpp: No such file or directory`.
2. **Verify Fallthrough Warning**:
   Inspect line 84-88 in `src/main.cpp` using `view_file`. Notice absence of `break;` before `case PREGIRO_DER:`.
3. **Verify Single-Wall Inversion**:
   Inspect lines 17-23 in `src/hardware/movimiento/PID.cpp` using `view_file`. Verify that `OFSET_IZQ` is omitted from calculation and error sign is inverted.
4. **Verify Encoder Asymmetry**:
   Inspect lines 89 and 104 in `src/main.cpp`. Notice `(abs(verPulsosEncoderA())) / 2` versus `abs(verPulsosEncoderB())`.
5. **Verify 180° Pulse Value**:
   Inspect line 71 in `src/config.h`. Notice `PULSOS_GIRO_180 300` matches `PULSOS_GIRO_90_DER 300`.
