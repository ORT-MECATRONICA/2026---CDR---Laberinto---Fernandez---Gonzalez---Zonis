# Handoff Report — Empirical & Mathematical Adversarial Challenge of `audit_report.md`

**Agent ID:** `audit_challenger_2` (`teamwork_preview_challenger`, critic, specialist)  
**Parent Orchestrator:** `09e9fafb-0471-416d-a60f-89422422a6c2` (`orchestrator_2`)  
**Target Deliverable:** `c:\Users\devandroid\Documents\GitHub\Laberinto\debugRobot\audit_report.md`  
**Milestone:** M3 (Review, Challenge & Forensic Verification)  
**Verdict:** **APPROVE**  
**Date:** 2026-10-07  

---

## 1. Observation

### 1.1 Empirical Toolchain Execution (`pio run`)
The compilation toolchain was executed directly in a read-only manner from `c:\Users\devandroid\Documents\GitHub\Laberinto\debugRobot`:
- **Command:** `& "C:\Users\devandroid\.platformio\penv\Scripts\pio.exe" run`
- **Exit Code:** `0` (Success)
- **Execution Time:** 15.50 seconds
- **Observed Toolchain Output:**
  ```text
  Processing esp32doit-devkit-v1 (platform: espressif32; board: esp32doit-devkit-v1; framework: arduino)
  --------------------------------------------------------------------------------
  CONFIGURATION: https://docs.platformio.org/page/boards/espressif32/esp32doit-devkit-v1.html
  PLATFORM: Espressif 32 (6.13.0) > DOIT ESP32 DEVKIT V1
  HARDWARE: ESP32 240MHz, 320KB RAM, 4MB Flash
  Building in release mode
  Retrieving maximum program size .pio\build\esp32doit-devkit-v1\firmware.elf
  Checking size .pio\build\esp32doit-devkit-v1\firmware.elf
  RAM:   [=         ]  12.4% (used 40604 bytes from 327680 bytes)
  Flash: [========= ]  86.8% (used 1137453 bytes from 1310720 bytes)
  ========================= [SUCCESS] Took 15.50 seconds =========================
  ```
- **Comparison with `audit_report.md` Claims:**
  - `audit_report.md:28-40` & `audit_report.md:698-702` claim: Exit Code 0, RAM 12.4% ($40,604 / 327,680\text{ bytes}$), Flash 86.8% ($1,137,453 / 1,310,720\text{ bytes}$).
  - **Empirical Match:** Exact match (0.00% discrepancy).

### 1.2 Inspection of Differential Drive Kinematics (`puenteH.cpp:39-56` & `PROJECT.md:22-27`)
- **Pin Allocations (`PROJECT.md` & `config.h`):**
  - Motor A (Left): Direction pins `AIN1` (GPIO 14), `AIN2` (GPIO 4); PWM `PWMA` (GPIO 12, LEDC Ch 0).
  - Motor B (Right): Direction pins `BIN1` (GPIO 16), `BIN2` (GPIO 17); PWM `PWMB` (GPIO 32, LEDC Ch 1).
  - Forward direction (`AVANZAR`, lines 21–24): `AIN1=HIGH, AIN2=LOW` (Motor A forward) and `BIN1=HIGH, BIN2=LOW` (Motor B forward).
  - Reverse direction (`RETROCEDER`, lines 31–34): `AIN1=LOW, AIN2=HIGH` (Motor A reverse) and `BIN1=LOW, BIN2=HIGH` (Motor B reverse).
- **Turning Routines (`puenteH.cpp:39-56`):**
  - `GIRAR_DER` (lines 39–46):
    ```cpp
    case GIRAR_DER: {
      digitalWrite(AIN1, LOW);
      digitalWrite(AIN2, HIGH);
      digitalWrite(BIN1, HIGH);
      digitalWrite(BIN2, LOW);
      ledcWrite(0, velocidad.izquierda);
      ledcWrite(1, velocidad.derecha);
      break;
    }
    ```
    Motor A (Left) receives `AIN1=LOW, AIN2=HIGH` (REVERSE).  
    Motor B (Right) receives `BIN1=HIGH, BIN2=LOW` (FORWARD).
  - `GIRAR_IZQ` (lines 48–55):
    ```cpp
    case GIRAR_IZQ: {
      digitalWrite(AIN1, HIGH);
      digitalWrite(AIN2, LOW);
      digitalWrite(BIN1, LOW);
      digitalWrite(BIN2, HIGH);
      ledcWrite(0, velocidad.izquierda);
      ledcWrite(1, velocidad.derecha);
      break;
    }
    ```
    Motor A (Left) receives `AIN1=HIGH, AIN2=LOW` (FORWARD).  
    Motor B (Right) receives `BIN1=LOW, BIN2=HIGH` (REVERSE).

