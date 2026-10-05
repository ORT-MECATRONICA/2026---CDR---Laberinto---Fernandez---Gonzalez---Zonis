# Adversarial Challenge & Verification Report: `bug_report.md`

**Project:** debugRobot (MicroMouse Autonomous Maze Solver)  
**Evaluator:** `teamwork_preview_challenger` (Instance 1 — Critic / Specialist)  
**Target Deliverable:** `c:\Users\devandroid\Documents\GitHub\Laberinto\debugRobot\bug_report.md`  
**Reference Specification:** `c:\Users\devandroid\Documents\GitHub\Laberinto\debugRobot\.agents\ORIGINAL_REQUEST.md`  
**Date:** 2026-10-05  

---

## 1. Challenge Summary

**Overall Risk Assessment of `bug_report.md`:** **LOW (NO FALSE POSITIVES DETECTED / AUDIT REPORT IS HIGHLY ACCURATE)**  
**Verdict on Code Integrity:** **CLEAN (0 application source code files modified by agents; strict read-only constraint observed)**  
**Recommendation:** **APPROVE**

---

## 2. Adversarial Challenges & Empirical Verifications

### Challenge 1: Build Failure & Toolchain Blocking Claims (BUG-01, BUG-02, BUG-03)
- **Assumption Challenged:** Did the audit workers fabricate or exaggerate the claim that the firmware fails to compile due to `src/CITÉ.cpp`?
- **Attack Scenario:** Perhaps `platformio run` tolerates non-ASCII UTF-8 filenames under Windows or ignores `CITÉ.cpp` if an entry point already exists in `main.cpp`.
- **Empirical Test:**
  Executed PlatformIO build toolchain directly:
  ```powershell
  & "C:\Users\devandroid\.platformio\penv\Scripts\platformio.exe" run
  ```
- **Observed Toolchain Output:**
  ```text
  Compiling .pio\build\esp32doit-devkit-v1\src\CIT.cpp.o
  xtensa-esp32-elf-g++: error: src/CIT.cpp: No such file or directory
  xtensa-esp32-elf-g++: fatal error: no input files
  compilation terminated.
  *** [.pio\build\esp32doit-devkit-v1\src\CIT.cpp.o] Error 1
  [FAILED] Took 8.25 seconds
  ```
- **Finding:** The bug is 100% reproducible and fatal. Furthermore, direct inspection of `src/CITÉ.cpp` confirmed duplicate declarations of `setup()` (line 23), `loop()` (line 36), and global state variables colliding with `src/main.cpp`, alongside an undeclared type `MAQUINA_ESTADOS` (line 16) which is commented out in `src/main.h:3-10`.
- **Verdict:** **CONFIRMED DEFECT — NOT A FALSE POSITIVE.**

---

### Challenge 2: Control Flow Fallthrough in FSM (BUG-07)
- **Assumption Challenged:** Does `case AVANZANDO:` really fall through into `case PREGIRO_DER:` in `src/main.cpp:54-88`? Does C++ scoped compound block `{ ... }` prevent fallthrough?
- **Attack Scenario:** In C/C++, a compound statement `{ ... }` within a switch label defines local variable scope, but does **not** terminate switch control flow without a `break;`, `return;`, or `goto;`.
- **Empirical Code Inspection (`src/main.cpp:84-88`):**
  ```cpp
  84:         estado = GIRANDO_180;
  85:       }
  86:     }
  87: 
  88:     case  PREGIRO_DER : {
  ```
  Line 86 closes the brace for `case AVANZANDO:` without a `break;`.
- **Blast Radius & Impact:** On every iteration where `estado == AVANZANDO`, the MCU evaluates PID and issues motor commands at line 60, but immediately falls through to execute line 92 inside `case PREGIRO_DER:` (`movimiento(AVANZAR, {VEL_BASE_IZQ, VEL_BASE_DER})`), instantly erasing the PID speed correction. Furthermore, once `pulsosActuales >= PULSOS_PREGIRO_90_DER` (300 pulses), lines 93–99 execute, commanding `FRENO_F` and transitioning the robot to `GIRANDO_DER` regardless of maze walls.
- **Verdict:** **CONFIRMED CRITICAL DEFECT — NOT A FALSE POSITIVE.**

---

