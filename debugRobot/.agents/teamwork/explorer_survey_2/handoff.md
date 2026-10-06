# Handoff Report: Explorer Survey 2 (R1 & R2 Investigation)

## 1. Observation
- **User Request & Requirements (`ORIGINAL_REQUEST.md:18-23`):**
  - R1: "En el estado AVANZANDO, si el robot viene detectando una pared lateral y esta desaparece (pasa de <130 a >130), debe resetear sus encoders y avanzar exactamente 400 pulsos (media celda) para frenar en el centro. Si no detecta paredes laterales desde el inicio, debe usar el conteo original de 800 pulsos como plan B."
  - R2: "Si el robot avanza hacia una celda con pared enfrente, no debe usar los encoders para frenar. Debe avanzar hasta que el sensor central lea exactamente 50 mm (o menos) de distancia y entonces detenerse para tomar la decisión."
- **Sensors and Units (`src/hardware/sensoresDistancia/sensoresDistancia.h:14-18`, `sensoresDistancia.cpp:80-107`, `config.h:25-28, 34-37`):**
  - Sensor struct: `struct sensado { int16_t distanciaCent; int16_t distanciaDer; int16_t distanciaIzq; };`
  - Units: Millimeters (`mm`), signed 16-bit integer (`int16_t`).
  - Offsets: `OFSET_IZQ = 40`, `OFSET_DER = 47`, `OFSET_CENT = 50`.
  - Wall threshold: `UMBRAL_PARED_ESTADO_NORMAL = 130` mm, `UMBRAL_PARED_FRENTE = 120` mm.
  - Initial sensor values: `sensadoActual = {0,0,0}` in `main.cpp:13` and `static sensado lecturaAct = {0,0,0}` in `sensoresDistancia.cpp:83`.
- **Encoders and Stop Condition (`src/main.cpp:72-92`, `src/config.h:66`):**
  - Pulse calculation: `pulsosActuales = (abs(verPulsosEncoderA()) + abs(verPulsosEncoderB()))/2;`
  - Current stop condition: strictly `if (pulsosActuales < PULSOS_CELDA)` with `PULSOS_CELDA = 800`.
  - When `pulsosActuales >= 800`: transitions to `FRENANDO` (`tiempoInicioFreno = millis(); estadoPostFreno = DECISION; estado = FRENANDO;`).
  - No front sensor stop condition exists in current `AVANZANDO`.
  - No lateral falling edge logic exists in current `AVANZANDO`.
- **FSM Transitions (`src/main.cpp:95-123, 170-179`):**
  - `FRENANDO`: holds active brake `movimiento(FRENO_F, {0,0})` for 150 ms, calls `resetearErrorAnterior()` and `resetearEncoders()`, then transitions to `estadoPostFreno` (`DECISION` or `AVANZANDO`).
  - `DECISION`: evaluates `sensadoActual` to trigger `GIRANDO_DER`, `AVANZANDO`, `GIRANDO_IZQ`, or `GIRANDO_180`.

## 2. Logic Chain
1. **R1 Falling Edge Mechanics:**
   - In Micromouse, a wall presence signal is boolean: $P = (\text{distancia} \le 130\text{ mm})$.
   - The transition from wall present to wall absent is a falling edge ($< 130\text{ mm} \to > 130\text{ mm}$).
   - To detect this reliably without loop blocking:
     - The robot must track if a wall was ever observed during the current cell advance (`habiaPared = true`). If started in an open space, `habiaPared` stays `false` and no edge triggers.
     - When `habiaPared && (distancia > 130)` evaluates to `true`, a one-shot latch (`flancoDetectado = true`) must be set immediately, calling `resetearEncoders()` and setting target limit to 400 pulses.
     - Without the one-shot latch, every subsequent loop cycle would see `distancia > 130` and re-reset encoders continuously (reproducing BUG-08).
     - 800 pulses = 1 cell (180 mm). 400 pulses = 1/2 cell (90 mm). The edge occurs at the cell boundary peg; advancing 400 pulses centers the robot in the next cell.
2. **R2 Front Alignment Mechanics:**
   - The front sensor reading is `sensadoActual.distanciaCent` in millimeters.
   - Currently, stopping only depends on `pulsosActuales >= 800`.
   - Front stop condition must evaluate `distanciaCent <= 50 mm`.
   - If evaluated naively, uninitialized `{0,0,0}` or stale readings from `FRENANDO` would evaluate `0 <= 50 == true`, stopping prematurely at pulse 0.
   - Protection requires checking `distanciaCent > 0 && distanciaCent <= 50` and requiring minimum pulse progression (e.g. `pulsosActuales > 100`) before permitting front stop.
   - When approaching a front wall, the front stop condition must override the encoder limit so the robot does not stop short at 70 mm or 80 mm; conversely, if no front wall exists (`distanciaCent > 120 mm`), the robot relies solely on encoder targets (400 or 800 pulses).
3. **Cross-Requirement Interactions & Race Conditions:**
   - **R1 vs R3:** R3 requires PID blindness for the first 100 pulses of `AVANZANDO`. If encoders are reset mid-stride by R1, checking `pulsosActuales < 100` would re-blind the PID for another 100 pulses in the middle of an intersection. Solution: track absolute cell pulses or use a `cegueraPIDCompletada` flag.
   - **R1 vs R2:** If both a lateral edge and a front wall occur (e.g. turning corner), R2 front proximity ($\le 50\text{ mm}$) must take strict precedence over completing the 400 encoder pulses of R1 to prevent front collision.
   - **Dual lateral edge:** In a 4-way cross junction, both left and right walls drop. The one-shot flag must prevent double-resetting encoders.

## 3. Caveats
- Sensor noise: VL53L0X can produce occasional spurious readings. While a 150-pulse distance threshold and consecutive checks prevent most spikes, physical hardware testing on track is necessary to fine-tune timing margins.
- Physical offset calibration: `OFSET_CENT` is defined as 50 mm, meaning a reading of `distanciaCent <= 50` represents `rawCent <= 100 mm`. The physical distance from the chassis front bumper to the sensor lens must be confirmed by the user/hardware team.
- No code was modified in the workspace (strictly read-only mode).

## 4. Conclusion
- Requirements R1 and R2 are fully compatible with the existing architecture in `src/main.cpp`.
- Implementation requires:
  1. Adding state variables in `main.cpp` for cell advance tracking: `limitePulsosActual`, `flancoLateralDetectado`, `habiaParedIzq`, `habiaParedDer`, `pulsosAbsolutosCelda`.
  2. One-shot lateral edge detection with encoder reset and 400-pulse target in `case AVANZANDO:`.
  3. Safe front stopping condition (`distanciaCent > 0 && distanciaCent <= 50 && pulsosActuales > 100`) overriding encoder limits.
  4. Safety watchdog pulse ceiling (`PULSOS_CELDA + 250`) against dead front sensor.
  5. State variable resets upon any transition entering `AVANZANDO`.
- Comprehensive details and code designs are documented in `report.md`.

## 5. Verification Method
- **Code Inspection:**
  - Verify that `main.cpp` contains no `while()` or `delay()` in `case AVANZANDO:` (non-blocking).
  - Verify that `flancoLateralDetectado` acts as a one-shot latch preventing repeated encoder resets.
  - Verify that `distanciaCent <= 50` has guards against `0` and pulse progress guards (`> 100`).
  - Verify that PID blindness (R3) is decoupled from the R1 encoder reset.
- **Build Verification:**
  - PlatformIO build command: `pio run` (or `platformio run`).
