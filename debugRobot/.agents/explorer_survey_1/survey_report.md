# Comprehensive Survey, Architecture & Static Analysis Report

**Project**: debugRobot (Micromouse / Maze-solving robot)  
**Evaluator**: Explorer Survey Instance 1 (`teamwork_preview_explorer`)  
**Date**: 2026-10-05  
**Working Mode**: 100% Read-Only Inspection  

---

## 1. Executive Summary

This report delivers an exhaustive static and architectural audit of the `debugRobot` firmware codebase located at `c:\Users\devandroid\Documents\GitHub\Laberinto\debugRobot`. 

### Key Findings
1. **Fatal Compilation Blockers**:
   - The project currently **fails to compile**. `src/CITÉ.cpp` contains a non-ASCII accented character in its filename (`É`), which causes `xtensa-esp32-elf-g++` under Windows to terminate with a fatal error: `error: src/CIT.cpp: No such file or directory`.
   - Even if the filename encoding was resolved, `src/CITÉ.cpp` defines duplicate global symbols (`setup()`, `loop()`, `sensadoActual`, `velocidadActual`, `pulsosActuales`) that collide with `src/main.cpp`, alongside an undeclared type `MAQUINA_ESTADOS`.
   - `platformio.ini` lacks a `build_src_filter` to exclude duplicate or scratch files from the build pipeline.

2. **Critical State Machine & Control Flaws**:
   - In `src/main.cpp` (`loop()`), `case AVANZANDO:` is **missing a `break;` statement**. Every execution of `AVANZANDO` falls through immediately into `case PREGIRO_DER:`, corrupting the robot's motion control logic.
   - In `src/hardware/movimiento/PID.cpp`, the single-wall PID error calculation omits the setpoint target entirely (`error = -mediciones.distanciaIzq` / `error = mediciones.distanciaDer`), generating massive non-zero errors even when the robot is perfectly centered, causing severe oscillation and steering into walls.
   - In `src/main.cpp`, condition branches repeatedly reset encoders and spray debug strings over Bluetooth (`enviarString`) at high loop frequencies, starving MCU cycles and overflowing Bluetooth buffers.
   - Pulse counting for turns is heavily asymmetric: `PREGIRO_DER` and `GIRANDO_DER` arbitrarily divide encoder pulses by 2 (`pulsosActuales = abs(verPulsosEncoderA()) / 2`), doubling the required turn angle for right turns relative to left turns.

3. **Hardware & Electrical Risks**:
   - GPIO 12 (`PWMA`) is an ESP32 bootstrapping/strapping pin (MTDI). If pulled HIGH during power-up by motor driver circuitry, the ESP32 selects 1.8V flash voltage, resulting in a flash brownout / boot loop.
   - GPIO 34 (`BOTON1`) is an input-only pin without internal pull-up/pull-down resistors. If no external hardware resistor is installed, the input floats, causing erratic false triggers or hanging `while(digitalRead(BOTON1) == LOW)`.
   - Sensor initialization in `sensoresDistancia.cpp` uses hard blocking loops (`while (true) delay(1000);`) if any VL53L0X sensor fails to initialize, permanently freezing the robot at boot without error recovery or watchdog tripping.
   - The I2C clock is configured to an excessively slow 10 kHz (`Wire.setClock(10000)`), introducing high bus latency that stalls loop iterations during continuous sensor polling.

---

## 2. Project Inventory & Technology Stack

### 2.1 Technology Stack & Frameworks
- **Target Microcontroller**: Espressif ESP32 (DOIT ESP32 DevKit V1, 240 MHz Dual Core, 320 KB RAM, 4 MB Flash)
- **Framework**: Arduino Core for ESP32 (`framework-arduinoespressif32 @ 3.20017.241212`)
- **Toolchain**: PlatformIO with `toolchain-xtensa-esp32 @ 8.4.0+2021r2-patch5`
- **Build System**: PlatformIO Core (`platformio.ini`)
- **Key Libraries**:
  - `VL53L0X @ 1.3.1` (Pololu Time-of-Flight distance sensor driver)
  - `madhephaestus/ESP32Encoder @ ^0.11.7` (ESP32 hardware PCNT quadrature encoder counter)
  - `BluetoothSerial` (ESP32 Classical Bluetooth SPP library)
  - `Wire` (ESP32 I2C driver)

