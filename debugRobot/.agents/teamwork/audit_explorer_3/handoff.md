# Audit Handoff Report: Distance Sensors, Telemetry/Logger, Build System, Legacy Files, and Hardware Pinout

**Auditor Agent**: `audit_explorer_3` (`teamwork_preview_explorer`)  
**Target Repository**: `debugRobot` (MicroMouse firmware for ESP32 DevKit V1)  
**Investigation Mode**: 100% Read-Only Code Audit  
**Date**: 2026-10-07  

---

## 1. Observation

Direct observations from source code inspection, tool invocations, and git history:

### 1.1 Distance Sensors (`src/hardware/sensoresDistancia/`)
- **`src/hardware/sensoresDistancia/sensoresDistancia.cpp:20-23`**:
  ```cpp
  20: gpio_set_pull_mode((gpio_num_t)SDA, GPIO_PULLUP_ONLY);
  21: gpio_set_pull_mode((gpio_num_t)SCL, GPIO_PULLUP_ONLY);
  22: 
  23: Wire.setClock(10000); // Recomendable combinar con la reducción de velocidad/
  ```
  The I2C bus clock is explicitly configured to 10,000 Hz (10 kHz) instead of standard 100 kHz or Fast Mode 400 kHz.

- **`src/hardware/sensoresDistancia/sensoresDistancia.cpp:42-45, 55-58, 68-71`**:
  ```cpp
  43:   if (!sensorDer.init()) {
  44:     Serial.printf("ERROR: fallo init sensor en pin %d\n", xshutPinDer);
  45:   }
  46: 
  47:   sensorDer.setAddress(adressDer);
  48:   sensorDer.startContinuous(0);
  ```
  In sequential XSHUT initialization, if `sensor.init()` returns false, an error message is printed to UART `Serial`, but execution proceeds unconditionally to `setAddress()` and `startContinuous(0)`. There is no retry, abort, or fallback state.

- **`src/hardware/sensoresDistancia/sensoresDistancia.cpp:80, 86, 91, 96, 104`**:
  ```cpp
  80:   static sensado lecturaAct = {0,0,0}; 
  81:   static unsigned long ultimoSensado = 0;
  ...
  86:     if((sensorIzq.readReg(VL53L0X::RESULT_INTERRUPT_STATUS) & 0x07) != 0){
  ...
  104:   return lecturaAct;
  ```
  `lecturaAct` is initialized with `{0, 0, 0}`. Until the VL53L0X sensors complete their first conversion (~33 ms) and set the interrupt status flag, `actualizarSensado()` returns `{0, 0, 0}`.
  In `src/main.cpp:74-77`, all threshold comparisons test `< UMBRAL_PARED_ESTADO_NORMAL` (130 mm).

- **`src/hardware/sensoresDistancia/sensoresDistancia.cpp:86-100`**:
  ```cpp
  86:     if((sensorIzq.readReg(VL53L0X::RESULT_INTERRUPT_STATUS) & 0x07) != 0){
  87:       uint16_t rawIzq = sensorIzq.readRangeContinuousMillimeters();
  88:       if (rawIzq > 2000) rawIzq = 2000;
  89:       lecturaAct.distanciaIzq = rawIzq - OFSET_IZQ;
  90:     }
  91:     if((sensorCent.readReg(VL53L0X::RESULT_INTERRUPT_STATUS) & 0x07) != 0){
  92:       uint16_t rawCent = sensorCent.readRangeContinuousMillimeters();
  93:       if (rawCent > 2000) rawCent = 2000;
  94:       lecturaAct.distanciaCent =  rawCent - OFSET_CENT;
  95:     }
  96:     if((sensorDer.readReg(VL53L0X::RESULT_INTERRUPT_STATUS) & 0x07) != 0){
  97:       uint16_t rawDer = sensorDer.readRangeContinuousMillimeters();
  98:       if (rawDer > 2000) rawDer = 2000;
  99:       lecturaAct.distanciaDer =  rawDer - OFSET_DER;
  100:    }
  ```
  Pololu VL53L0X library returns `65535` (`0xFFFF`) upon timeout or communication error. `raw > 2000` clamps `65535` to `2000`. Subtracting offsets results in `1960` mm (left), `1950` mm (center), and `1953` mm (right).

