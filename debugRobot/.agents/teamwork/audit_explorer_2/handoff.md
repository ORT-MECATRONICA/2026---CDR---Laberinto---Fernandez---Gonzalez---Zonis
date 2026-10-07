# Handoff Report — Audit Explorer 2: Motor Driving, PID Control, Encoders & Kinematics

**Agent ID**: `audit_explorer_2`  
**Milestone**: Comprehensive Read-Only Firmware Audit  
**Target Subsystems**:
- PID Control (`src/hardware/movimiento/PID.cpp`, `src/hardware/movimiento/PID.h`)
- H-Bridge Motor Driving (`src/hardware/movimiento/puenteH.cpp`, `src/hardware/movimiento/puenteH.h`)
- Encoders & Odometry (`src/hardware/encoders/encoders.cpp`, `src/hardware/encoders/encoders.h`)
- Kinematics & Requirements Compliance (`src/config.h`, `src/main.cpp`, `ORIGINAL_REQUEST.md`)

---

## 1. Observation

Direct code inspections, compiler diagnostics, and file contents observed:

### 1.1 PID Control Subsystem (`src/hardware/movimiento/PID.h` and `PID.cpp`)

1. **Orphaned Declaration in Header (`src/hardware/movimiento/PID.h:5`)**:
   ```cpp
   4: int16_t calcularCorreccion(sensado mediciones);
   5: int16_t calcularCorreccionRightHand(int16_t error);
   6: void resetearErrorAnterior();
   ```
   In `src/hardware/movimiento/PID.cpp`, `calcularCorreccionRightHand` is NOT implemented. A git diff reveals it was deleted from `PID.cpp`, leaving an orphaned function prototype in `PID.h`.

2. **Inverted Polarity and Missing Setpoint Reference in Single-Wall Tracking (`src/hardware/movimiento/PID.cpp:19-24`)**:
   ```cpp
   19:     } else if (hayIzq) {
   20:         // Solo pared izquierda: mantenerse a la distancia ideal (OFSET_IZQ representa nuestro objetivo ideal)
   21:         error = - (int16_t)mediciones.distanciaIzq;
   22:     } else if (hayDer) {
   23:         // Solo pared derecha
   24:         error = (int16_t)mediciones.distanciaDer;
   25:     }
   ```
   No target setpoint (e.g. `DISTANCIA_OBJETIVO_PARED`) is subtracted.
   In `src/main.cpp:70-71`:
   ```cpp
   70:       velocidadActual.izquierda = constrain(VEL_BASE_IZQ + correccion, 0, 255);
   71:       velocidadActual.derecha = constrain(VEL_BASE_DER - correccion, 0, 255);
   ```
   `correccion > 0` speeds up left and slows down right (turns RIGHT). `correccion < 0` speeds up right and slows down left (turns LEFT).

3. **`correccionAnterior` Is Never Updated, Causing 50 ms Intervals of Zero Correction / Chattering (`src/hardware/movimiento/PID.cpp:7, 29-37`)**:
   ```cpp
   7: static int16_t correccionAnterior = 0;
   ...
   29:     int32_t tiempoActual = millis();
   30:     if (tiempoActual - tiempoAnterior > 50) {
   31:         int16_t correccion = (KP * error) + (KD * (error - errorAnterior)) ;
   32:         errorAnterior = error; 
   33:         tiempoAnterior = tiempoActual;
   34:         return constrain(correccion, -25,25); //CUIDADO CON ESTE
   35:     } else {
   36:         return correccionAnterior;
   37:     }
   ```
   `correccionAnterior` is initialized to 0 and is NEVER assigned to `correccion` anywhere in the file. Whenever `tiempoActual - tiempoAnterior <= 50`, line 36 returns 0.

4. **Derivative Term Lacks Time Normalization ($\Delta t$) (`src/hardware/movimiento/PID.cpp:31`)**:
   ```cpp
   31:         int16_t correccion = (KP * error) + (KD * (error - errorAnterior)) ;
   ```
   The difference `(error - errorAnterior)` is not divided by elapsed time $\Delta t$. In `src/config.h:15`:
   ```cpp
   15: #define KD 0 //SI LE AGREGO ALGO NO VA A FUNCIONAR!!
   ```