### 2.2 Complete File Inventory

| Relative Path | Type | Language / Format | Role & Purpose |
|---|---|---|---|
| `platformio.ini` | Config | INI | PlatformIO project configuration & dependencies |
| `.gitignore` | Config | Text | Git version control exclusions |
| `.vscode/c_cpp_properties.json` | Config | JSON | VS Code Intellisense include paths and compiler definitions |
| `.vscode/extensions.json` | Config | JSON | Recommended VS Code extensions (PlatformIO IDE) |
| `.vscode/launch.json` | Config | JSON | VS Code GDB debug configurations |
| `include/README` | Documentation | Markdown | Standard PlatformIO header directory placeholder |
| `lib/README` | Documentation | Markdown | Standard PlatformIO private library placeholder |
| `test/README` | Documentation | Markdown | PlatformIO unit test placeholder |
| `src/main.cpp` | Source | C++ | Active robot entrypoint (`setup`, `loop`, FSM) |
| `src/main.h` | Header | C++ | State machine enum declarations |
| `src/config.h` | Header | C++ | Global constants, pin maps, PID gains, and thresholds |
| `src/CITÉ.cpp` | Source | C++ | Stray/Legacy main file with non-ASCII filename; duplicate symbols |
| `src/hardware/encoders/encoders.h` | Header | C++ | Encoder interface function declarations |
| `src/hardware/encoders/encoders.cpp` | Source | C++ | Quadrature encoder initialization and readout using ESP32Encoder |
| `src/hardware/logger/logger.h` | Header | C++ | Bluetooth logger interface declarations |
| `src/hardware/logger/logger.cpp` | Source | C++ | Bluetooth serial logging implementation |
| `src/hardware/movimiento/PID.h` | Header | C++ | Wall-following PID controller interface declarations |
| `src/hardware/movimiento/PID.cpp` | Source | C++ | PID error calculation and output computation |
| `src/hardware/movimiento/puenteH.h` | Header | C++ | H-Bridge motion primitives and enum declarations |
| `src/hardware/movimiento/puenteH.cpp` | Source | C++ | Motor PWM and direction control via ESP32 LEDC |
| `src/hardware/sensoresDistancia/sensoresDistancia.h` | Header | C++ | ToF sensor interface declarations and `sensado` struct |
| `src/hardware/sensoresDistancia/sensoresDistancia.cpp` | Source | C++ | 3x VL53L0X XSHUT initialization and distance reading |
| `src/bluetooth.txt` | Scratch / Test | C++ (txt) | Standalone Bluetooth remote control test sketch |
| `src/encodersSerial.txt` | Scratch / Test | C++ (txt) | Standalone manual encoder testing sketch |
| `src/movimiento.txt` | Scratch / Test | C++ (txt) | Standalone open-loop motor movement test sketch |
| `src/pruebaEncoders` | Scratch / Test | C++ (no ext) | Standalone encoder closed-loop test sketch |
| `src/pruebaMotoresAislados.txt` | Scratch / Test | C++ (txt) | Standalone isolated motor test sketch (references missing headers) |
| `src/sensoresSDist.txt` | Scratch / Test | C++ (txt) | Standalone ToF sensor print test sketch |
| `src/test.txt` | Scratch / Test | C++ (txt) | Standalone open-loop timed drive sketch |
| `src/testearHardware.txt` | Scratch / Test | C++ (txt) | Standalone sequential hardware diagnostic sketch |

---

## 3. Architecture & Component Boundaries

### 3.1 Architectural Decomposition

The codebase is organized in a monolithic embedded architecture with modular hardware abstraction layers:

```
                          ┌────────────────────────┐
                          │      src/main.cpp      │
                          │ (Finite State Machine) │
                          └───────────┬────────────┘
                                      │
     ┌──────────────────┬─────────────┼───────────────┬──────────────────┐
     ▼                  ▼             ▼               ▼                  ▼
┌──────────────┐ ┌──────────────┐ ┌───────────────┐ ┌───────────────┐ ┌──────────────┐
│  config.h    │ │    logger    │ │  sensoresDist │ │      PID      │ │   puenteH    │
│ (Pin/Consts) │ │ (Bluetooth)  │ │ (3x VL53L0X)  │ │ (Correction)  │ │   (Motors)   │
└──────────────┘ └──────────────┘ └───────────────┘ └───────┬───────┘ └──────┬───────┘
                                                            │                │
                                                            └───────┬────────┘
                                                                    ▼
                                                            ┌───────────────┐
                                                            │   encoders    │
                                                            │ (Wheel Count) │
                                                            └───────────────┘
```

1. **Supervisory Controller (`src/main.cpp`)**:
   Runs a non-preemptive state machine in `loop()` (`LISTO`, `AVANZANDO`, `PREGIRO_DER`, `PREGIRO_IZQ`, `GIRANDO_DER`, `GIRANDO_IZQ`, `GIRANDO_180`). Handles start-button debouncing, reads distance sensors, invokes the PID controller, calculates motor velocities, and transitions states based on encoder pulse counts and distance thresholds.

2. **Distance Sensing Subsystem (`src/hardware/sensoresDistancia/`)**:
   Controls three VL53L0X Time-of-Flight sensors sharing a single hardware I2C bus (`Wire`). Manages dynamic address reallocation via `XSHUT` pins (`0x30` right, `0x31` left, `0x32` center). Returns distance struct `sensado` with offsets subtracted.

3. **Motion & Trajectory Subsystem (`src/hardware/movimiento/`)**:
   - `puenteH`: Low-level motor driver using ESP32 `ledc` PWM peripherals (channels 0 and 1) and digital GPIO direction pins (`AIN1`, `AIN2`, `BIN1`, `BIN2`).
   - `PID`: Proportional-Derivative controller providing differential steering corrections based on sensor distances.

4. **Odometry Subsystem (`src/hardware/encoders/`)**:
   Uses the ESP32 hardware Pulse Counter peripheral via `ESP32Encoder` to track wheel rotation on two quadrature encoder channels.

5. **Telemetry Subsystem (`src/hardware/logger/`)**:
   Uses `BluetoothSerial` to transmit diagnostic messages and receive configuration commands wirelessly.

---

## 4. Compilation & Build System Defects

### Bug B1: Non-ASCII Filename Breaks Build
- **Location**: `src/CITÉ.cpp`
- **Severity**: **FATAL (Build Blocker)**
- **Compiler Output**:
  ```
  xtensa-esp32-elf-g++: error: src/CIT.cpp: No such file or directory
  xtensa-esp32-elf-g++: fatal error: no input files
  compilation terminated.
  *** [.pio\build\esp32doit-devkit-v1\src\CIT.cpp.o] Error 1
  ```
- **Description**: The filename contains `É` (Unicode U+00C9). Under Windows, the Xtensa GCC toolchain fails to decode the UTF-8 multibyte character from PlatformIO build targets, terminating compilation immediately.
- **Recommended Solution**: Remove or rename `CITÉ.cpp`, or configure `build_src_filter = -<CITÉ.cpp>` in `platformio.ini`.

### Bug B2: Duplicate Global Symbols & Undeclared Enum
- **Location**: `src/CITÉ.cpp` (Lines 14-36) vs `src/main.cpp` (Lines 15-37)
- **Severity**: **FATAL (Link Blocker)**
- **Description**: Both `CITÉ.cpp` and `main.cpp` define identical global symbols: `void setup()`, `void loop()`, `sensadoActual`, `velocidadActual`, and `pulsosActuales`. Additionally, `CITÉ.cpp` line 16 declares `MAQUINA_ESTADOS estado = LISTO;`, but `MAQUINA_ESTADOS` is commented out in `src/main.h` lines 3-10. This causes multiple definition link errors and compile-time type errors.
- **Recommended Solution**: Delete `CITÉ.cpp` or move it outside of the `src/` compilation root (e.g. to an archive or test directory).