- **`src/hardware/sensoresDistancia/sensoresDistancia.cpp:85, 102`**:
  ```cpp
  85: //  if(millis() - ultimoSensado > 20){
  ...
  102:   //}
  ```
  The 20 ms rate-limiting guard is commented out, causing 3 I2C register reads on every single execution of `loop()`.

---

### 1.2 Telemetry & Logger (`src/hardware/logger/`)
- **`src/hardware/logger/logger.cpp:9-16`**:
  ```cpp
  9:  void enviarString(String str){
  10:     SerialBT.println(str);
  11: }
  12: 
  13: void inicializarLogger(){
  14:     SerialBT.begin("Manati");
  15:     Serial.begin(115200);
  16: }
  ```
  `enviarString` writes exclusively to `SerialBT.println(str)`. USB UART (`Serial`) is initialized at 115200 baud but receives zero logs or telemetry messages during execution.

- **Empirical PlatformIO Build Output (`pio run`)**:
  ```text
  RAM:   [=         ]  12.4% (used 40604 bytes from 327680 bytes)
  Flash: [========= ]  86.8% (used 1137453 bytes from 1310720 bytes)
  ```
  The compiled binary consumes 1,137,453 bytes of flash. In the default partition table (`default.csv`), `app0` has a capacity of 1,310,720 bytes (1.25 MB). Flash headroom is only 173,267 bytes (13.2%).

- **`src/hardware/logger/logger.cpp:18-24, 27`**:
  ```cpp
  18: bool cambioDeCelda(){
  19:     if(SerialBT.available()>0){ return true;} else { return false;}
  20: }
  21: 
  22: bool hayDatosBT() {
  23:     return SerialBT.available() > 0;
  24: }
  ...
  27:     String trama = SerialBT.readStringUntil('\n');
  ```
  `cambioDeCelda()` checks `SerialBT.available() > 0`, behaving identically to `hayDatosBT()`.
  `readStringUntil('\n')` utilizes Stream's default 1000 ms timeout, blocking synchronously if a newline delimiter is missing.

---

### 1.3 Legacy Files & Build Configuration (`src/CITÉ.cpp`, `platformio.ini`)
- **`platformio.ini:11-19`**:
  ```ini
  [env:esp32doit-devkit-v1]
  platform = espressif32
  board = esp32doit-devkit-v1
  framework = arduino
  lib_deps =
      VL53L0X
      madhephaestus/ESP32Encoder @ ^0.11.7

  monitor_speed = 115200
  ```
  `build_src_filter` is omitted. `board_build.partitions` is omitted.

- **`src/CITÉ.cpp` (renamed to `src/CITE.txt` in commit bceb5e4)**:
  Direct view of `src/CITE.txt:13-20, 59, 114`:
  ```cpp
  13: sensado sensadoActual = {0,0,0};
  14: VELOCIDAD velocidadActual = {0,0};
  15: MAQUINA_ESTADOS estado = LISTO;
  16: uint32_t pulsosActuales = 0;
  ...
  20: MAQUINA_ESTADOS estadoPostFreno = LISTO;
  ...
  59: void setup (){
  ...
  114: void loop(){
  ```
  Duplicate entry points `setup()` and `loop()`, duplicated global variables, and references to obsolete `MAQUINA_ESTADOS` (which is commented out in `src/main.h:2-12`).
  In commit d16f46c, the filename was `src/CITÉ.cpp` (UTF-8 bytes `0xC3 0x89`), causing Windows toolchains to fail with `xtensa-esp32-elf-g++: error: src/CIT.cpp: No such file or directory`.

---

### 1.4 Hardware Pinout & Silicon Constraints (`src/config.h`)
- **`src/config.h:42-50`**:
  ```cpp
  42: #define BOTON1 34
  43: #define BOTON2 35
  ...
  49: #define PWMA 12
  50: #define PWMB 32
  ```
- **ESP32 Silicon Pin Architecture**:
  1. GPIO 12 is strapping pin `MTDI`. If MTDI is sampled HIGH at boot/reset, internal VDD_SDIO switches to 1.8V instead of 3.3V, causing SPI flash read corruption and continuous bootloop.
  2. GPIO 34 and GPIO 35 are GPI (input-only) pins without internal pull-up or pull-down circuitry in silicon. `pinMode(BOTON1, INPUT)` in `src/main.cpp:28` leaves the pin floating if an external physical resistor is absent.