### Challenge 3: Inverted PID Wall-Following Error & Missing Setpoint (BUG-16)
- **Assumption Challenged:** Did the auditor confuse the sign convention of the single-wall PID algorithm?
- **Attack Scenario:** Traced mathematical control law through `src/hardware/movimiento/PID.cpp:17-23` and `src/main.cpp:57-60`:
  ```cpp
  // PID.cpp:17-23
  } else if (hayIzq) {
      error = - (int16_t)mediciones.distanciaIzq;
  }
  ```
  ```cpp
  // main.cpp:58-59
  velocidadActual.izquierda = constrain(VEL_BASE_IZQ + correccion, 0, 255);
  velocidadActual.derecha   = constrain(VEL_BASE_DER - correccion, 0, 255);
  ```
- **Mathematical Simulation:**
  - Suppose left wall is detected at nominal distance `distanciaIzq = 45 mm`.
  - `error = -45`.
  - With $K_p = 0.5$, $\text{correccion} \approx -22$.
  - $\text{velIzq} = 45 + (-22) = 23$ (slow left wheel).
  - $\text{velDer} = 45 - (-22) = 67$ (fast right wheel).
  - Slower left wheel + faster right wheel steers the robot to the **LEFT**, directly into the wall. As the robot gets closer (e.g. 20 mm), error remains negative, steering even harder into the wall.
  - In addition, there is no setpoint subtraction ($\text{distancia} - \text{setpoint}$), so even if the robot is at the ideal distance, a permanent large error is generated.
- **Verdict:** **CONFIRMED FATAL ALGORITHMIC DEFECT — NOT A FALSE POSITIVE.**

---

### Challenge 4: Inversion of Motor Rotation Polarity (BUG-19)
- **Assumption Challenged:** Could the motor pins or wheel assignments be inverted elsewhere in hardware, making the software implementation correct?
- **Empirical Hardware Driver Inspection (`src/hardware/movimiento/puenteH.cpp:12-56`):**
  - Line 14-15: Channel 0 is `PWMA` (Motor A, left motor). Channel 1 is `PWMB` (Motor B, right motor).
  - Line 20-27 (`AVANZAR`):
    - Motor A (Left): `AIN1=HIGH, AIN2=LOW` (Forward)
    - Motor B (Right): `BIN1=HIGH, BIN2=LOW` (Forward)
  - Line 39-47 (`GIRAR_DER`):
    - Motor A (Left): `AIN1=LOW, AIN2=HIGH` (**Reverse**)
    - Motor B (Right): `BIN1=HIGH, BIN2=LOW` (**Forward**)
    - Differential drive kinematics: Left wheel reverse + Right wheel forward produces a counter-clockwise rotation (**TURNING LEFT**).
  - Line 48-56 (`GIRAR_IZQ`):
    - Motor A (Left): `AIN1=HIGH, AIN2=LOW` (**Forward**)
    - Motor B (Right): `BIN1=LOW, BIN2=HIGH` (**Reverse**)
    - Differential drive kinematics: Left wheel forward + Right wheel reverse produces a clockwise rotation (**TURNING RIGHT**).
- **Verdict:** **CONFIRMED CRITICAL INVERSION — NOT A FALSE POSITIVE.**

---

### Challenge 5: Asymmetric Encoder Pulse Division (BUG-13) & 180° Pulse Mismatch (BUG-14)
- **Assumption Challenged:** Is the `/ 2` division in `PREGIRO_DER` and `GIRANDO_DER` accounted for in `config.h`?
- **Empirical Inspection:**
  - `src/main.cpp:89`: `pulsosActuales = (abs(verPulsosEncoderA())) / 2;`
  - `src/main.cpp:120`: `pulsosActuales = (abs(verPulsosEncoderA())) / 2;`
  - `src/main.cpp:104`: `pulsosActuales = abs(verPulsosEncoderB());` (NO division by 2)
  - `src/main.cpp:135`: `pulsosActuales = abs(verPulsosEncoderB());` (NO division by 2)
  - In `src/config.h:69-70`: `PULSOS_GIRO_90_DER` is 300, while `PULSOS_GIRO_90_IZQ` is 280.
  - Because Encoder A pulses are halved in software, the right motor must physically rotate for 600 encoder counts to satisfy the condition, causing a ~180° rotation when a 90° turn was requested.
  - In `src/config.h:71`: `PULSOS_GIRO_180 300` is identical to `PULSOS_GIRO_90_DER 300`, making a 180° dead-end turnaround turn only 90°.
- **Verdict:** **CONFIRMED CRITICAL DEFECTS — NOT A FALSE POSITIVE.**

