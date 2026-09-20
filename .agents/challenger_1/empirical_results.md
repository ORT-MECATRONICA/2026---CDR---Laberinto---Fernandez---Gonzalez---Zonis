# Empirical Stress Test & Adversarial Analysis Results

**Author**: Challenger 1 (`teamwork_preview_challenger` / `challenger_1`)  
**Target Subsystems**: Median Filter (`sensoresDistancia.cpp`), PID Controller (`PID.cpp`, `config.h`)  
**Date**: 2026-09-20  

---

## 1. Stress Test: Median Filter Logic (`sensoresDistancia.cpp`)

### 1.1 Input Sequence: `[0, 2000, 50, 50, 50]`
- **Hypothesis**: Can a rolling median filter of order $N=5$ withstand dual-sided catastrophic outlier corruption (simultaneous drop to 0 mm and spike to 2000 mm) without corrupting the filtered output?
- **Mathematical Principle**: The breakdown point of a median filter of size $N$ is $\lfloor \frac{N-1}{2} \rfloor$. For $N=5$, the breakdown point is $\lfloor \frac{4}{2} \rfloor = 2$. It can tolerate up to 2 arbitrary corrupted samples within any 5-sample sliding window.
- **Cycle-by-Cycle Execution Trace (Primed Buffer at 50 mm)**:
  - Initial state after `prellenarFiltro(f, 50)`:
    `buffer = [50, 50, 50, 50, 50]`, `count = 5`, `index = 0`, `valorMediana = 50`.
  - **Sample 1 (`new = 0`)**:
    - `buffer = [0, 50, 50, 50, 50]`, `index = 1`.
    - Stack array `temp` sorted: `[0, 50, 50, 50, 50]`.
    - `valorMediana = temp[5 / 2] = temp[2] = 50`.
    - Result: **0 is completely rejected**.
  - **Sample 2 (`new = 2000`)**:
    - `buffer = [0, 2000, 50, 50, 50]`, `index = 2`.
    - Stack array `temp` sorted: `[0, 50, 50, 50, 2000]`.
    - `valorMediana = temp[2] = 50`.
    - Result: **Both 0 and 2000 are simultaneously rejected**. Output remains strictly 50.
  - **Sample 3 (`new = 50`)**:
    - `buffer = [0, 2000, 50, 50, 50]`, `index = 3`.
    - Sorted `temp`: `[0, 50, 50, 50, 2000]`.
    - `valorMediana = temp[2] = 50`.
  - **Sample 4 (`new = 50`)**:
    - `buffer = [0, 2000, 50, 50, 50]`, `index = 4`.
    - Sorted `temp`: `[0, 50, 50, 50, 2000]`.
    - `valorMediana = temp[2] = 50`.
  - **Sample 5 (`new = 50`)**:
    - `buffer = [0, 2000, 50, 50, 50]`, `index = 0`.
    - Sorted `temp`: `[0, 50, 50, 50, 2000]`.
    - `valorMediana = temp[2] = 50`.
- **Conclusion**: Filter demonstrates maximum theoretical robustness. Output remains constant at 50 mm throughout the entire sequence despite severe two-sided impulse noise.

---

### 1.2 Loop Rate Mismatch & Sample Flooding (Slow Readings vs Fast Loop)
- **Scenario**: The main Arduino `loop()` runs at tens of kHz (cycle time $< 100\ \mu\text{s}$), whereas the VL53L0X ToF sensor has a physical photon integration and conversion period of $20 - 33\ \text{ms}$.
- **Vulnerability Investigated**: If `actualizarSensado()` is called thousands of times while no new reading is ready, does the median filter flood with duplicate samples, causing stale noise or wiping historical values?
- **Code Gating Mechanism (`sensoresDistancia.cpp:140, 146, 152`)**:
  ```cpp
  if((sensorIzq.readReg(VL53L0X::RESULT_INTERRUPT_STATUS) & 0x07) != 0) { ... }
  if((sensorCent.readReg(VL53L0X::RESULT_INTERRUPT_STATUS) & 0x07) != 0) { ... }
  if((sensorDer.readReg(VL53L0X::RESULT_INTERRUPT_STATUS) & 0x07) != 0) { ... }
  ```
- **Analysis**:
  1. The VL53L0X register `0x13` (`RESULT_INTERRUPT_STATUS`) bits `[2:0]` are non-zero ONLY when a new ranging measurement has completed and is latched in the device output register.
  2. During the 30 ms interval between ranging events, `(readReg(...) & 0x07)` evaluates to `0`.
  3. The `if` body is skipped completely.
  4. `actualizarFiltroMediana()` is **NOT** invoked. `f.buffer`, `f.index`, and `f.count` remain untouched.
  5. `lecturaAct` retains the existing filtered value, returning it safely to the FSM.
- **Conclusion**: Sample flooding is **100% prevented** by hardware register status gating.

---