5. **Incomplete State Reset in `resetearErrorAnterior()` (`src/hardware/movimiento/PID.cpp:43-45`)**:
   ```cpp
   43: void resetearErrorAnterior() {
   44:     errorAnterior = 0;
   45: }
   ```
   `tiempoAnterior` and `correccionAnterior` are not reset. In `src/main.cpp:109, 125, 145, 163, 178, 194`, `resetearErrorAnterior()` is called on every state exit.

6. **Static File-Scope Initialization of `tiempoAnterior = millis()` (`src/hardware/movimiento/PID.cpp:6`)**:
   ```cpp
   6: static int32_t tiempoAnterior = millis();
   ```
   Declared as signed `int32_t` and initialized at static C++ construct time before Arduino setup.

7. **Sensor Offset Asymmetry Induces 7 mm Center Error (`src/hardware/movimiento/PID.cpp:18`, `src/config.h:22-23`, `src/hardware/sensoresDistancia/sensoresDistancia.cpp:89, 99`)**:
   ```cpp
   // config.h:22-23
   #define OFSET_DER 47
   #define OFSET_IZQ 40
   // sensoresDistancia.cpp:89, 99
   lecturaAct.distanciaIzq = rawIzq - OFSET_IZQ;
   lecturaAct.distanciaDer = rawDer - OFSET_DER;
   // PID.cpp:18
   error = (int16_t)mediciones.distanciaDer - (int16_t)mediciones.distanciaIzq;
   ```
   When `rawDer == rawIzq`, `error = (rawDer - 47) - (rawIzq - 40) = -7 mm`.

---

### 1.2 H-Bridge Motor Driver Subsystem (`src/hardware/movimiento/puenteH.h` and `puenteH.cpp`)

1. **Inverted Turning Polarities for `GIRAR_DER` and `GIRAR_IZQ` (`src/hardware/movimiento/puenteH.cpp:39-56`)**:
   ```cpp
   39:     case GIRAR_DER: {
   40:       digitalWrite(AIN1, LOW);
   41:       digitalWrite(AIN2, HIGH); // Motor A (Izquierdo) -> RETROCESO
   42:       digitalWrite(BIN1, HIGH);
   43:       digitalWrite(BIN2, LOW);  // Motor B (Derecho) -> AVANCE
   44:       ledcWrite(0, velocidad.izquierda);
   45:       ledcWrite(1, velocidad.derecha);
   46:       break;
   47:     }
   48:     case GIRAR_IZQ: {
   49:       digitalWrite(AIN1, HIGH);
   50:       digitalWrite(AIN2, LOW);  // Motor A (Izquierdo) -> AVANCE
   51:       digitalWrite(BIN1, LOW);
   52:       digitalWrite(BIN2, HIGH); // Motor B (Derecho) -> RETROCESO
   53:       ledcWrite(0, velocidad.izquierda);
   54:       ledcWrite(1, velocidad.derecha);
   55:       break;
   56:     }
   ```
   Compared with `AVANZAR` (lines 21–24: `AIN1=HIGH, AIN2=LOW, BIN1=HIGH, BIN2=LOW`), `GIRAR_DER` reverses left wheel and advances right wheel (counter-clockwise / turn left). `GIRAR_IZQ` advances left wheel and reverses right wheel (clockwise / turn right).

2. **Implicit Signed-to-Unsigned 32-bit Conversion in `ledcWrite` (`src/hardware/movimiento/puenteH.cpp:25-26, 35-36, 44-45, 53-54`)**:
   `struct VELOCIDAD` in `puenteH.h:12-15` uses `int16_t`. `ledcWrite(uint8_t chan, uint32_t duty)` takes unsigned 32-bit integers. Negative speeds implicitly cast to `4,294,967,xxx` on an 8-bit timer (0–255). No internal clamping is performed in `puenteH.cpp`.

3. **Missing `default:` Label in `switch (movimiento)` (`src/hardware/movimiento/puenteH.cpp:19-66`)**:
   The switch does not contain a `default:` case for unexpected enum values.

4. **Missing Safe Initial Level and Zero PWM in `inicializarMotores()` (`src/hardware/movimiento/puenteH.cpp:6-16`)**:
   ```cpp
   6: void inicializarMotores(){
   7:     pinMode(AIN1, OUTPUT);
   8:     pinMode(AIN2, OUTPUT);
   9:     pinMode(BIN1, OUTPUT);
   10:     pinMode(BIN2, OUTPUT);
   11:     
   12:   ledcSetup(0, 20000, 8);
   13:   ledcSetup(1, 20000, 8);
   14:   ledcAttachPin(PWMA, 0);
   15:   ledcAttachPin(PWMB, 1);
   16: }
   ```
   `digitalWrite` is not called, leaving pins indeterminate on reset, and `ledcWrite(0, 0)` / `ledcWrite(1, 0)` are omitted.

