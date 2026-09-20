# Handoff Report: Sensor Subsystem & Median Filter (R1) Analysis

**Author**: Sensor Specialist (`teamwork_preview_explorer`)  
**Target Path**: `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\bahiaBlanca`  
**Date**: 2026-09-20T00:46:00Z  

---

## 1. Observation

### 1.1 Sensor Subsystem Files & Architecture
- **Header**: `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\bahiaBlanca\src\hardware\sensoresDistancia\sensoresDistancia.h`
  - Defines the public data transfer object `struct sensado`:
    ```cpp
    struct sensado {
        uint16_t distanciaCent;
        uint16_t distanciaDer;
        uint16_t distanciaIzq;
    };
    ```
  - Exposes two public functions:
    - Line 23: `void inicializacionSensoresDist();`
    - Line 26: `sensado actualizarSensado();`
  - Encapsulation note in lines 1-3:
    ```cpp
    // HEADER DE LOS SENSORES DE DISTANCIA. 
    // TODAS LAS VARIABLES SON PRIVADAS SALVO UN STRUCT CON EL QUE SE CREARÁ LA VARIABLE DE POSICIÓN EN EL MAIN. 
    // NO SE TIENE QUE EXPORTAR EL OBJETO NI NADA, SOLAMENTE EL STRUCT DEFINIDO EN LAS PRIMERAS LINEAS.
    ```

- **Implementation**: `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\bahiaBlanca\src\hardware\sensoresDistancia\sensoresDistancia.cpp`
  - Uses Pololu's `VL53L0X` library (`#include <VL53L0X.h>`).
  - Three global sensor instances: `VL53L0X sensorDer, sensorIzq, sensorCent;` (line 12).
  - Sensor Boot & Address Assignment (`inicializacionSensoresDist` lines 14-71):
    1. Initializes I2C bus via `Wire.begin()`.
    2. Pulls all three XSHUT pins `LOW` (`xshutPinDer`, `xshutPinCent`, `xshutPinIzq`) with a `delay(10)`.
    3. Sequentially raises each XSHUT pin `HIGH`, waits `delay(10)`, calls `init()`, assigns a unique I2C address (`adressDer` = `0x30`, `adressCent` = `0x32`, `adressIzq` = `0x31`), and starts continuous ranging via `startContinuous(0)`.
  - Raw Reading Gathering (`actualizarSensado` lines 73-101):
    ```cpp
    sensado actualizarSensado(){
      static sensado lecturaAct = {0,0,0}; 
      static unsigned long ultimoSensado = 0;

      // Evitamos saturar el bus I2C (El ESP32 es tan rápido que ahogaba a los sensores)
      // Limitamos la lectura a cada 20ms (50Hz)
    //  if(millis() - ultimoSensado > 20){
        if((sensorIzq.readReg(VL53L0X::RESULT_INTERRUPT_STATUS) & 0x07) != 0){
          uint16_t rawIzq = sensorIzq.readRangeContinuousMillimeters();
          if (rawIzq > 2000) rawIzq = 2000;
          lecturaAct.distanciaIzq = (rawIzq > OFSET_IZQ) ? (rawIzq - OFSET_IZQ) : 0;
        }
        if((sensorCent.readReg(VL53L0X::RESULT_INTERRUPT_STATUS) & 0x07) != 0){
          uint16_t rawCent = sensorCent.readRangeContinuousMillimeters();
          if (rawCent > 2000) rawCent = 2000;
          lecturaAct.distanciaCent = (rawCent > OFSET_CENT) ? (rawCent - OFSET_CENT) : 0;
        }
        if((sensorDer.readReg(VL53L0X::RESULT_INTERRUPT_STATUS) & 0x07) != 0){
          uint16_t rawDer = sensorDer.readRangeContinuousMillimeters();
          if (rawDer > 2000) rawDer = 2000;
          lecturaAct.distanciaDer = (rawDer > OFSET_DER) ? (rawDer - OFSET_DER) : 0;
        }
        ultimoSensado = millis();
      //}
      
      return lecturaAct;
    }
    ```
  - Crucial timing observation: The non-blocking check `(readReg(VL53L0X::RESULT_INTERRUPT_STATUS) & 0x07) != 0` evaluates whether the sensor has completed a measurement (~30-33 ms default timing budget). If not ready, `lecturaAct` retains the previous reading.
  - Offsets applied:
    - Left: `OFSET_IZQ` (defined as `40` in `config.h` line 26).
    - Center: `OFSET_CENT` (defined as `80` in `config.h` line 27).
    - Right: `OFSET_DER` (defined as `47` in `config.h` line 25).
  - Out-of-range clamp: Hardcoded magic number `2000` (`if (raw > 2000) raw = 2000;`).