### Bug B3: Missing `build_src_filter` in `platformio.ini`
- **Location**: `platformio.ini` (Lines 11-19)
- **Severity**: **HIGH (Build Hygiene / Fragility)**
- **Description**: PlatformIO compiles all `.c` and `.cpp` files in `src/` by default. Any developer scratch file placed in `src/` inadvertently breaks the build.
- **Recommended Solution**: Add an explicit `build_src_filter`:
  ```ini
  build_src_filter = +<*> -<CIT*.cpp>
  ```

---

## 5. Comprehensive Bug, Defect & Vulnerability Inventory

### Detailed Defect Catalog

#### Defect D1: Missing `break;` in `case AVANZANDO` (Unconditional Fallthrough)
- **File**: `src/main.cpp`
- **Lines**: 54–86
- **Category**: Logic Error / Control Flow Bug
- **Severity**: **CRITICAL**
- **Description**: In `switch (estado)`, the `case AVANZANDO:` block terminates at line 86 without a `break;` statement. When the robot is in the `AVANZANDO` state, execution immediately falls through into `case PREGIRO_DER:` on every loop cycle. This immediately evaluates `pulsosActuales = (abs(verPulsosEncoderA())) / 2` and prematurely switches motor states or triggers `estado = GIRANDO_DER`.
- **Recommended Solution**: Add `break;` immediately before line 88:
  ```cpp
      } // end if-else
      break;
    } // end case AVANZANDO

    case PREGIRO_DER: {
  ```

---

#### Defect D2: Broken Single-Wall PID Error Formula
- **File**: `src/hardware/movimiento/PID.cpp`
- **Lines**: 17–23
- **Category**: Algorithm / Mathematical Defect
- **Severity**: **CRITICAL**
- **Description**: When only one wall is detected:
  ```cpp
  } else if (hayIzq) {
      // Solo pared izquierda: mantenerse a la distancia ideal (OFSET_IZQ representa nuestro objetivo ideal)
      error = - (int16_t)mediciones.distanciaIzq;
  } else if (hayDer) {
      // Solo pared derecha
      error = (int16_t)mediciones.distanciaDer;
  }
  ```
  The comment explicitly acknowledges that a setpoint distance should be maintained. However, the calculation uses the raw distance directly without subtracting any setpoint! If `distanciaIzq` is 50 mm, the error is `-50` instead of `0` (or `mediciones.distanciaIzq - DISTANCIA_OBJETIVO`). Consequently, the PID controller continuously applies maximum steering correction (-25 / +25 PWM), violently driving the robot into the wall or steering away erratically.
- **Recommended Solution**: Define a desired nominal wall following distance (e.g., `DISTANCIA_PARED_OBJETIVO = 60`) and compute:
  ```cpp
  } else if (hayIzq) {
      error = (int16_t)mediciones.distanciaIzq - DISTANCIA_PARED_OBJETIVO;
  } else if (hayDer) {
      error = DISTANCIA_PARED_OBJETIVO - (int16_t)mediciones.distanciaDer;
  }
  ```

---

#### Defect D3: Asymmetric Turn Pulse Counting (Divided by 2 for Right, 1 for Left)
- **File**: `src/main.cpp`
- **Lines**: 89, 104, 120, 135
- **Category**: Algorithmic Inconsistency
- **Severity**: **HIGH**
- **Description**: 
  - Line 89 (`PREGIRO_DER`): `pulsosActuales = (abs(verPulsosEncoderA())) / 2;`
  - Line 104 (`PREGIRO_IZQ`): `pulsosActuales = abs(verPulsosEncoderB());` (not divided by 2)
  - Line 120 (`GIRANDO_DER`): `pulsosActuales = (abs(verPulsosEncoderA())) / 2;`
  - Line 135 (`GIRANDO_IZQ`): `pulsosActuales = abs(verPulsosEncoderB());` (not divided by 2)
  - Line 150 (`GIRANDO_180`): `pulsosActuales = abs(verPulsosEncoderA());` (not divided by 2)
  Because `verPulsosEncoderA()` is divided by 2 during right turns but not left turns, a 90° right turn requires 600 pulses (since `PULSOS_GIRO_90_DER = 300`), whereas a 90° left turn completes at 280 pulses. This causes the robot to over-rotate right turns by 100%.