### 1.3 Inspection of Single-Wall PID Error Function (`PID.cpp:19-25` & `main.cpp:70-72`)
- **`PID.cpp:19-25`:**
  ```cpp
  } else if (hayIzq) {
      // Solo pared izquierda: mantenerse a la distancia ideal (OFSET_IZQ representa nuestro objetivo ideal)
      error = - (int16_t)mediciones.distanciaIzq;
  } else if (hayDer) {
      // Solo pared derecha
      error = (int16_t)mediciones.distanciaDer;
  }
  ```
- **`main.cpp:70-72`:**
  ```cpp
  velocidadActual.izquierda = constrain(VEL_BASE_IZQ + correccion, 0, 255);
  velocidadActual.derecha = constrain(VEL_BASE_DER - correccion, 0, 255);
  movimiento(AVANZAR, {velocidadActual.izquierda, velocidadActual.derecha});
  ```
- **`PID.cpp:31`:**
  ```cpp
  int16_t correccion = (KP * error) + (KD * (error - errorAnterior)) ;
  ```
  Where `KP = 0.5` and `KD = 0` (`config.h:13,15`).

### 1.4 Read-Only Compliance Verification
- Git status and file system timestamps:
  - Timestamp of `audit_report.md`: `2026-10-07 14:07:23`.
  - Timestamps of source files in `src/`: all precede `13:30:06` on `2026-10-07`.
  - No source files (`src/*` or `platformio.ini`) were modified during the audit session (M1, M2, or M3).
  - Read-only constraint is strictly maintained.

---

## 2. Logic Chain

### 2.1 Mathematical Challenge of Differential Drive Turning Polarity (`DEF-CRIT-04`)
Let the differential mobile robot have track width $W > 0$. Let $v_L$ and $v_R$ denote the longitudinal ground speeds of the left wheel (Motor A) and right wheel (Motor B), respectively.
In the robot body frame ($+x$ pointing forward, $+y$ pointing left):
1. The forward linear velocity is:
   $$v = \frac{v_R + v_L}{2}$$
2. The yaw angular velocity (counter-clockwise CCW being positive) is:
   $$\omega = \frac{v_R - v_L}{W}$$
3. A right turn in an egocentric navigation frame requires a **clockwise (CW)** rotation, which mathematically corresponds to:
   $$\omega < 0 \iff v_R < v_L$$
   For an on-the-spot rotation with turning speed $v_{turn} > 0$:
   $$v_L = +v_{turn}, \quad v_R = -v_{turn} \implies \omega = \frac{-v_{turn} - (+v_{turn})}{W} = -\frac{2 v_{turn}}{W} < 0 \quad (\text{Clockwise / Right Turn})$$
4. Evaluation of `GIRAR_DER` in `puenteH.cpp:39-46`:
   - Left Motor A is set to `AIN1=LOW, AIN2=HIGH` (matching `RETROCEDER`), so $v_L = -v_{turn} < 0$.
   - Right Motor B is set to `BIN1=HIGH, BIN2=LOW` (matching `AVANZAR`), so $v_R = +v_{turn} > 0$.
   - Computing angular velocity:
     $$\omega = \frac{(+v_{turn}) - (-v_{turn})}{W} = +\frac{2 v_{turn}}{W} > 0$$
   - Since $\omega > 0$, the rotation is **strictly counter-clockwise (CCW)**, which physically rotates the chassis to the **LEFT**.