- **`src/config.h:22-24`**:
  ```cpp
  22: #define OFSET_DER 47
  23: #define OFSET_IZQ 40
  24: #define OFSET_CENT 50
  ```
  7 mm physical offset asymmetry between left (40 mm) and right (47 mm) sensors.

---

### 1.5 Header Declarations vs Implementations
- **`src/hardware/movimiento/PID.h:4-6` vs `src/hardware/movimiento/PID.cpp`**:
  ```cpp
  4: int16_t calcularCorreccion(sensado mediciones);
  5: int16_t calcularCorreccionRightHand(int16_t error);
  6: void resetearErrorAnterior();
  ```
  `calcularCorreccionRightHand` is declared on line 5 of `PID.h`. Searching `PID.cpp` yields zero definitions for this function.

---

## 2. Logic Chain

1. **Sensor Timeout Masking & Dynamic Collsion**:
   - *Observation*: `sensoresDistancia.cpp:88, 93, 98` clamps readings `> 2000` down to `2000`.
   - *Logic*: When a VL53L0X sensor encounters an optical timeout or communication failure, Pololu library returns `65535`. Clamping `65535` to `2000` turns a fatal sensor error into a valid reading of ~1950 mm.
   - *Conclusion*: A failing sensor reports an open corridor. In a maze corner or approaching a dead-end, the robot will fail to detect walls and crash into them at full speed.

2. **Zero-Initialization Hazard & Boot False-Turn**:
   - *Observation*: `lecturaAct` is initialized to `{0,0,0}` (`sensoresDistancia.cpp:80`).
   - *Logic*: The VL53L0X takes ~33 ms per continuous measurement. In the first few cycles of `loop()`, status checks return 0, leaving `lecturaAct` at `{0,0,0}`. In `main.cpp:77`, `distanciaDer < 130 && distanciaCent < 130 && distanciaIzq < 130` evaluates to TRUE for `{0,0,0}`.
   - *Conclusion*: Immediately upon entering `AVANZANDO`, the robot evaluates a dead-end condition and transitions to `GIRANDO_180` without moving forward.

3. **I2C Clock Degradation & Loop Starvation**:
   - *Observation*: Clock is set to `10000` Hz (`sensoresDistancia.cpp:23`), and rate limiter is commented out (`sensoresDistancia.cpp:85, 102`).
   - *Logic*: An I2C byte transfer at 10 kHz takes ~1 ms. Reading interrupt registers and range data across 3 sensors consumes 20-30 ms per iteration.
   - *Conclusion*: The main loop cycle time is severely throttled by bus waiting times, preventing rapid motor PID adjustments and inducing odometry drift.

4. **Sequential XSHUT Error Cascading**:
   - *Observation*: `sensor.init()` return value is logged via `Serial.printf` without conditional branch or error abort (`sensoresDistancia.cpp:43-48`).
   - *Logic*: All sensors power up at default I2C address `0x29`. If `sensorDer.init()` fails, its address is never reassigned to `0x30`. When `xshutPinCent` is brought HIGH, both Sensor Der and Sensor Cent reside at `0x29`.
   - *Conclusion*: Bus collision occurs on `0x29`, rendering all subsequent sensor initializations invalid.

5. **Flash Saturation & Build Breakage**:
   - *Observation*: `BluetoothSerial` is included (`logger.cpp:3, 5`), and `pio run` reports `Flash: 86.8% (used 1137453 bytes from 1310720 bytes)`.
   - *Logic*: Default ESP32 partition `app0` has only 1.25 MB. The firmware already takes 1.137 MB. MicroMouse maze navigation algorithms (FloodFill, distance transform, cell map arrays) require substantial flash and RAM structures.
   - *Conclusion*: Adding maze-solving logic or telemetry logging buffers will immediately overflow the 1.25 MB partition during linking.

6. **Observability Blackout**:
   - *Observation*: `enviarString()` only calls `SerialBT.println(str)` (`logger.cpp:10`). `Serial.begin(115200)` is initialized (`logger.cpp:15`), but never written to by `enviarString()`.
   - *Logic*: PlatformIO monitors the USB UART interface at 115200 baud.
   - *Conclusion*: The developer/tester receives zero output on the USB serial console, making debugging impossible unless an SPP Bluetooth terminal is actively paired.