- **Recommended Solution**: Remove `/ 2` from lines 89 and 120, and calibrate `PULSOS_GIRO_90_DER` and `PULSOS_GIRO_90_IZQ` uniformly to actual physical rotation pulses.

---

#### Defect D4: Motor Turn Direction Inversion in H-Bridge Driver
- **File**: `src/hardware/movimiento/puenteH.cpp`
- **Lines**: 39–56
- **Category**: Hardware Driver / Kinematics Bug
- **Severity**: **HIGH**
- **Description**:
  Motor A is Channel 0 (`PWMA`, Left speed). Motor B is Channel 1 (`PWMB`, Right speed).
  In `GIRAR_DER` (lines 39-46):
  - `AIN1 = LOW, AIN2 = HIGH` -> Motor A (Left) reverses.
  - `BIN1 = HIGH, BIN2 = LOW` -> Motor B (Right) advances.
  If the left wheel reverses and the right wheel advances, a differential-drive robot spins **counter-clockwise (LEFT)**.
  In `GIRAR_IZQ` (lines 48-55):
  - `AIN1 = HIGH, AIN2 = LOW` -> Motor A (Left) advances.
  - `BIN1 = LOW, BIN2 = HIGH` -> Motor B (Right) reverses.
  If the left wheel advances and the right wheel reverses, the robot spins **clockwise (RIGHT)**.
  The directional polarities in `GIRAR_DER` and `GIRAR_IZQ` are inverted relative to differential robot kinematics.
- **Recommended Solution**: Swap polarities in `GIRAR_DER` and `GIRAR_IZQ`:
  - `GIRAR_DER`: Motor A forward (`AIN1=HIGH, AIN2=LOW`), Motor B reverse (`BIN1=LOW, BIN2=HIGH`).
  - `GIRAR_IZQ`: Motor A reverse (`AIN1=LOW, AIN2=HIGH`), Motor B forward (`BIN1=HIGH, BIN2=LOW`).

---

#### Defect D5: Constant Resetting of Encoders and Bluetooth Flooding in `AVANZANDO`
- **File**: `src/main.cpp`
- **Lines**: 71–76
- **Category**: Logic Flaw / Performance Bug
- **Severity**: **HIGH**
- **Description**:
  In `case AVANZANDO:`, if `condicionAvanzar` is met:
  ```cpp
  } else if (condicionAvanzar) {
    resetearEncoders();
    resetearErrorAnterior();
    estado = AVANZANDO;
    enviarString (">>> AVANZANDO <<<");
  }
  ```
  Since `estado` is already `AVANZANDO`, this condition executes on **every single iteration of `loop()`**.
  1. It resets the encoders continuously, destroying any accumulated distance or odometry count.
  2. It executes `SerialBT.println(">>> AVANZANDO <<<")` at loop rate (> 50-100 times/sec), overflowing the Bluetooth UART TX ring buffer and causing execution stalls.
- **Recommended Solution**: Do not reset encoders or send continuous strings while remaining in `AVANZANDO`. State transitions should only trigger when changing to a new state.

---

#### Defect D6: Unhandled Boundary Condition in Distance Thresholds
- **File**: `src/main.cpp`
- **Lines**: 63–66
- **Category**: Boundary Condition / Incomplete Logic
- **Severity**: **MEDIUM**
- **Description**:
  The four decision conditions all use strict inequality against `UMBRAL_PARED_ESTADO_NORMAL`:
  - `condicionGiroDer = sensadoActual.distanciaDer > UMBRAL_PARED_ESTADO_NORMAL;`
  - `condicionAvanzar = sensadoActual.distanciaDer < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaCent > UMBRAL_PARED_ESTADO_NORMAL;`
  - `condicionGiroIzq = sensadoActual.distanciaDer < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaCent < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaIzq > UMBRAL_PARED_ESTADO_NORMAL;`
  - `condicionGiro180 = sensadoActual.distanciaDer < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaCent < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaIzq < UMBRAL_PARED_ESTADO_NORMAL;`
  If `distanciaDer == UMBRAL_PARED_ESTADO_NORMAL` (130 mm) or `distanciaCent == 130` or `distanciaIzq == 130`, all four conditions evaluate to `false`, leaving the robot without any transition.