### 1.2 Consumers of Distance Readings
Across the codebase, all distance readings are consumed exclusively via `actualizarSensado()` and the `struct sensado` type:
1. `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\bahiaBlanca\src\maquinaEstados\rightHand.cpp`:
   - Line 10: `sensadoActual = actualizarSensado();`
   - Lines 13, 15, 17: Wall presence checks against `UMBRAL_PARED_ESTADO_NORMAL` (`100` mm):
     - `sensadoActual.distanciaDer > UMBRAL_PARED_ESTADO_NORMAL`
     - `sensadoActual.distanciaCent > UMBRAL_PARED_ESTADO_NORMAL`
     - `sensadoActual.distanciaIzq > UMBRAL_PARED_ESTADO_NORMAL`
   - Lines 25, 40: Passed to PID controller: `long error = calcularCorreccion(sensadoActual);`
2. `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\bahiaBlanca\src\hardware\movimiento\PID.cpp`:
   - Line 7: `int16_t calcularCorreccion(sensado mediciones)`
   - Lines 9-10: Wall presence flags:
     - `bool hayIzq = mediciones.distanciaIzq < (UMBRAL_PARED_ESTADO_NORMAL + 50);`
     - `bool hayDer = mediciones.distanciaDer < (UMBRAL_PARED_ESTADO_NORMAL + 50);`
   - Lines 16, 19, 22: Error calculation:
     - Both walls: `error = (int16_t)mediciones.distanciaIzq - (int16_t)mediciones.distanciaDer;`
     - Left only: `error = (int16_t)mediciones.distanciaIzq - OFSET_IZQ;`
     - Right only: `error = OFSET_DER - (int16_t)mediciones.distanciaDer;`
3. `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\bahiaBlanca\src\test.cpp.bak`:
   - Line 19: `sensado distancias = actualizarSensado();`
   - Used for serial/bluetooth telemetry.

### 1.3 Pinout & Hardware Conflict Discovery
- In `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\bahiaBlanca\src\config.h`:
  - Line 59: `#define ENC_B_2 18` (Encoder B channel 2)
  - Line 64: `#define xshutPinIzq 18` (Left VL53L0X XSHUT shutdown pin)
  - **CRITICAL PIN CONFLICT**: GPIO 18 is assigned simultaneously to both `ENC_B_2` (read via `ESP32Encoder::attachFullQuad` in `encoders.cpp:12`) and `xshutPinIzq` (driven output via `pinMode(xshutPinIzq, OUTPUT)` in `sensoresDistancia.cpp:23`).

---

## 2. Logic Chain

### 2.1 Window Size $N$ Selection for R1
1. **Observation**: VL53L0X continuous ranging has a measurement period of ~30-33 ms (approx. 30 Hz).
2. **Kinematic Reasoning**: A micromouse travels at speeds between 200 mm/s and 500 mm/s.
   - At 250 mm/s, the robot travels ~8.25 mm per 33 ms sensor sample.
   - A median filter of window size $N$ incurs an effective group delay of $\frac{N-1}{2}$ samples.
   - For $N=3$: Group delay = 1 sample ($\approx 33\text{ ms}$, $\approx 8.25\text{ mm}$). Rejects isolated 1-sample spikes, but vulnerable to 2-sample bursts.
   - For $N=5$: Group delay = 2 samples ($\approx 66\text{ ms}$, $\approx 16.5\text{ mm}$). Rejects up to 2 corrupted samples out of 5 consecutive readings. In a standard maze cell ($168\times168\text{ mm}$ or $180\times180\text{ mm}$), 16.5 mm is less than 10% of a cell length, preserving nimble reaction times while eliminating transient optical noise.
   - For $N=7$: Group delay = 3 samples ($\approx 100\text{ ms}$, $\approx 25\text{ mm}$). Noticeable lag during high-speed cell transitions.
3. **Deduction**: $N=5$ provides the optimal trade-off between latency and optical outlier rejection. It must be configurable via `#define FILTRO_MEDIANA_N 5` in `config.h`.

### 2.2 Circular Buffer & Median Algorithm Efficiency on ESP32
1. **Observation**: The microcontroller is an ESP32 (Xtensa 32-bit dual-core running at 240 MHz). Memory is ample, but dynamic allocations (`malloc`, `new`, `std::vector`) must be avoided in embedded control loops to prevent heap fragmentation and non-deterministic execution times.
2. **Buffer Update Timing**:
   - `actualizarSensado()` is called frequently in `loop()`.
   - The VL53L0X interrupt register `readReg(RESULT_INTERRUPT_STATUS) & 0x07` is non-zero ONLY when a new physical sample is ready (~30-33 ms).
   - If the circular buffer were pushed on every `actualizarSensado()` call, the buffer would instantly fill with identical duplicate values in a few milliseconds, completely destroying the median filter's outlier rejection.
   - **Therefore, new samples must ONLY be pushed into the circular buffer when `(readReg(...) & 0x07) != 0`.**
   - The median only needs to be recalculated when a new sample is inserted. Between updates, the cached median is returned.