7. **Silicon Strapping Pin Bootloop**:
   - *Observation*: `PWMA` is mapped to GPIO 12 (`config.h:49`).
   - *Logic*: On ESP32, GPIO 12 is MTDI. If sampled HIGH at reset, internal flash LDO shifts from 3.3V to 1.8V. Motor driver boards (e.g. TB6612FNG) often exhibit pull-up or voltage leakage on input pins.
   - *Conclusion*: When powered or reset, the ESP32 enters a permanent bootloop due to flash undervoltage.

8. **Floating Button False Trigger**:
   - *Observation*: `BOTON1` is GPIO 34 (`config.h:42`) and configured as `INPUT` (`main.cpp:28`).
   - *Logic*: GPIO 34 has no internal pull-up resistor in ESP32 silicon.
   - *Conclusion*: Without an external pull-up resistor on the PCB, electrical noise on the floating pin triggers `digitalRead(BOTON1) == LOW`, causing the robot to launch unpredictably.

9. **Orphaned Declaration Linker Hazard**:
   - *Observation*: `PID.h:5` declares `calcularCorreccionRightHand(int16_t error);` with no corresponding definition in `PID.cpp`.
   - *Logic*: Invoking this declared prototype results in an unresolved symbol during the link phase.
   - *Conclusion*: Dead/orphaned API declaration degrades code hygiene and creates linker hazards.

---

## 3. Caveats

- **External PCB Pull-ups**: Whether the physical custom PCB contains external 10 kΩ pull-ups on GPIO 34/35 or external pull-down on GPIO 12 cannot be verified from read-only software code alone; however, silicon-level constraints dictate that the firmware must not rely on internal pull-ups for GPIO 34-39.
- **Hardware Revision of VL53L0X Modules**: Pololu modules typically contain onboard 2.8V regulators and level shifters with weak pull-ups, but operating 3 devices on a shared bus without dedicated 2.2k-4.7kΩ pull-ups to 3.3V causes signal rise-time degradation at 400 kHz.
- **No Source Code Modifications**: Under strict read-only audit instructions, no files in `src/` were edited or committed.

---

## 4. Conclusion & Complete Defect Catalog

### Summary Table of Audited Defect Findings

| Bug ID | File Path | Line(s) | Severity | Category | Defect Title |
|---|---|---|---|---|---|
| **AUD-01** | `src/hardware/sensoresDistancia/sensoresDistancia.cpp` | 87–99 | **High** | Sensor Logic | VL53L0X Timeout (65535) Masked as 1960 mm Open Hallway |
| **AUD-02** | `src/hardware/sensoresDistancia/sensoresDistancia.cpp` | 80, 104 | **High** | Sensor Logic | Static Cache Zero-Init `{0,0,0}` Induces Immediate 180° Spin on Startup |
| **AUD-03** | `src/hardware/sensoresDistancia/sensoresDistancia.cpp` | 43–48, 56–61, 69–74 | **High** | Sensor Driver | Sequential XSHUT Init Ignores Failures, Leading to Address Collision on 0x29 |
| **AUD-04** | `src/hardware/sensoresDistancia/sensoresDistancia.cpp` | 23 | **Medium** | I2C Performance | Bus Clock Degraded to 10 kHz (Imposes 20–30 ms Latency Bottleneck) |
| **AUD-05** | `src/hardware/sensoresDistancia/sensoresDistancia.cpp` | 85, 102 | **Medium** | I2C Optimization | Commented 20 ms Rate Limiter Results in Continuous Bus Flooding |
| **AUD-06** | `src/hardware/sensoresDistancia/sensoresDistancia.cpp` | 89, 94, 99 | **Medium** | Calibration / Types | Offset Subtraction Produces Negative Distances and 7 mm Asymmetric Bias |
| **AUD-07** | `platformio.ini` | 11–19 | **High** | Build / Memory | Default Partition Flash Saturation (86.8% Used by BluetoothSerial) |
| **AUD-08** | `src/hardware/logger/logger.cpp` | 9–11, 14–16 | **Medium** | Telemetry | Inobservability on USB UART (`enviarString` Routes Exclusively to Bluetooth) |
| **AUD-09** | `src/hardware/logger/logger.cpp` | 27 | **Medium** | Telemetry | Synchronous 1000 ms Timeout in `SerialBT.readStringUntil('\n')` Freezes CPU |
| **AUD-10** | `src/hardware/logger/logger.cpp` | 18–20 | **Low** | Telemetry | Semantic Mismatch: `cambioDeCelda()` Checks Bluetooth Rx Instead of Odometry |
| **AUD-11** | `src/config.h` | 49 | **High** | Hardware / Strapping | GPIO 12 (`PWMA`) is MTDI Strapping Pin: High Voltage Boots Flash at 1.8V (Bootloop) |
| **AUD-12** | `src/config.h` | 42 | **High** | Hardware / GPIO | GPIO 34 (`BOTON1`) Has No Internal Silicon Pull-Up; Risk of False Runaway Starts |
| **AUD-13** | `platformio.ini` | 11–19 | **High** | Build System | Missing `build_src_filter` Allows Inadvertent Compilation of Test Files in `src/` |
| **AUD-14** | `src/CITÉ.cpp` (`src/CITE.txt`) | File & 13–20, 59, 114 | **Critical** | Build / Toolchain | Non-ASCII Filename GCC Crash, Duplicate `setup`/`loop`, and Obsolete Enum |
| **AUD-15** | `src/hardware/movimiento/PID.h` | 5 | **Medium** | Architecture | Orphaned Declaration `calcularCorreccionRightHand` Lacks Implementation |