- **Recommended Solution**: Use `<=` or `>=` systematically (e.g. `distanciaDer >= UMBRAL_PARED_ESTADO_NORMAL`).

---

#### Defect D7: Hard Halt on Sensor Init Failure
- **File**: `src/hardware/sensoresDistancia/sensoresDistancia.cpp`
- **Lines**: 43–46, 57–60, 71–74
- **Category**: Fault Tolerance / Robustness
- **Severity**: **HIGH**
- **Description**:
  ```cpp
  if (!sensorDer.init()) {
    Serial.printf("ERROR: fallo init sensor en pin %d\n", xshutPinDer);
    while (true) delay(1000);
  }
  ```
  If any of the three VL53L0X sensors is disconnected or has an I2C glitch during startup, the ESP32 hangs permanently in `while (true) delay(1000);`. There is no visual LED alert, error code return, or fallback operation.
- **Recommended Solution**: Return a `bool` status from `inicializacionSensoresDist()` to allow `setup()` to report the failure or retry, and avoid deadlocking the MCU.

---

#### Defect D8: Extremely Slow I2C Bus Clock (10 kHz)
- **File**: `src/hardware/sensoresDistancia/sensoresDistancia.cpp`
- **Line**: 23
- **Category**: Performance / Latency
- **Severity**: **MEDIUM**
- **Description**:
  `Wire.setClock(10000);` sets the I2C clock to 10 kHz. Standard I2C is 100 kHz, and Fast Mode is 400 kHz. At 10 kHz, each single byte transfer takes nearly 1 ms. Polling 3 sensors on each loop iteration introduces ~20–40 ms of latency, severely reducing the responsiveness of the motor control loop.
- **Recommended Solution**: Set the clock to standard 100 kHz (`Wire.setClock(100000)`) or 400 kHz (`Wire.setClock(400000)`). If signal integrity required 10 kHz, add proper external I2C pull-up resistors (2.2kΩ to 4.7kΩ).

---

#### Defect D9: Negative Distance Underflow & Sensor Timeout Masking
- **File**: `src/hardware/sensoresDistancia/sensoresDistancia.cpp`
- **Lines**: 90–103
- **Category**: Data Integrity / Boundary Handling
- **Severity**: **MEDIUM**
- **Description**:
  1. `lecturaAct.distanciaIzq = rawIzq - OFSET_IZQ;`: If `rawIzq < OFSET_IZQ` (e.g. an object is 10 mm away), `distanciaIzq` becomes negative (`-30`). In `main.cpp`, `-30 < UMBRAL_PARED_ESTADO_NORMAL` is evaluated as true, but downstream arithmetic or math expecting non-negative distance can misbehave.
  2. When `sensor.readRangeContinuousMillimeters()` times out, it returns `65535`. The code checks `if (rawIzq > 2000) rawIzq = 2000;`, transforming a sensor communication failure into a valid reading of `2000 - OFSET = 1960 mm` (interpreting a failed sensor as an open corridor).
- **Recommended Solution**: Check `if (sensor.timeoutOccurred())` to handle sensor dropouts, and clamp negative results: `if (distancia < 0) distancia = 0;`.

---

#### Defect D10: Infinite True Return in `cambioDeCelda()`
- **File**: `src/hardware/logger/logger.cpp`
- **Lines**: 20–22
- **Category**: Logic Flaw
- **Severity**: **MEDIUM**
- **Description**:
  ```cpp
  bool cambioDeCelda(){
      if(SerialBT.available()>0){ return true;} else { return false;}
  }
  ```
  The function checks if a byte is available on Bluetooth, but **never reads or consumes the byte**. Once a single byte is received, `SerialBT.available() > 0` remains true for the entire uptime of the robot.
- **Recommended Solution**: Consume the byte when reading: `if (SerialBT.available() > 0) { SerialBT.read(); return true; }`.

---