3. **Sorting Algorithm**:
   - For $N=5$, standard Insertion Sort on a small stack-allocated array of `uint16_t` requires at most $\frac{N(N-1)}{2} = 10$ comparisons.
   - On a 240 MHz Xtensa core, this executes in less than 0.2 microseconds (negligible CPU load).
   - Stack copy ensures the historical circular buffer order is never altered.

### 2.3 Handling Initial Readings (Buffer Priming / Ramp-Up)
1. **Observation**: If a circular buffer of size 5 is initialized with zeroes, computing the median of `[0, 0, 0, 0, raw1]` yields `0`. For the first 2-3 sample cycles (~100 ms), the robot would perceive distance `0`, triggering false wall detections and violent PID steering reactions.
2. **Deduction & Solution**:
   - **Primary Strategy (Warm-up Priming during Setup)**: In `inicializacionSensoresDist()`, take the first valid physical measurement from each sensor and duplicate it across all $N$ slots of the buffer. This ensures the filter begins in a fully valid, steady-state condition with zero transient distortion.
   - **Defensive Strategy (Dynamic Window Sizing)**: Maintain a sample counter `count` (incrementing from 0 up to $N$). If the filter is evaluated before $N$ readings have accumulated, compute the median using only the `count` active samples (`temp[count / 2]`).

### 2.4 Consumer Protection & Zero API Disruption
1. **Observation**: All navigation algorithms (`right_hand()`), wall follower PID (`calcularCorreccion()`), and diagnostic monitors query distances through `actualizarSensado()` returning `struct sensado`.
2. **Deduction**: By encapsulating the median filter entirely inside `sensoresDistancia.cpp` and returning the filtered values inside `struct sensado`, all current and future consumers are guaranteed to receive filtered median values without altering any function signatures or caller code.

---

## 3. Proposed Implementation Design for Implementer

### 3.1 Data Structure in `sensoresDistancia.cpp`
```cpp
#include "config.h"

struct FiltroMediana {
    uint16_t buffer[FILTRO_MEDIANA_N];
    uint8_t index;       // Circular buffer write index
    uint8_t count;       // Number of valid samples accumulated (up to FILTRO_MEDIANA_N)
    uint16_t valorMediana; // Cached median value
};

static FiltroMediana filtroIzq = {{0}, 0, 0, 0};
static FiltroMediana filtroCent = {{0}, 0, 0, 0};
static FiltroMediana filtroDer = {{0}, 0, 0, 0};
```

### 3.2 Insertion & Median Calculation Helper
```cpp
static uint16_t actualizarFiltroMediana(FiltroMediana &f, uint16_t nuevoValor) {
    // 1. Insert into circular buffer
    f.buffer[f.index] = nuevoValor;
    f.index = (f.index + 1) % FILTRO_MEDIANA_N;
    if (f.count < FILTRO_MEDIANA_N) {
        f.count++;
    }

    // 2. Copy active elements to local stack array for sorting
    uint16_t temp[FILTRO_MEDIANA_N];
    for (uint8_t i = 0; i < f.count; i++) {
        temp[i] = f.buffer[i];
    }

    // 3. In-place insertion sort (deterministic, no dynamic memory)
    for (uint8_t i = 1; i < f.count; i++) {
        uint16_t key = temp[i];
        int8_t j = i - 1;
        while (j >= 0 && temp[j] > key) {
            temp[j + 1] = temp[j];
            j--;
        }
        temp[j + 1] = key;
    }

    // 4. Extract median
    f.valorMediana = temp[f.count / 2];
    return f.valorMediana;
}

static void prellenarFiltro(FiltroMediana &f, uint16_t valorInicial) {
    for (uint8_t i = 0; i < FILTRO_MEDIANA_N; i++) {
        f.buffer[i] = valorInicial;
    }
    f.index = 0;
    f.count = FILTRO_MEDIANA_N;
    f.valorMediana = valorInicial;
}
```