---

### Challenge 6: Silicon Strapping Pin & Input Pin Risks (BUG-28, BUG-29)
- **Assumption Challenged:** Are GPIO 12 (`PWMA`) and GPIO 34 (`BOTON1`) valid ESP32 GPIOs? Is the risk genuine?
- **Datasheet Cross-Check (Espressif ESP32 Technical Reference Manual & Datasheet):**
  - **GPIO 12 is MTDI**: A critical strapping pin. If sampled HIGH at chip boot, the internal LDO voltage for SPI flash memory is configured to 1.8V instead of 3.3V. Standard 3.3V flash fails to read, throwing `flash read err, 1000` and entering an infinite boot loop.
  - **GPIO 34 is a GPI pin (Input Only)**: Pins GPIO 34–39 do NOT contain internal pull-up or pull-down resistors in silicon. `pinMode(BOTON1, INPUT)` leaves the pin floating in high impedance unless an external physical pull-up resistor is soldered on the PCB.
- **Verdict:** **CONFIRMED HIGH-SEVERITY HARDWARE RISKS — NOT A FALSE POSITIVE.**

---

## 3. Comprehensive Defect Catalog Verification Matrix

Every reported defect in `bug_report.md` was checked for existence, file path, line numbers, and veracity:

| Bug ID | Category | Claimed Location | Empirically Verified? | False Positive? | Severity Rating Assessment |
|---|---|---|---|---|---|
| **BUG-01** | Toolchain | `src/CITÉ.cpp` | Yes (Reproduced with `pio run`) | No | Accurate (Critical) |
| **BUG-02** | Linker | `src/CITÉ.cpp:14-36` | Yes (Duplicate globals/setup/loop) | No | Accurate (Critical) |
| **BUG-03** | Syntax | `src/CITÉ.cpp:16` | Yes (`MAQUINA_ESTADOS` undeclared) | No | Accurate (Critical) |
| **BUG-04** | Build System | `platformio.ini:11-19` | Yes (No `build_src_filter`) | No | Accurate (High) |
| **BUG-05** | Headers | `logger.h`, `sensoresDistancia.h`, `puenteH.h` | Yes (5 orphan declarations) | No | Accurate (High) |
| **BUG-06** | Modular API | `puenteH.cpp:69-80` | Yes (Missing header prototypes) | No | Accurate (Low) |
| **BUG-07** | FSM | `main.cpp:54-88` | Yes (Missing `break;` in AVANZANDO) | No | Accurate (Critical) |
| **BUG-08** | Odometry | `main.cpp:71-76` | Yes (Resetting encoders in loop) | No | Accurate (Critical) |
| **BUG-09** | PID Deriv. | `main.cpp:73` | Yes (Resetting `errorAnterior` in loop)| No | Accurate (High) |
| **BUG-10** | FSM Logic | `main.cpp:63-66` | Yes (Strict inequality dead zone) | No | Accurate (High) |
| **BUG-11** | FSM Enum | `main.h:12-22` vs `main.cpp` | Yes (DECISION & POSTGIRO unhandled)| No | Accurate (Medium) |
| **BUG-12** | Safety | `main.cpp:91, 106, 122, 137, 152` | Yes (No timeout guard in turns) | No | Accurate (High) |
| **BUG-13** | Odometry | `main.cpp:89, 120` | Yes (Asymmetric `/ 2` on Encoder A)| No | Accurate (Critical) |
| **BUG-14** | Odometry | `config.h:71`, `main.cpp:152` | Yes (`PULSOS_GIRO_180 == 300`) | No | Accurate (Critical) |
| **BUG-15** | Architecture | `config.h:66` vs `main.cpp` | Yes (`PULSOS_CELDA` unused) | No | Accurate (High) |
| **BUG-16** | Control | `PID.cpp:17-23` | Yes (Single wall PID inverted/no setpoint)| No | Accurate (Critical) |
| **BUG-17** | Calibration | `sensoresDistancia.cpp:92, 102` | Yes (40 vs 47 mm permanent offset)| No | Accurate (High) |
| **BUG-18** | Control | `PID.cpp:28` | Yes (No $\Delta t$ normalization) | No | Accurate (Medium) |
| **BUG-19** | Kinematics | `puenteH.cpp:39-56` | Yes (Turn directions inverted) | No | Accurate (Critical) |
| **BUG-20** | Dynamics | `main.cpp:92, 107, 123, 138, 153` | Yes (`VEL_BASE` used for turns) | No | Accurate (Medium) |
| **BUG-21** | Motor Driver | `puenteH.cpp:57-65` | Yes (`FRENO_F` coast instead of short brake)| No | Accurate (Medium) |
| **BUG-22** | Types | `puenteH.cpp:25, 35` | Yes (`int16_t` to `uint32_t` overflow)| No | Accurate (Medium) |
| **BUG-23** | Sensor Init | `sensoresDistancia.cpp:83` | Yes (`{0,0,0}` initial reading) | No | Accurate (High) |
| **BUG-24** | Robustness | `sensoresDistancia.cpp:45, 59, 73` | Yes (`while(true)` blocking loop) | No | Accurate (High) |
| **BUG-25** | Bus Speed | `sensoresDistancia.cpp:23` | Yes (10 kHz I2C bus clock) | No | Accurate (Medium) |
| **BUG-26** | Data Range | `sensoresDistancia.cpp:90-103` | Yes (Timeout 65535 capped as 1960 mm)| No | Accurate (High) |
| **BUG-27** | Bus Cycles | `main.cpp:56, 61` | Yes (Consecutive duplicate reads) | No | Accurate (Medium) |
| **BUG-28** | Strapping | `config.h:49` | Yes (GPIO 12 MTDI flash voltage) | No | Accurate (High) |
| **BUG-29** | Input Pin | `config.h:42`, `main.cpp:25` | Yes (GPIO 34 floating input) | No | Accurate (High) |