---

### Detailed Analysis of Each Defect

#### AUD-01: VL53L0X Timeout (65535) Masked as 1960 mm Open Hallway
- **File**: `src/hardware/sensoresDistancia/sensoresDistancia.cpp`
- **Lines**: 87–89, 92–94, 97–99
- **Severity**: High
- **Root Cause**: The Pololu `VL53L0X` library returns `65535` (`0xFFFF`) upon measurement timeout or I2C communication abort. The code tests `if (raw > 2000) raw = 2000;`. Since 65535 is greater than 2000, it clamps the value to 2000 and subtracts offsets (40/50/47 mm), outputting ~1950–1960 mm.
- **Dynamic Consequences**: A disconnected sensor, dirty optics, or timing failure is reported as a completely open hallway nearly 2 meters away. The robot fails to register front or side walls and crashes violently into maze boundaries.

#### AUD-02: Static Cache Zero-Init `{0,0,0}` Induces Immediate 180° Spin on Startup
- **File**: `src/hardware/sensoresDistancia/sensoresDistancia.cpp`
- **Lines**: 80, 104
- **Severity**: High
- **Root Cause**: `static sensado lecturaAct = {0,0,0};` initializes all distance outputs to zero. In continuous ranging mode, the VL53L0X requires ~33 ms to complete its initial ranging cycle. Until the interrupt flag is raised, `actualizarSensado()` returns `{0,0,0}`.
- **Dynamic Consequences**: When transitioning from `LISTO` to `AVANZANDO` in `src/main.cpp:77`, `condicionGiro180` evaluates whether all three distances are `< UMBRAL_PARED_ESTADO_NORMAL` (130 mm). Because `0 < 130`, the robot registers a complete dead-end at the starting cell and initiates an unwanted 180° rotation immediately upon launch.

#### AUD-03: Sequential XSHUT Init Ignores Failures, Leading to Address Collision on 0x29
- **File**: `src/hardware/sensoresDistancia/sensoresDistancia.cpp`
- **Lines**: 43–48, 56–61, 69–74
- **Severity**: High
- **Root Cause**: If `sensorDer.init()` returns `false`, `inicializacionSensoresDist()` logs an error via `Serial.printf` and proceeds to call `sensorDer.setAddress(adressDer)` without checking if the device responded. When `sensorCent` is powered up via `xshutPinCent`, both devices share the default address `0x29`.
- **Dynamic Consequences**: I2C bus arbitration fails or corrupts data due to multiple devices responding on address `0x29`, rendering all subsequent distance sensing inoperative.

#### AUD-04: Bus Clock Degraded to 10 kHz (Imposes 20–30 ms Latency Bottleneck)
- **File**: `src/hardware/sensoresDistancia/sensoresDistancia.cpp`
- **Line**: 23
- **Severity**: Medium
- **Root Cause**: `Wire.setClock(10000);` reduces the I2C bus clock to 10 kHz (10x slower than standard 100 kHz, 40x slower than 400 kHz Fast Mode).
- **Dynamic Consequences**: Each register read takes several milliseconds. Polling 3 sensors on each control loop cycle creates a 20–30 ms delay, starving the main loop and severely degrading PID real-time tracking performance.