#### Defect D11: Unimplemented Header Function Declarations
- **File**: `src/hardware/logger/logger.h`, `src/hardware/sensoresDistancia/sensoresDistancia.h`, `src/hardware/movimiento/puenteH.h`
- **Lines**: `logger.h`: 11, 15; `sensoresDistancia.h`: 26, 31; `puenteH.h`: 19
- **Category**: Link-Time Defect / Interface Mismatch
- **Severity**: **MEDIUM**
- **Description**:
  The following functions are declared in headers but have no implementation in their respective `.cpp` files:
  - `enviarLog(char* mensaje);` (`logger.h:11`)
  - `leerAccion();` (`logger.h:15`)
  - `inicializacionSensoresHCSR04();` (`sensoresDistancia.h:26`)
  - `actualizarSensadoHCSR04();` (`sensoresDistancia.h:31`)
  - `actualizarDeltaX();` (`puenteH.h:19`)
  Conversely, `girar90GradosBloqueante()` and `avanzarBloqueante()` are defined in `puenteH.cpp` (lines 69-80) but never declared in `puenteH.h`.
- **Recommended Solution**: Reconcile headers and source files by removing dead declarations and adding missing prototypes.

---

#### Defect D12: Strapping Pin Conflict on GPIO 12 (`PWMA`)
- **File**: `src/config.h`
- **Line**: 49
- **Category**: Hardware / Bootstrapping Conflict
- **Severity**: **HIGH**
- **Description**:
  `#define PWMA 12` assigns motor PWM channel A to GPIO 12. GPIO 12 is the `MTDI` bootstrapping pin on the ESP32. If this pin is pulled HIGH during power-on/reset, the ESP32 sets the internal flash voltage to 1.8V instead of 3.3V. This causes flash memory read failure, resulting in an immediate boot crash (`SPI_FAST_FLASH_BOOT` failure) or cyclic reboot.
- **Recommended Solution**: Reassign `PWMA` to a non-strapping general-purpose output pin (such as GPIO 13, 15 [with caution], 21, or 22).

---

#### Defect D13: Floating Input Pin on GPIO 34 (`BOTON1`)
- **File**: `src/config.h`, `src/main.cpp`
- **Lines**: `config.h:42`, `main.cpp:24`
- **Category**: Hardware / Electrical Flaw
- **Severity**: **MEDIUM**
- **Description**:
  GPIO 34 and GPIO 35 are input-only pins (GPIs) on the ESP32. They **do not possess internal pull-up or pull-down resistors**. In `main.cpp`:
  `pinMode(BOTON1, INPUT);`
  `if (digitalRead(BOTON1) == LOW)`
  If the PCB does not feature a dedicated hardware pull-up resistor on GPIO 34, the pin will float, leading to spurious button triggers or lockup in `while(digitalRead(BOTON1) == LOW) delay(10);`.
- **Recommended Solution**: Ensure an external pull-up resistor (10kΩ) is present on GPIO 34, or reassign the button to a pin with internal pull-up support (e.g. GPIO 18, 19, 23) and use `pinMode(pin, INPUT_PULLUP)`.

---

#### Defect D14: Clutter of Test Scripts and Stray Files in `src/`
- **File**: `src/bluetooth.txt`, `src/encodersSerial.txt`, `src/movimiento.txt`, `src/pruebaEncoders`, `src/pruebaMotoresAislados.txt`, `src/sensoresSDist.txt`, `src/test.txt`, `src/testearHardware.txt`
- **Category**: Project Hygiene / Architectural Flaw
- **Severity**: **LOW**
- **Description**:
  Eight different test files containing duplicated `setup()`/`loop()` implementations and obsolete code are stored directly in `src/`. `src/pruebaEncoders` lacks an extension. `pruebaMotoresAislados.txt` includes headers that don't exist in the project (`hardware/sensorPiso/sensorPiso.h`, `memoria/funcionesMapeo.h`).
- **Recommended Solution**: Move all test and diagnostic scripts to a dedicated `test/` or `tools/` folder.

---

## 6. Comprehensive Bug Summary Table