5. **`FRENO_F` Hardcodes PWM Duty 40 and Ignores Passed Speed (`src/hardware/movimiento/puenteH.cpp:57-65`)**:
   ```cpp
   57:     case FRENO_F: {
   58:       digitalWrite(AIN1, HIGH);
   59:       digitalWrite(AIN2, HIGH);
   60:       digitalWrite(BIN1, HIGH);
   61:       digitalWrite(BIN2, HIGH);
   62:       ledcWrite(0, 40);
   63:       ledcWrite(1, 40);
   64:       break;
   65:     }
   ```
   Both inputs HIGH configures short-circuit brake, but duty cycle is hardcoded to 40 (15.7%), severely limiting braking power and causing current draw during idle `LISTO` state.

6. **Strapping Pin Hazard on GPIO 12 (`PWMA`) (`src/config.h:49`, `src/hardware/movimiento/puenteH.cpp:14`)**:
   GPIO 12 is MTDI strapping pin on ESP32. If high at boot, flash LDO falls to 1.8V, triggering bootloop.

---

### 1.3 Encoder & Odometry Subsystem (`src/hardware/encoders/encoders.h` and `encoders.cpp`)

1. **Asymmetric Single-Wheel Odometry in Rotation States Without Timeouts (`src/main.cpp:134-138, 152-156, 186-189`)**:
   ```cpp
   // main.cpp
   134: pulsosActuales = abs(verPulsosEncoderA()); // GIRANDO_DER only reads Left
   152: pulsosActuales = abs(verPulsosEncoderB()); // GIRANDO_IZQ only reads Right
   186: pulsosActuales = abs(verPulsosEncoderA()); // GIRANDO_180 only reads Left
   // Timeout lines 137 and 155 are commented out:
   // bool cortePorTiempoDer = (millis() - tiempoInicioGiro) < 2000;
   ```
   If the monitored wheel slips or stalls, the robot spins indefinitely.

2. **64-bit to 32-bit Integer Narrowing (`src/hardware/encoders/encoders.cpp:18-24`, `encoders.h:8, 11`)**:
   `ESP32Encoder::getCount()` returns `int64_t`, narrowed to `int32_t`.

3. **`abs()` Masking Direction and Inability to Detect Backwards Motion (`src/main.cpp:67, 101, 117, 170`)**:
   `pulsosActuales = (abs(verPulsosEncoderA()) + abs(verPulsosEncoderB())) / 2;`
   If the robot rolls backward upon braking or recoil, `abs()` accumulates positive distance, misrepresenting travel.

---

### 1.4 Kinematics & Requirements Compliance

1. **Requirement R3 (Grace Period / PID Blindness) Unimplemented (`ORIGINAL_REQUEST.md:24-25`, `src/config.h:70`, `src/main.cpp:66-72`)**:
   `PULSOS_GRACIA_PID 100` is defined in `config.h:70`, but `src/main.cpp` executes `calcularCorreccion` from pulse 0 of `AVANZANDO`.
2. **Requirements R1 & R2 Unimplemented (`ORIGINAL_REQUEST.md:18-23`, `src/main.cpp:74-97`)**:
   Falling edge detection for cell centering (400 pulses) and front wall 50 mm approach alignment are absent from `main.cpp`.
3. **Turn Velocity Insufficient (`src/main.cpp:139, 157, 189`, `src/config.h:17-18`)**:
   Rotation states command `{VEL_BASE_IZQ, VEL_BASE_DER}` (PWM 45 = 17.6% duty) instead of `VEL_GIRO` (100). Motors risk stall under static friction.

---

## 2. Logic Chain

1. **PID Single Wall Failure Chain**:
   - `PID.cpp:21` computes `error = - (int16_t)mediciones.distanciaIzq;`.
   - Any valid measurement gives positive distance (e.g., 40 mm), yielding `error = -40`.
   - In `main.cpp:70-71`, `velIzq = VEL_BASE_IZQ + correccion` and `velDer = VEL_BASE_DER - correccion`.
   - With $K_p = 0.5$, $\text{correccion} = -20$. Left wheel slows to 25, right wheel speeds to 65.
   - Differential kinematics dictates the robot turns towards the slower wheel (Left).
   - Therefore, detecting only the left wall commands the robot to turn into the left wall. The closer it gets, the persistently negative error continues steering left until impact.
   - Symmetric reasoning applies to `PID.cpp:24`: detecting only the right wall gives `error = +distanciaDer`, steering the robot directly into the right wall.