### 3.3 Integration in `actualizarSensado()`
```cpp
sensado actualizarSensado() {
    static sensado lecturaAct = {0, 0, 0};

    // Sensor Izquierdo
    if ((sensorIzq.readReg(VL53L0X::RESULT_INTERRUPT_STATUS) & 0x07) != 0) {
        uint16_t rawIzq = sensorIzq.readRangeContinuousMillimeters();
        if (rawIzq > DISTANCIA_MAX_VALIDA) rawIzq = DISTANCIA_MAX_VALIDA;
        uint16_t filtradoIzq = actualizarFiltroMediana(filtroIzq, rawIzq);
        lecturaAct.distanciaIzq = (filtradoIzq > OFSET_IZQ) ? (filtradoIzq - OFSET_IZQ) : 0;
    }

    // Sensor Central
    if ((sensorCent.readReg(VL53L0X::RESULT_INTERRUPT_STATUS) & 0x07) != 0) {
        uint16_t rawCent = sensorCent.readRangeContinuousMillimeters();
        if (rawCent > DISTANCIA_MAX_VALIDA) rawCent = DISTANCIA_MAX_VALIDA;
        uint16_t filtradoCent = actualizarFiltroMediana(filtroCent, rawCent);
        lecturaAct.distanciaCent = (filtradoCent > OFSET_CENT) ? (filtradoCent - OFSET_CENT) : 0;
    }

    // Sensor Derecho
    if ((sensorDer.readReg(VL53L0X::RESULT_INTERRUPT_STATUS) & 0x07) != 0) {
        uint16_t rawDer = sensorDer.readRangeContinuousMillimeters();
        if (rawDer > DISTANCIA_MAX_VALIDA) rawDer = DISTANCIA_MAX_VALIDA;
        uint16_t filtradoDer = actualizarFiltroMediana(filtroDer, rawDer);
        lecturaAct.distanciaDer = (filtradoDer > OFSET_DER) ? (filtradoDer - OFSET_DER) : 0;
    }

    return lecturaAct;
}
```

---

## 4. Constants Required in `config.h`

To eliminate magic numbers and comply with acceptance criteria, the following constants must be added to `src/config.h`:

| Constant Name | Recommended Value | Purpose & Context |
|---|---|---|
| `FILTRO_MEDIANA_N` | `5` | Window size for median filter (must be an odd integer: 3, 5, or 7) |
| `DISTANCIA_MAX_VALIDA` | `2000` | Clamp threshold (mm) for VL53L0X out-of-range/timeout readings |
| `TIMEOUT_SENSOR_MS` | `500` | Timeout in ms for sensor I2C initialization (`setTimeout`) |
| `DELAY_BOOT_SENSOR_MS` | `10` | Wait delay in ms after toggling XSHUT lines |

---

## 5. Caveats

1. **GPIO 18 Pin Conflict**:
   - `config.h:59`: `#define ENC_B_2 18`
   - `config.h:64`: `#define xshutPinIzq 18`
   - If encoder B pulses arrive on GPIO 18, they will toggle the XSHUT line of the left VL53L0X sensor, forcing continuous hardware resets of the left sensor. Conversely, driving GPIO 18 as an output will corrupt encoder readings. This pin assignment must be reassigned by the hardware/configuration team.
2. **I2C Bus Contention**:
   - All three VL53L0X sensors operate on the default I2C bus (`Wire`). Non-blocking status checking avoids waiting on the bus, but I2C clock frequency defaults to 100 kHz in standard Arduino `Wire.begin()`. Configuring `Wire.setClock(400000)` (400 kHz Fast-Mode) can reduce register read latency by 75%, allowing faster loop execution.
3. **No Code Modification Performed**:
   - As mandated by the read-only exploration rules, no source files were modified. The proposed code blocks are provided as drop-in blueprints for the implementing agent.

---

## 6. Conclusion

1. **R1 Filter Strategy**: A dedicated circular buffer per sensor with $N=5$ elements, updated strictly on sensor measurement readiness interrupts (`readReg & 0x07 != 0`), sorted via stack-based insertion sort, perfectly fulfills requirement R1.
2. **Seamless Interface**: By returning the filtered median inside `struct sensado` from `actualizarSensado()`, all consumers (`rightHand.cpp`, `PID.cpp`, `test.cpp.bak`) receive filtered data with zero breaking changes.
3. **Magic Numbers Eliminated**: Identified and scoped all configuration constants (`FILTRO_MEDIANA_N`, `DISTANCIA_MAX_VALIDA`, `TIMEOUT_SENSOR_MS`, `DELAY_BOOT_SENSOR_MS`) for inclusion in `config.h`.
4. **Actionable Alert**: The GPIO 18 conflict between encoder B and left sensor XSHUT is flagged for immediate resolution.

---

## 7. Verification Method

1. **Compilation Verification**:
   - Verify `config.h` additions and `sensoresDistancia.cpp` changes by running:
     `pio run`
2. **Filter Correctness Inspection**:
   - Verify that `FILTRO_MEDIANA_N` is odd (`FILTRO_MEDIANA_N % 2 == 1`).
   - Verify that `actualizarFiltroMediana` is called exclusively within the readiness `if` block, ensuring no repeated sample flooding.
   - Verify that all consumer calls to `actualizarSensado()` compile cleanly without signature changes.
3. **Pin Verification**:
   - Inspect `src/config.h` to ensure GPIO 18 is not assigned to both `ENC_B_2` and `xshutPinIzq`.