#### AUD-05: Disabled 20 ms Rate Limiter Flooding I2C Bus
- **File**: `src/hardware/sensoresDistancia/sensoresDistancia.cpp`
- **Lines**: 85, 102
- **Severity**: Medium
- **Root Cause**: The rate limiter `// if(millis() - ultimoSensado > 20){` is commented out.
- **Dynamic Consequences**: In every execution of `loop()`, I2C transactions are sent over the slow 10 kHz bus, unnecessarily increasing CPU core utilization and creating variable control jitter.

#### AUD-06: Offset Subtraction Produces Negative Distances and 7 mm Asymmetric Bias
- **File**: `src/hardware/sensoresDistancia/sensoresDistancia.cpp` (Lines 89, 94, 99) and `src/config.h` (Lines 22–24)
- **Severity**: Medium
- **Root Cause**: `OFSET_IZQ` (40) and `OFSET_DER` (47) are subtracted directly from `raw` (`uint16_t`) and assigned to `int16_t`. If distance is < 40 mm, negative values are produced without saturation. Additionally, the 7 mm difference between left and right offsets introduces a persistent lateral centering bias into PID.
- **Dynamic Consequences**: When the robot gets close to a wall, negative values distort PID error calculations, and the asymmetric calibration causes the robot to drive 7 mm closer to the left wall.

#### AUD-07: Default Partition Flash Saturation (86.8% Used by BluetoothSerial)
- **File**: `platformio.ini` (Lines 11–19) and `src/hardware/logger/logger.cpp` (Lines 3, 5)
- **Severity**: High
- **Root Cause**: Linking `BluetoothSerial` pulls in the heavy ESP-IDF Bluedroid SPP stack, consuming 1,137,453 bytes (86.8%) of the default 1.25 MB `app0` partition.
- **Dynamic Consequences**: Leaves only 170 KB of flash space. Implementing maze grid mapping or FloodFill algorithm will overflow the flash partition and abort compilation.

#### AUD-08: Inobservability on USB UART (`enviarString` Routes Exclusively to Bluetooth)
- **File**: `src/hardware/logger/logger.cpp` (Lines 9–11, 14–16)
- **Severity**: Medium
- **Root Cause**: `enviarString(String str)` calls only `SerialBT.println(str)`. Although `Serial.begin(115200)` is executed, USB UART never receives FSM state transitions or telemetry.
- **Dynamic Consequences**: The PlatformIO Serial Monitor remains completely silent when connected via USB cable. Without active Bluetooth pairing, no debug or diagnostic telemetry can be observed.

#### AUD-09: Synchronous 1000 ms Timeout in `SerialBT.readStringUntil('\n')` Freezes CPU
- **File**: `src/hardware/logger/logger.cpp`
- **Line**: 27
- **Severity**: Medium
- **Root Cause**: `SerialBT.readStringUntil('\n')` uses Arduino Stream's default 1-second timeout.
- **Dynamic Consequences**: If a Bluetooth command arrives without a trailing newline `\n` or packet transmission stalls, the ESP32 CPU is blocked for 1000 ms, freezing the navigation and motor control loop.

#### AUD-10: Semantic Mismatch: `cambioDeCelda()` Checks Bluetooth Rx Instead of Odometry
- **File**: `src/hardware/logger/logger.cpp` (Lines 18–20) and `src/hardware/logger/logger.h` (Line 12)
- **Severity**: Low
- **Root Cause**: `cambioDeCelda()` is defined as `if(SerialBT.available()>0){ return true;} else { return false;}`. It checks for Bluetooth serial bytes instead of detecting cell boundaries.
- **Dynamic Consequences**: Misleads callers attempting to use the API for cell transition synchronization.

#### AUD-11: GPIO 12 (`PWMA`) is MTDI Strapping Pin: High Voltage Boots Flash at 1.8V (Bootloop)
- **File**: `src/config.h`
- **Line**: 49
- **Severity**: High
- **Root Cause**: GPIO 12 is the hardware strapping pin `MTDI`. If sampled HIGH at power-up or reset, internal LDO voltage for SPI flash drops from 3.3V to 1.8V.
- **Dynamic Consequences**: If the motor driver board has an internal pull-up or voltage leakage on its PWM line, the ESP32 fails to read flash memory upon boot and enters an unrecoverable bootloop.