| ID | File | Line(s) | Severity | Description | Fix Summary |
|---|---|---|---|---|---|
| **B1** | `src/CITÉ.cpp` | Filename | **FATAL** | Non-ASCII character `É` crashes GCC on Windows | Remove/rename `CITÉ.cpp` or exclude in `platformio.ini` |
| **B2** | `src/CITÉ.cpp` | 14–36 | **FATAL** | Duplicate symbols (`setup`, `loop`, globals) and undeclared `MAQUINA_ESTADOS` | Remove duplicate file from build |
| **B3** | `platformio.ini` | 11–19 | **HIGH** | Missing `build_src_filter` compiles unwanted files | Add `build_src_filter = +<*> -<CIT*.cpp>` |
| **D1** | `src/main.cpp` | 54–86 | **CRITICAL** | Missing `break;` in `case AVANZANDO:` causes unconditional fallthrough | Add `break;` before `case PREGIRO_DER:` |
| **D2** | `src/hardware/movimiento/PID.cpp` | 17–23 | **CRITICAL** | Raw distance used as error without setpoint subtraction for single-wall PID | Subtract target distance `DISTANCIA_PARED_OBJETIVO` |
| **D3** | `src/main.cpp` | 89, 120 | **HIGH** | Arbitrary `/ 2` on `verPulsosEncoderA()` doubles right turn pulse requirements | Remove `/ 2` to unify turn odometry |
| **D4** | `src/hardware/movimiento/puenteH.cpp` | 39–56 | **HIGH** | Direction polarities for `GIRAR_DER` and `GIRAR_IZQ` are inverted | Correct motor polarities for differential rotation |
| **D5** | `src/main.cpp` | 71–76 | **HIGH** | Continuous encoder reset and Bluetooth flood while advancing | Remove resets and logs during steady-state `AVANZANDO` |
| **D6** | `src/main.cpp` | 63–66 | **MEDIUM** | Strict `<` and `>` comparisons leave 130 mm deadzone | Use `<=` and `>=` across conditions |
| **D7** | `src/hardware/sensoresDistancia/sensoresDistancia.cpp` | 45, 59, 73 | **HIGH** | Infinite `while(true)` loop on sensor init failure hangs boot | Add non-blocking return status and timeout |
| **D8** | `src/hardware/sensoresDistancia/sensoresDistancia.cpp` | 23 | **MEDIUM** | 10 kHz I2C bus clock causes high latency | Increase clock to 100 kHz or 400 kHz |
| **D9** | `src/hardware/sensoresDistancia/sensoresDistancia.cpp` | 90–103 | **MEDIUM** | Raw distance underflow yields negative distance; timeout returns 1960 mm | Clamp distance at 0 and check sensor timeout |
| **D10** | `src/hardware/logger/logger.cpp` | 20–22 | **MEDIUM** | `cambioDeCelda()` does not consume byte, stays true permanently | Read byte from buffer upon check |
| **D11** | `src/hardware/logger/logger.h` etc. | Multiple | **MEDIUM** | Header declarations without implementations in `.cpp` | Synchronize headers with source implementations |
| **D12** | `src/config.h` | 49 | **HIGH** | `PWMA` mapped to GPIO 12 (ESP32 MTDI strapping pin) | Reassign to safe GPIO |
| **D13** | `src/config.h` | 42 | **MEDIUM** | `BOTON1` mapped to GPIO 34 (input-only, no internal pull-ups) | Ensure external pull-up or move to GPIO with pull-up |
| **D14** | `src/` root | Various | **LOW** | 8 scratch/test text files pollute source directory | Relocate test sketches to `test/` |

---

## 7. Architectural Recommendations & Best Practices

1. **Source Organization**:
   - Establish a clean separation between application logic (`src/app/`), hardware abstraction (`src/hal/` or `src/hardware/`), and configuration (`include/`).
   - Relocate all test sketches (`*.txt`, `pruebaEncoders`) to `test/manual_tests/`.

2. **Finite State Machine Architecture**:
   - Refactor `loop()` to follow a clean state-action-transition pattern with explicit `break;` on all cases and a `default:` watchdog handler.
   - Separate sensing/estimation from control execution with a fixed-rate scheduler (e.g. `Ticker` or `millis()` timer).

3. **Motion Control**:
   - Calibrate encoder ticks-per-millimeter and ticks-per-degree instead of raw arbitrary constants.
   - Implement acceleration/deceleration profiling to avoid wheel slip on sharp starts and stops.