**Summary of False Positives:** **0 of 29 (0.0% false positive rate).**

---

## 4. Git Forensic Status & Source Code Immutability Verification

We conducted an empirical audit of the git working tree and file modification timestamps to verify compliance with the strict read-only constraint:

### Empirical Git Inspection:
```text
PS C:\Users\devandroid\Documents\GitHub\Laberinto\debugRobot> git status --porcelain .
 M debugRobot/src/config.h
 M debugRobot/src/main.cpp
 M debugRobot/src/main.h
?? debugRobot/.agents/
?? debugRobot/PROJECT.md
?? debugRobot/bug_report.md
?? "debugRobot/src/CIT\303\211.cpp"
```

### Forensic Timestamp Verification:
- First agent dispatch timestamp (`explorer_survey_1`): **`2026-10-05T11:33:53Z`**
- Last write times of modified files in repository:
  - `src/CITÉ.cpp`: `2026-10-05T10:57:57Z` (Created before audit dispatch)
  - `src/main.h`: `2026-10-05T11:18:06Z` (Modified before audit dispatch)
  - `src/config.h`: `2026-10-05T11:21:31Z` (Modified before audit dispatch)
  - `src/main.cpp`: `2026-10-05T11:33:40Z` (Modified before audit dispatch)
- Non-agent files created during teamwork execution:
  - `bug_report.md` (Created by `worker_report_1` at `2026-10-05T11:51:30Z`)
  - `PROJECT.md` (Created by `worker_report_1` at `2026-10-05T11:53:20Z`)

**Finding:** **Zero application source code files have been created, modified, or deleted by any agent.** The read-only constraint has been 100% adhered to.

---

## 5. Deliverable Requirements Conformance

Checking deliverables against `c:\Users\devandroid\Documents\GitHub\Laberinto\debugRobot\.agents\ORIGINAL_REQUEST.md`:

| Requirement | Expected | Actual | Pass/Fail |
|---|---|---|---|
| **R1. Análisis general sin modificaciones** | 0 files in source tree modified | 0 files modified by agents | **PASS** |
| **R2. Reporte de Bugs (`bug_report.md`)** | Exists in project root | `c:\Users\devandroid\Documents\GitHub\Laberinto\debugRobot\bug_report.md` exists (728 lines) | **PASS** |
| **AC1. Ubicación, Problema, Solución** | All 3 sections per finding | All 29 findings explicitly have `**Ubicación:**`, `**Problema:**`, `**Solución recomendada:**` | **PASS** |
| **AC2. Authenticity & Depth** | No placeholders, genuine analysis | Mathematical proofs, hardware references, code snippets provided | **PASS** |

---

## 6. Verdict

**FINAL VERDICT:** **APPROVE**  
The master bug report `bug_report.md` is technically rigorous, empirically sound, free of false positives or exaggerations, and strictly complies with all original user requirements.