2. **PID 50 ms Chattering Defect Chain**:
   - `PID.cpp:7` initializes `static int16_t correccionAnterior = 0;`.
   - In `PID.cpp:30-37`, when `tiempoActual - tiempoAnterior > 50`, `correccion` is computed and returned directly without updating `correccionAnterior`.
   - In all subsequent iterations where `tiempoActual - tiempoAnterior <= 50`, line 36 executes `return correccionAnterior;`, returning 0.
   - In a loop executing every 5–10 ms, the PID output is 0 for 90% of the cycles, punctuated by an isolated single-cycle impulse every 50 ms.
   - This destroys continuous control, induces motor PWM jitter, and renders the robot incapable of maintaining heading.

3. **Puente H Inverted Kinematics Chain**:
   - `puenteH.cpp:20-25` sets `AIN1=HIGH, AIN2=LOW, BIN1=HIGH, BIN2=LOW` for `AVANZAR`.
   - This defines the forward direction for Left (Motor A) and Right (Motor B).
   - In `GIRAR_DER` (lines 40–43), `AIN1=LOW, AIN2=HIGH` (Left backwards) and `BIN1=HIGH, BIN2=LOW` (Right forward).
   - In a differential drive robot, rotating the left wheel backwards while moving the right wheel forward produces counter-clockwise rotation about the center of mass (turning LEFT).
   - Thus, any request for `GIRAR_DER` turns the robot left.

4. **Encoder State Machine Deadlock Chain**:
   - In `main.cpp:134, 152, 186`, rotation transitions evaluate only one encoder (`abs(verPulsosEncoderA())` or `B()`).
   - Timeout guards in lines 137 and 155 are commented out.
   - If the single monitored wheel experiences slip, high friction, or connection fault, `pulsosActuales` never reaches `PULSOS_GIRO_90_DER` / `IZQ` / `180`.
   - The state machine remains trapped in the rotation state, continuously powering the motors with no exit condition.

---

## 3. Caveats

1. **Hardware Verification**: Static analysis was performed strictly read-only. Physical PCB wiring of motor phase leads and encoder channel pins could theoretically have been inverted in hardware to compensate for software inversions; however, `AVANZAR` drives both motors forward symmetrically, proving `GIRAR_DER`/`GIRAR_IZQ` are software sign errors.
2. **PlatformIO Build Output**: PlatformIO build succeeds (`RAM 12.4%`, `Flash 86.8%`). Compilation success does not catch logic errors, orphan function declarations that are never linked, or inverted motor polarities.
3. **Bluetooth/UART Interactions**: Scope was restricted to PID, Puente H, Encoders, Kinematics, and their main.cpp/config.h interfaces. Full ToF driver internals and Bluetooth telemetry buffer details were referenced only where they affect PID sampling rates and loop jitter.

---

## 4. Conclusion

A comprehensive code audit of `PID.cpp/h`, `puenteH.cpp/h`, `encoders.cpp/h`, and their control interactions reveals **12 distinct defects** across these modules:

### Defect Matrix

