# Handoff Report: Codebase Survey (Explorer 1)

## 1. Observation
- **File Structure & PlatformIO**:
  - `platformio.ini` (lines 11-18): Targets `esp32doit-devkit-v1`, `framework = arduino`, with dependencies `VL53L0X` and `madhephaestus/ESP32Encoder @ ^0.11.7`.
  - Main code is located in `src/main.cpp`, `src/main.h`, `src/config.h`, and hardware drivers in `src/hardware/{encoders,logger,movimiento,sensoresDistancia}`.
- **FSM Implementation**:
  - `src/main.h` (lines 3-12): Defines enum `MAQUINA_ESTADOS { LISTO, INFORMACION_RECIBIDA, AVANZANDO, DECISION, GIRANDO_DER, GIRANDO_IZQ, GIRANDO_180, FRENANDO };`.
  - `src/main.cpp` (lines 72-93): In `AVANZANDO`, odometría is computed as:
    ```cpp
    pulsosActuales = (abs(verPulsosEncoderA()) + abs(verPulsosEncoderB()))/2;
    if (pulsosActuales < PULSOS_CELDA) {
        sensadoActual = actualizarSensado();
        int16_t correccion = calcularCorreccion(sensadoActual);
        velocidadActual.izquierda = constrain(VEL_BASE_DER - correccion, 0, 255);
        velocidadActual.derecha = constrain(VEL_BASE_IZQ + correccion, 0, 255);
        movimiento(AVANZAR, {velocidadActual.izquierda, velocidadActual.derecha});
    } else {
        enviarString(">>> INGRESO A DECISIÓN <<<");
        movimiento(FRENO_F, {0,0});
        tiempoInicioFreno = millis();
        estadoPostFreno = DECISION;
        estado = FRENANDO;
    }
    ```
  - `src/main.cpp` (lines 170-179): `FRENANDO` applies `movimiento(FRENO_F, {0,0})` and evaluates non-blocking timeout `if (millis() - tiempoInicioFreno >= 150)`, resetting encoders and error before transitioning to `estadoPostFreno`.
  - `src/main.cpp` (lines 95-124): In `DECISION`, evaluates sensor distances against `UMBRAL_PARED_ESTADO_NORMAL` (130 mm in `config.h:34`) to choose between `GIRANDO_DER`, `AVANZANDO`, `GIRANDO_IZQ`, and `GIRANDO_180`.
- **Sensors and Actuators**:
  - `src/hardware/sensoresDistancia/sensoresDistancia.cpp` (lines 80-108): Samples 3x VL53L0X sensors over I2C at 10 kHz, checking status register non-blockingly and subtracting offsets (`OFSET_CENT = 50`, `OFSET_DER = 47`, `OFSET_IZQ = 40`). Retains previous readings in static `lecturaAct`.
  - `src/hardware/movimiento/PID.cpp` (lines 7-33): Computes PD correction using `KP = 0.5` and `KD = 0.3`. Error is calculated based on lateral wall thresholds (`< 180 mm`).
  - `src/hardware/encoders/encoders.cpp` (lines 9-28): Quadrature PCNT decoding on ESP32, reset via `clearCount()`.

## 2. Logic Chain
1. *Observation*: In `src/main.cpp:75`, `AVANZANDO` checks only `pulsosActuales < PULSOS_CELDA` (800) to decide whether to stop. There is no tracking of lateral wall state transitions (`<130` to `>130`).
   *Inference*: Requirement R1 (reseteo por flanco lateral a 400 pulsos con fallback a 800) is completely missing from `main.cpp`.
2. *Observation*: In `src/main.cpp:72-93`, `sensadoActual.distanciaCent` is not checked for braking in `AVANZANDO`; it is only evaluated once the robot enters `DECISION` (line 101).
   *Inference*: Requirement R2 (parada frontal por distancia central $\le 50\text{ mm}$ sobreescribiendo encoders) is completely missing.
3. *Observation*: In `src/main.cpp:77-84`, `calcularCorreccion(sensadoActual)` and the resulting steering speed offsets are applied unconditionally for every pulse `0 <= pulsosActuales < PULSOS_CELDA`.
   *Inference*: Requirement R3 (ceguera del PID durante los primeros 100 pulsos tras iniciar `AVANZANDO`) is completely missing.
4. *Observation*: In `src/main.cpp` and `src/main.h`, the FSM is purely reactive with 8 states, utilizing `FRENANDO` for a 150 ms stabilization window, with no matrices or grid coordinates.
   *Inference*: Requirement R4 is satisfied in the existing structure and must be maintained when implementing R1-R3.

## 3. Caveats
- No modifications were made to source files (strict adherence to read-only constraint and user global rule).
- Terminal compilation (`pio run`) requires execution environment access where `pio` or `platformio` CLI is installed and configured in system PATH; interactive shell commands required user permissions that timed out, so dynamic hardware tests were not executed. Verification was conducted through static code and architectural analysis.

## 4. Conclusion
The codebase is clean, well-modularized, and ready for implementing requirements R1, R2, and R3. All three requirements locate their changes primarily within `src/main.cpp` inside `case AVANZANDO:`, plus necessary state variables in `main.cpp` (or `config.h` for thresholds/constants). No architectural rewrite is needed; the existing FSM and `FRENANDO` state provide the exact foundation required.

## 5. Verification Method
- **Code Inspection**:
  - Inspect `src/main.cpp` lines 72-93 to confirm absence of R1, R2, R3 logic.
  - Inspect `src/main.h` lines 3-12 to confirm states `AVANZANDO`, `FRENANDO`, `DECISION`.
  - Inspect `src/config.h` lines 34-37 to confirm `UMBRAL_PARED_ESTADO_NORMAL` (130) and `UMBRAL_PARED_FRENTE` (120).
- **Invalidation Condition**:
  - If `src/main.cpp` already had logic checking `distanciaCent <= 50` or lateral edge detection `< 130` to `> 130` in `AVANZANDO`, this report's gap analysis would be invalidated. Direct view of `src/main.cpp` confirms none of these exist.