5. Evaluation of `GIRAR_IZQ` in `puenteH.cpp:48-55`:
   - Left Motor A is set to `AIN1=HIGH, AIN2=LOW` (matching `AVANZAR`), so $v_L = +v_{turn} > 0$.
   - Right Motor B is set to `BIN1=LOW, BIN2=HIGH` (matching `RETROCEDER`), so $v_R = -v_{turn} < 0$.
   - Computing angular velocity:
     $$\omega = \frac{(-v_{turn}) - (+v_{turn})}{W} = -\frac{2 v_{turn}}{W} < 0$$
   - Since $\omega < 0$, the rotation is **strictly clockwise (CW)**, which physically rotates the chassis to the **RIGHT**.
6. **Kinematic Inversion Proof:**  
   `GIRAR_DER` produces an angular velocity $\omega > 0$ (Left), and `GIRAR_IZQ` produces $\omega < 0$ (Right). Both routines command the exact opposite physical yaw rotation to their state machine identifiers. The finding `DEF-CRIT-04` in `audit_report.md` is mathematically indisputable.

### 2.2 Mathematical Challenge of Single-Wall PID Steering Behavior (`DEF-CRIT-03`)
In `main.cpp:70-71`, the closed-loop motor velocities during forward travel are:
$$v_L = v_{base} + c, \quad v_R = v_{base} - c$$
where $c$ is the PID output `correccion`.
The resulting yaw rate is:
$$\omega = \frac{v_R - v_L}{W} = \frac{(v_{base} - c) - (v_{base} + c)}{W} = -\frac{2c}{W}$$
Thus:
- If $c > 0 \implies \omega < 0$ (Robot steers **RIGHT**).
- If $c < 0 \implies \omega > 0$ (Robot steers **LEFT**).

Given `KP = 0.5 > 0` and `KD = 0`, the correction is:
$$c = 0.5 \cdot \text{error}$$
Therefore:
$$\text{error} > 0 \implies c > 0 \implies \text{Steer RIGHT}$$
$$\text{error} < 0 \implies c < 0 \implies \text{Steer LEFT}$$

Now evaluate single-wall modes in `PID.cpp:19-25`:
1. **Case: Only Left Wall Present (`hayIzq && !hayDer`):**
   - Code executes: `error = - (int16_t)mediciones.distanciaIzq;`
   - Since a wall is present, $d_{izq} > 0$ (e.g. $40\text{ mm}$).
   - Therefore, $\text{error} = -d_{izq} < 0$ for all physical distances.
   - For $d_{izq} = 40\text{ mm}$: $\text{error} = -40 \implies c = -20$.
   - Motor outputs: $v_L = 45 + (-20) = 25$, $v_R = 45 - (-20) = 65$.
   - Result: $v_R > v_L \implies \omega > 0 \implies$ **The robot steers LEFT**.
   - **Instability / Collision Proof:** The robot is already tracking the left wall at $40\text{ mm}$. Instead of steering right away from the wall or maintaining distance, the controller commands a hard left steering torque directly toward the left wall until collision. Furthermore, since no target setpoint $D_{target}$ is subtracted, $\text{error}$ is never zero for any non-zero distance.
2. **Case: Only Right Wall Present (`hayDer && !hayIzq`):**
   - Code executes: `error = (int16_t)mediciones.distanciaDer;`
   - Since $d_{der} > 0$ (e.g. $40\text{ mm}$), $\text{error} = +d_{der} > 0$ for all physical distances.
   - For $d_{der} = 40\text{ mm}$: $\text{error} = +40 \implies c = +20$.
   - Motor outputs: $v_L = 45 + 20 = 65$, $v_R = 45 - 20 = 25$.
   - Result: $v_L > v_R \implies \omega < 0 \implies$ **The robot steers RIGHT**.
   - **Instability / Collision Proof:** The robot is tracking the right wall and steers directly into the right wall. This represents 100% positive feedback with no equilibrium point.
3. **Correct Setpoint Reference Comparison:**
   - To stabilize tracking at distance $D_{target} \approx 45\text{ mm}$:
     - Left wall: $\text{error} = D_{target} - d_{izq}$. If $d_{izq} < D_{target}$ (too close), $\text{error} > 0 \implies c > 0 \implies$ steer RIGHT (away from wall).
     - Right wall: $\text{error} = d_{der} - D_{target}$. If $d_{der} < D_{target}$ (too close), $\text{error} < 0 \implies c < 0 \implies$ steer LEFT (away from wall).
   - The implementation in `PID.cpp:19-25` completely omits $D_{target}$ and assigns signs that destabilize the robot into the detected wall. Finding `DEF-CRIT-03` is mathematically proven.