| ID | File & Line | Severity | Summary | Dynamic Impact |
|---|---|:---:|---|---|
| **AUD-01** | `src/hardware/movimiento/PID.cpp:19-24` | **CRITICAL** | Single-wall PID lacks setpoint and has inverted polarity | Robot veers into whatever single wall is detected until physical crash. |
| **AUD-02** | `src/hardware/movimiento/PID.cpp:7, 30-37` | **CRITICAL** | `correccionAnterior` never updated; returns 0 for 50 ms | Severe PWM chattering; 90% of loop cycles produce 0 correction. |
| **AUD-03** | `src/hardware/movimiento/puenteH.cpp:39-56` | **CRITICAL** | Inverted motor polarities in `GIRAR_DER` and `GIRAR_IZQ` | Robot turns left on right turn commands and vice versa. |
| **AUD-04** | `src/hardware/movimiento/puenteH.cpp:25, 35, 44, 53` | **HIGH** | Implicit negative `int16_t` conversion to `uint32_t` in `ledcWrite` | Negative speed converts to $4.29 \times 10^9$, corrupting LEDC registers. |
| **AUD-05** | `src/config.h:49`, `src/hardware/movimiento/puenteH.cpp:14` | **HIGH** | GPIO 12 (`PWMA`) is MTDI strapping pin | Bootloop hazard if pulled high at power-up (flash voltage to 1.8V). |
| **AUD-06** | `src/main.cpp:134-138, 152-156`, `encoders.cpp` | **HIGH** | Single-wheel encoder monitoring in turns without timeouts | Infinite spin deadlock if monitored wheel slips or stalls. |
| **AUD-07** | `src/hardware/movimiento/PID.cpp:30-33` | **HIGH** | Derivative term lacks $\Delta t$ normalization | Loop jitter causes erratic derivative spikes; $K_d$ forced to 0. |
| **AUD-08** | `src/hardware/movimiento/PID.cpp:43-45` | **HIGH** | `resetearErrorAnterior()` does not reset `tiempoAnterior` | Violent derivative kick on first PID cycle entering `AVANZANDO`. |
| **AUD-09** | `src/hardware/movimiento/PID.cpp:18`, `config.h:22-23` | **HIGH** | Static offset asymmetry (40 vs 47 mm) | Continuous -7 mm error bias pulls robot to the left in centered corridors. |
| **AUD-10** | `src/config.h:70`, `src/main.cpp:66-72` | **HIGH** | R3 (100-pulse PID grace period) unimplemented | Post-turn corner clipping caused by premature PID correction. |
| **AUD-11** | `src/hardware/movimiento/puenteH.cpp:57-65` | **MEDIUM** | `FRENO_F` hardcodes duty 40 and ignores speed parameter | Weak active braking (~16% torque); continuous power drain in `LISTO`. |
| **AUD-12** | `src/hardware/movimiento/puenteH.cpp:19-66`, `6-16` | **MEDIUM** | Missing `default:` and missing safe GPIO/PWM init | Uncontrolled motor state upon corrupt enum; reset glitch impulses. |
| **AUD-13** | `src/hardware/movimiento/PID.h:5` | **MEDIUM** | Orphaned `calcularCorreccionRightHand` declaration | Linker failure if invoked by external callers. |
| **AUD-14** | `src/hardware/encoders/encoders.cpp:18-24`, `main.cpp:67` | **MEDIUM** | `abs()` on encoders masks backwards recoil | Backward roll incorrectly accumulates as forward travel distance. |

---

## 5. Verification Method

To independently verify these findings:

1. **Verify Compilation and Partition Size**:
   ```powershell
   & "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run
   ```
   *Expected outcome*: Passes compilation, showing Flash at 86.8%, confirming lack of compiler warnings on logic bugs and orphaned header declarations.

2. **Inspect PID Polarity Mathematically**:
   Inspect `src/hardware/movimiento/PID.cpp:21`:
   - Pass `mediciones = {distanciaCent: 200, distanciaDer: 200, distanciaIzq: 40}`.
   - Evaluates `hayIzq = true`, `error = -40`.
   - `correccion = 0.5 * (-40) = -20`.
   - In `main.cpp:70-71`: `velIzq = 45 + (-20) = 25`, `velDer = 45 - (-20) = 65`.
   - Right wheel is faster than Left wheel $\implies$ turns LEFT into the left wall.

3. **Inspect Puente H Polarity Kinematically**:
   Inspect `src/hardware/movimiento/puenteH.cpp:40-43`:
   - `GIRAR_DER`: `AIN1=LOW, AIN2=HIGH` (reverse left) and `BIN1=HIGH, BIN2=LOW` (forward right).
   - In differential robotics, this is counter-clockwise rotation (left).

4. **Inspect `correccionAnterior` Stagnation**:
   Inspect `src/hardware/movimiento/PID.cpp:30-37`:
   - Search for all occurrences of `correccionAnterior`. Line 7 initializes to 0. Line 36 returns `correccionAnterior`. No write exists in lines 1–46.

5. **Invalidation Conditions**:
   - The PID single-wall finding would be invalidated only if `velocidadActual.izquierda` subtracted correction and `derecha` added correction in `main.cpp`. (Inspection confirmed line 70 adds and line 71 subtracts).
   - The Puente H polarity finding would be invalidated only if the Left motor hardware wiring was inverted relative to the Right motor hardware wiring, and `AVANZAR` used opposing pin levels. (Inspection confirmed lines 21–24 use identical `HIGH, LOW` pin pairs).