#### AUD-12: GPIO 34 (`BOTON1`) Has No Internal Silicon Pull-Up; Risk of False Runaway Starts
- **File**: `src/config.h` (Line 42) and `src/main.cpp` (Line 28)
- **Severity**: High
- **Root Cause**: GPIO 34 is an input-only (GPI) pin without internal pull-up or pull-down circuitry in silicon. `pinMode(BOTON1, INPUT)` leaves the line floating.
- **Dynamic Consequences**: If no physical pull-up resistor is present on the PCB, electromagnetic interference or motor noise will cause false `LOW` readings, prematurely triggering autonomous movement (`estado = AVANZANDO`).

#### AUD-13: Missing `build_src_filter` Allows Inadvertent Compilation of Test Files in `src/`
- **File**: `platformio.ini`
- **Lines**: 11–19
- **Severity**: High
- **Root Cause**: `platformio.ini` lacks `build_src_filter`. PlatformIO defaults to compiling all `.c`, `.cpp`, `.S` files found under `src/`.
- **Dynamic Consequences**: Any experimental or temporary file placed in `src/` (such as `CITÉ.cpp` or scripts renamed to `.cpp`) is automatically included in the build, causing duplicate symbol errors and compiler failures.

#### AUD-14: Non-ASCII Filename GCC Crash, Duplicate `setup`/`loop`, and Obsolete Enum
- **File**: `src/CITÉ.cpp` (now `src/CITE.txt`)
- **Lines**: File path, 13–20, 59, 114
- **Severity**: Critical
- **Root Cause**: The file was historically named `src/CITÉ.cpp` containing non-ASCII character `É`. On Windows, the GCC cross-compiler terminates with a file-not-found error. Additionally, it implements duplicated `setup()` and `loop()` functions and references commented-out enum `MAQUINA_ESTADOS`.
- **Dynamic Consequences**: Complete build stoppage if included in the compiler source tree.

#### AUD-15: Orphaned Declaration `calcularCorreccionRightHand` Lacks Implementation
- **File**: `src/hardware/movimiento/PID.h`
- **Line**: 5
- **Severity**: Medium
- **Root Cause**: Prototype `int16_t calcularCorreccionRightHand(int16_t error);` is declared in `PID.h`, but no definition exists in `PID.cpp`.
- **Dynamic Consequences**: Linker error `undefined reference` if invoked by any module.

---

## 5. Verification Method

To independently verify these findings:

1. **Verify Flash Memory Saturation**:
   Run PlatformIO build from repository root:
   ```powershell
   & "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run
   ```
   *Expected output*: `Flash: [========= ] 86.8% (used 1137453 bytes from 1310720 bytes)`.
   *Invalidation condition*: Flash usage below 50% or non-default partition table specified.

2. **Verify Orphaned Prototype in `PID.h`**:
   Inspect line 5 of `src/hardware/movimiento/PID.h`:
   ```cpp
   int16_t calcularCorreccionRightHand(int16_t error);
   ```
   Search for definition in `src/hardware/movimiento/PID.cpp`:
   ```powershell
   Select-String -Path "src/hardware/movimiento/PID.cpp" -Pattern "calcularCorreccionRightHand"
   ```
   *Expected result*: No matches found.

3. **Verify VL53L0X Timeout Masking (65535 Clamping)**:
   Inspect lines 87–99 of `src/hardware/sensoresDistancia/sensoresDistancia.cpp`:
   ```cpp
   uint16_t rawIzq = sensorIzq.readRangeContinuousMillimeters();
   if (rawIzq > 2000) rawIzq = 2000;
   lecturaAct.distanciaIzq = rawIzq - OFSET_IZQ;
   ```
   Confirm that when `readRangeContinuousMillimeters()` returns `65535` on timeout, `rawIzq` is clamped to `2000`, yielding `1960` mm.

4. **Verify Silicon Constraints on GPIO 12 and GPIO 34**:
   Cross-reference ESP32 Technical Reference Manual (Section 4.10 Strapping Pins and Section 4.1 IO MUX):
   - MTDI (GPIO 12) bootstrap logic level sets VDD_SDIO to 1.8V if HIGH.
   - GPIO 34–39 are GPI pins lacking internal pull-up and pull-down resistors.