### 1.3 Cold Start Transient & `prellenarFiltro`
- **Scenario**: At microcontroller power-on / boot, the static structs `filtroDer, filtroCent, filtroIzq` are initialized to 0. If navigation starts before $N=5$ readings are collected, uninitialized 0s would dominate the median.
- **Implementation in `sensoresDistancia.cpp:121-136`**:
  ```cpp
  uint16_t rawInitDer = sensorDer.readRangeContinuousMillimeters();
  if (rawInitDer > DISTANCIA_MAX_VALIDA) rawInitDer = DISTANCIA_MAX_VALIDA;
  prellenarFiltro(filtroDer, rawInitDer);
  ...
  static void prellenarFiltro(FiltroMediana &f, uint16_t valorInicial) {
      for (uint8_t i = 0; i < FILTRO_MEDIANA_N; i++) {
          f.buffer[i] = valorInicial;
      }
      f.index = 0;
      f.count = FILTRO_MEDIANA_N;
      f.valorMediana = valorInicial;
  }
  ```
- **Analysis**:
  - `prellenarFiltro` fills all 5 slots with the first real measurement, sets `count = 5`, and calculates initial `valorMediana`.
  - `lecturaAct` is initialized with valid offset-subtracted distances before `setup()` finishes.
  - When `right_hand()` begins, there are zero 0-dips.
- **Conclusion**: `prellenarFiltro` completely prevents cold-start dips.

---

### 1.4 Stack-Allocated Insertion Sort: Bounds, Memory & Duplicates
- **Sorting Implementation (`sensoresDistancia.cpp:39-48`)**:
  ```cpp
  for (uint8_t i = 1; i < f.count; i++) {
      uint16_t key = temp[i];
      int8_t j = i - 1;
      while (j >= 0 && temp[j] > key) {
          temp[j + 1] = temp[j];
          j--;
      }
      temp[j + 1] = key;
  }
  ```
- **Boundary Verification**:
  - **Signedness of `j`**: Declared as `int8_t` (signed). When `i = 1` and `j = 0` decrements, $j$ becomes $-1$. Because `j` is signed, condition `j >= 0` evaluates to `false` and short-circuits. Out-of-bounds `temp[-1]` is never evaluated. (If `j` were unsigned `uint8_t`, decrement would underflow to 255, resulting in memory corruption/crash).
  - **Duplicate Handling**: Comparison `temp[j] > key` uses strict inequality (`>`). When duplicate values occur (`temp[j] == key`), the while loop breaks immediately, inserting `key` directly at `j+1`. Stable, $O(N)$ for identical values.
  - **Stack Footprint**: `uint16_t temp[5]` consumes exactly 10 bytes on the stack frame. No heap allocation, zero risk of heap fragmentation.
- **Conclusion**: Implementation is safe, stable, and correct.

---

## 2. Stress Test: PID Guard Clause (`PID.cpp`, `config.h`, `rightHand.cpp`)

### 2.1 Boundary Distances: `[0, 109, 110, 111, 200, 2000]`
Evaluation of Guard Clause:
`bool paredValida = (distancia > 0) && (distancia <= UMBRAL_PARED_VALIDA_PID);` (`UMBRAL_PARED_VALIDA_PID = 110`)

| Distance (mm) | `dist > 0` | `dist <= 110` | `paredValida` | Operational Impact |
|:---:|:---:|:---:|:---:|:---|
| **0** | FALSE | TRUE | **FALSE** | **Vulnerability Note**: If robot is scraping a wall ($dist \le OFSET$), $dist = 0$. Because $0 > 0$ is FALSE, the wall is discarded as invalid. If adjacent wall is open, PID falls into `else` ($error = 0$), failing to push away from the scraped wall! |
| **109** | TRUE | TRUE | **TRUE** | Valid wall. Included in proportional differential error. |
| **110** | TRUE | TRUE | **TRUE** | Boundary wall. Included in proportional differential error. |
| **111** | TRUE | FALSE | **FALSE** | Boundary gap. Omitted from PID error; references opposing wall. |
| **200** | TRUE | FALSE | **FALSE** | Standard corridor branch/opening. Omitted from PID; prevents corner jerk. |
| **2000** | TRUE | FALSE | **FALSE** | Sensor max range / timeout. Omitted from PID. |

---

### 2.2 Right-Side Opening (> 110 mm) Without Jerking Right
- **Scenario**: Robot traveling forward. Left wall at 40 mm (`DISTANCIA_OBJETIVO_PARED_IZQ`). Right wall suddenly opens into cross-corridor (`distanciaDer = 200 mm`).
- **Baseline (Defective Un-Guarded Behavior)**:
  `error = distDer - distIzq = 200 - 40 = +160`.
  `correccion = 0.5 * 160 = +80` (clamped to `+50`).
  `left_motor = 65 + 50 = 115`, `right_motor = 65 - 50 = 15`.
  Result: Violent right dive into the open corridor corner!