### 2.3 Additional Adversarial Stress Verifications
- **PID 50 ms Chattering (`DEF-CRIT-05`):**
  - Line 7 defines `static int16_t correccionAnterior = 0;`.
  - Lines 30–37 return `correccionAnterior` when `tiempoActual - tiempoAnterior <= 50`.
  - In `PID.cpp`, `correccionAnterior` is never assigned.
  - Therefore, during 4 out of every 5 iterations in a 10 ms loop, the function returns 0. The steering output oscillates between calculated correction and 0 at 20 Hz, completely verifying `DEF-CRIT-05`.
- **`PREGIRO_IZQ` Front Wall Collision (`DEF-CRIT-02`):**
  - Transition into `PREGIRO_IZQ` occurs when `distanciaCent < 130` (`main.cpp:76`).
  - Once in `PREGIRO_IZQ`, `main.cpp:119-121` commands `movimiento(AVANZAR, {45, 45})` until `pulsosActuales >= 280` ($\approx 63\text{ mm}$).
  - Advancing forward when facing an obstacle already within $50\text{ mm} - 130\text{ mm}$ causes a frontal crash before the left rotation begins, verifying `DEF-CRIT-02`.

---

## 3. Caveats

- **Alternative Hardware Mounting Assumptions:** If a hardware developer had swapped motor polarity at the physical terminal connector or swapped channels 0 and 1, the turning behavior would change. However, `PROJECT.md` is the canonical ground truth: Motor A (GPIO 14, 4, PWM 12) is documented as Left Motor, and Motor B (GPIO 16, 17, PWM 32) is Right Motor. The firmware code itself in `puenteH.cpp` sets `AVANZAR` as `AIN1=HIGH, AIN2=LOW` and `BIN1=HIGH, BIN2=LOW`. Relative to this forward reference, `GIRAR_DER` reverses Motor A and advances Motor B, which is an unequivocal counter-clockwise yaw command.
- **Physical Wheel Friction & Slip:** Real-world rotational drift and tire slippage will introduce non-ideal trajectory distortion, but they cannot invert the sign of the motor torque vector.

---

## 4. Conclusion

1. **Empirical Verification:** The PlatformIO compilation succeeds with Exit Code 0, consuming exactly 12.4% RAM (40,604 bytes) and 86.8% Flash (1,137,453 bytes). All toolchain metrics reported in `audit_report.md` are 100% accurate.
2. **Kinematic Polarity:** Differential drive kinematics rigorously proves that `puenteH.cpp:39-56` inverts `GIRAR_DER` (produces CCW / Left turn) and `GIRAR_IZQ` (produces CW / Right turn).
3. **Control Theory:** Single-wall PID tracking in `PID.cpp:19-25` is mathematically proven to be a positive feedback destabilizing controller that drives the robot directly into whichever wall is present.
4. **Read-Only Integrity:** Repository status confirms zero unauthorized changes to source code.
5. **Final Verdict:** **APPROVE**. `audit_report.md` is mathematically and empirically validated, highly thorough, and ready for deployment.

---

## 5. Verification Method

To independently reproduce the adversarial challenge:
1. **Toolchain Execution:**
   ```powershell
   & "C:\Users\devandroid\.platformio\penv\Scripts\pio.exe" run
   ```
   *Expected:* Exit code 0, RAM 12.4%, Flash 86.8%.
2. **Kinematic Code Inspection:**
   ```powershell
   Select-String -Path "src\hardware\movimiento\puenteH.cpp" -Pattern "AIN1|AIN2|BIN1|BIN2"
   ```
   Compare lines 21–24 (`AVANZAR`) with lines 40–43 (`GIRAR_DER`). Observe Motor A (`AIN`) is reversed and Motor B (`BIN`) is forward in `GIRAR_DER`.
3. **PID Steering Math Inspection:**
   ```powershell
   Select-String -Path "src\hardware\movimiento\PID.cpp" -Pattern "error = "
   ```
   Observe lines 21 and 24 lack setpoint subtraction and feature inverted signs.
4. **Read-Only Verification:**
   ```powershell
   git status --short
   ```
   Verify no modifications were introduced by the audit agents into `src/`.