- **Refactored Guard Clause Behavior**:
  - `paredIzqValida = (40 > 0 && 40 <= 110) = true`.
  - `paredDerValida = (200 > 0 && 200 <= 110) = false`.
  - Branch:
    `error = DISTANCIA_OBJETIVO_PARED_IZQ - mediciones.distanciaIzq = 40 - 40 = 0`.
  - Prior `errorAnterior` was $+7$ (from nominal centered corridor).
    Derivative term: $K_D \times (0 - 7) = 0.3 \times (-7) = -2.1 \implies -2$.
  - Correction: $0 - 2 = -2$.
    `left_motor = 65 - 2 = 63`, `right_motor = 65 + 2 = 67`.
  - Result: Slight left bias (away from opening), transitioning to $error = 0, correccion = 0$ on subsequent cycles.
- **Conclusion**: Left wall reference holds cleanly without any jerk towards the right opening.

---

### 2.3 Mathematical Sign Verification: Closer to Right Wall $\implies$ Steers Left
- **Kinematics in `rightHand.cpp:74-75`**:
  ```cpp
  .izquierda = (int16_t)(VEL_BASE_IZQ + correccion),
  .derecha   = (int16_t)(VEL_BASE_DER - correccion)
  ```
  - When `correccion > 0`: Left wheel faster, right wheel slower $\implies$ Turns **RIGHT**.
  - When `correccion < 0`: Left wheel slower, right wheel faster $\implies$ Turns **LEFT**.
- **Case 1: Two-Wall Centering, Robot Closer to Right Wall**:
  - Distance: `distanciaDer = 25 mm`, `distanciaIzq = 62 mm`.
  - Error: `error = distDer - distIzq = 25 - 62 = -37`.
  - Correction: `correccion = KP * error = 0.5 * (-37) = -18`.
  - Motor speeds:
    `izquierda = 65 + (-18) = 47`
    `derecha   = 65 - (-18) = 83`
  - Kinematic Result: Right wheel faster, left wheel slower $\implies$ Vehicle yaws counter-clockwise (**LEFT**).
  - Direction: Steers away from right wall towards maze center. **VERIFIED CORRECT**.
- **Case 2: Single-Wall Right Tracking, Robot Closer to Right Wall**:
  - Distance: `distanciaDer = 27 mm` (target is 47 mm).
  - Error: `error = distDer - DISTANCIA_OBJETIVO_PARED_DER = 27 - 47 = -20`.
  - Correction: `correccion = 0.5 * (-20) = -10`.
  - Motor speeds: `izquierda = 55`, `derecha = 75`.
  - Kinematic Result: Steers **LEFT**.
  - Direction: Steers away from right wall. **VERIFIED CORRECT**.

---

### 2.4 Derivative Kick Prevention via `resetearErrorAnterior()`
- **Mechanism**:
  - Upon completion of rotational maneuvers (`GIRANDO_DER`, `GIRANDO_IZQ`, `GIRANDO_180`) and intermediate tracking (`POST_GIRO_AVANZAR`), `resetearErrorAnterior()` is invoked.
  - When transitioning back into `AVANZANDO`:
    $errorAnterior = 0$.
  - First cycle derivative computation:
    $K_D \times (error - errorAnterior) = K_D \times (error - 0)$.
  - Without this reset, stale error from prior states (e.g. $-45$) would produce an artificial jump of $0.3 \times (0 - (-45)) = +13.5$ PWM units, creating sudden torque spikes.
- **Conclusion**: `resetearErrorAnterior()` completely eliminates derivative kick on state transitions.

---

## 3. Adversarial Findings & Actionable Recommendations

### Finding 1 [Medium]: `distancia > 0` Wall Scraping Blindspot
- **Location**: `PID.cpp:9,10`
- **Issue**: Distance is computed in `sensoresDistancia.cpp` as `(filtrado > OFSET) ? (filtrado - OFSET) : 0`. When the robot physically rubs against a wall, `distancia` is clamped to 0. Checking `distancia > 0` marks the wall as invalid. If the opposite wall is an opening, both walls evaluate as invalid, resulting in $error = 0$, abandoning steering away from the wall.
- **Recommendation**: Either allow `distancia >= 0` with a separate hardware health boolean flag, or check `filtrado > 0` before offset subtraction.

### Finding 2 [Low]: Offset Asymmetry in Centered Two-Wall Error
- **Location**: `PID.cpp:16` vs `config.h:107-108`
- **Issue**: `error = distanciaDer - distanciaIzq`. Because `DISTANCIA_OBJETIVO_PARED_DER = 47` and `DISTANCIA_OBJETIVO_PARED_IZQ = 40`, if the robot is physically centered at its target single-wall distances, two-wall error evaluates to $47 - 40 = +7$ instead of 0.
- **Recommendation**: Normalize two-wall error:
  `error = (distanciaDer - DISTANCIA_OBJETIVO_PARED_DER) - (distanciaIzq - DISTANCIA_OBJETIVO_PARED_IZQ);`
