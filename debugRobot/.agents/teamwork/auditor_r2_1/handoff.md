# Forensic Audit Report — Milestone M1 Iteration 2

**Work Product**: `src/main.cpp`, `src/config.h`  
**Auditor**: Forensic Auditor (`teamwork_preview_auditor`)  
**Directory**: `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\.agents\teamwork\auditor_r2_1`  
**Ground Truth**: `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\.agents\teamwork\ORIGINAL_REQUEST.md`  
**Profile**: General Project (Integrity Forensics)  
**Verdict**: **CLEAN**

---

### Phase Results
- **Hardcoded test results detection**: **PASS** — Zero hardcoded mock outputs, test strings, or bypass constants detected.
- **Facade implementation detection**: **PASS** — Zero empty/dummy methods, stubs, or delegate facades detected. Genuine reactive logic implemented.
- **Pre-populated artifact detection**: **PASS** — Zero pre-populated test/execution result files detected.
- **Prohibition check (R4 Mapping/Grids/Coordinates/History)**: **PASS** — Zero matrices, arrays, coordinate tracking (X/Y), or historical trajectory storage.
- **Requirement R1 (Lateral Edge Odometry Reset & Fallback)**: **PASS** — One-shot latch, 400-pulse reset on falling edge (>150 pulses), and fallback to 800 pulses when no wall present.
- **Requirement R2 Fix 1 (Immediate Front Stop at <= 50 mm)**: **PASS** — Removed `pulsosTotalesCelda > 100` guard; emergency front stop active from pulse 0.
- **Requirement R2 Fix 2 (Lower Bound `DISTANCIA_MIN_VALIDA -20`)**: **PASS** — Bounded sensor offset negative readings `[-20, 0]` mm to prevent collision bypass.
- **Requirement R2 Fix 3 (Front Approach Latch `aproximandoParedFrontal`)**: **PASS** — Protected encoder stop against optical ToF noise spikes past cell boundary.
- **Requirement R3 (Post-turn PID Blindness & Bumpless Transfer)**: **PASS** — 100-pulse PID suppression on cell entry, decoupled from R1 resets, smooth bumpless derivative transition.
- **Architecture & FSM Check (Non-blocking & `FRENANDO`)**: **PASS** — Fully cooperative FSM loop, unified 150 ms stabilizing `FRENANDO` state preserved across all motion stops.

---

## 1. Observation

### 1.1 Direct Source Code Inspection

#### A. Lower Bound Definition (`src/config.h`)
Lines 66-74 of `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\src\config.h`:
```cpp
#define PULSOS_CELDA 800
#define PULSOS_CELDA_MEDIA 400
#define DISTANCIA_PARADA_FRENTE 50
#define DISTANCIA_MIN_VALIDA -20
#define PULSOS_GRACIA_PID 100
#define PULSOS_MIN_DETECCION_FLANCO 150
#define PULSOS_WATCHDOG_SEGURIDAD 1050
#define PULSOS_PREGIRO_90_DER 300
#define PULSOS_PREGIRO_90_IZQ 280
```

#### B. State Variables & Cell Entry Initialization (`src/main.cpp`)
Lines 27-35 of `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\src\main.cpp`:
```cpp
// Variables para control de celda y odometría adaptativa (R1, R2, R3, R4)
uint16_t limitePulsosActual = PULSOS_CELDA;
bool flancoDetectado = false;
bool habiaParedIzq = false;
bool habiaParedDer = false;
uint32_t pulsosBaseCelda = 0;
bool graciaPIDFinalizada = false;
bool aproximandoParedFrontal = false;
```

Lines 40-53 of `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\src\main.cpp`:
```cpp
void iniciarAvanceCelda() {
  resetearEncoders();
  resetearErrorAnterior();
  limitePulsosActual = PULSOS_CELDA;
  flancoDetectado = false;
  habiaParedIzq = false;
  habiaParedDer = false;
  pulsosBaseCelda = 0;
  graciaPIDFinalizada = false;
  aproximandoParedFrontal = false;
  sensadoActual = actualizarSensado();
  enviarString(">>> AVANZANDO <<<");
  estado = AVANZANDO;
}
```

#### C. Case AVANZANDO Implementation (`src/main.cpp`)
Lines 98-174 of `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\src\main.cpp`:
```cpp
    case AVANZANDO: {
      int32_t pulsosA = abs(verPulsosEncoderA());
      int32_t pulsosB = abs(verPulsosEncoderB());
      pulsosActuales = (pulsosA + pulsosB) / 2;
      uint32_t pulsosTotalesCelda = pulsosBaseCelda + pulsosActuales;

      sensadoActual = actualizarSensado();

      // R1: Detección y reseteo por flanco lateral
      if (!flancoDetectado) {
        if (sensadoActual.distanciaIzq > 0 && sensadoActual.distanciaIzq <= UMBRAL_PARED_ESTADO_NORMAL) {
          habiaParedIzq = true;
        }
        if (sensadoActual.distanciaDer > 0 && sensadoActual.distanciaDer <= UMBRAL_PARED_ESTADO_NORMAL) {
          habiaParedDer = true;
        }

        bool flancoIzq = habiaParedIzq && (sensadoActual.distanciaIzq > UMBRAL_PARED_ESTADO_NORMAL);
        bool flancoDer = habiaParedDer && (sensadoActual.distanciaDer > UMBRAL_PARED_ESTADO_NORMAL);

        if ((flancoIzq || flancoDer) && pulsosTotalesCelda > PULSOS_MIN_DETECCION_FLANCO) {
          flancoDetectado = true;
          pulsosBaseCelda += pulsosActuales;
          resetearEncoders();
          pulsosActuales = 0;
          limitePulsosActual = PULSOS_CELDA_MEDIA;
          enviarString(">>> FLANCO DETECTADO: RESET A 400 PULSOS <<<");
        }
      }

      // R2: Condiciones de parada
      // 1. Parada por pared frontal a <= 50 mm (con guarda de validez física >= DISTANCIA_MIN_VALIDA)
      bool stopPorParedFrontal = (sensadoActual.distanciaCent >= DISTANCIA_MIN_VALIDA &&
                                  sensadoActual.distanciaCent <= DISTANCIA_PARADA_FRENTE);

      // 2. Detección de pared frontal en aproximación
      bool paredAlFrente = (sensadoActual.distanciaCent >= DISTANCIA_MIN_VALIDA &&
                            sensadoActual.distanciaCent <= UMBRAL_PARED_FRENTE);

      // Enclavar aproximación a pared frontal cerca o más allá del límite de celda
      if (paredAlFrente && pulsosActuales >= (limitePulsosActual - 100)) {
        aproximandoParedFrontal = true;
      }

      // 3. Parada por encoders (400 pulsos si hubo flanco, o 800 fallback si no hubo flanco).
      // R2 sobreescribe la parada por encoders si hay pared al frente o si se enclavó aproximación.
      bool stopPorEncoders = (pulsosActuales >= limitePulsosActual) && !paredAlFrente && !aproximandoParedFrontal;

      // 4. Watchdog de seguridad (anti-colisión ante fallo de sensor frontal)
      bool stopPorWatchdog = (pulsosTotalesCelda >= PULSOS_WATCHDOG_SEGURIDAD);

      if (stopPorParedFrontal || stopPorEncoders || stopPorWatchdog) {
        enviarString(">>> INGRESO A DECISIÓN (FRENANDO) <<<");
        movimiento(FRENO_F, {0,0});
        tiempoInicioFreno = millis();
        estadoPostFreno = DECISION;
        estado = FRENANDO;
      } else {
        // R3: Corrección PID y gracia post-giro (ceguera en los primeros 100 pulsos con bumpless transfer)
        int16_t correccion = calcularCorreccion(sensadoActual);
        if (!graciaPIDFinalizada) {
          if (pulsosTotalesCelda < PULSOS_GRACIA_PID) {
            correccion = 0;
          } else {
            graciaPIDFinalizada = true;
          }
        }

        // FIX PID: El Motor A (velocidadActual.izquierda) está mecánicamente en la rueda DERECHA.
        // Por eso invertimos los signos de la corrección y asignamos cruzadas las velocidades base.
        velocidadActual.izquierda = constrain(VEL_BASE_DER - correccion, 0, 255);
        velocidadActual.derecha = constrain(VEL_BASE_IZQ + correccion, 0, 255);

        movimiento(AVANZAR, {velocidadActual.izquierda, velocidadActual.derecha});
      }
      break;
    }
```

#### D. Telemetry Logging Sanitization (`src/main.cpp`)
Lines 176-179 of `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\src\main.cpp`:
```cpp
    case DECISION: {
      sensadoActual = actualizarSensado();
      enviarString(String(sensadoActual.distanciaDer) + " | " + String(sensadoActual.distanciaCent) + " | " + String(sensadoActual.distanciaIzq));
```

#### E. State Stabilization & FRENANDO Flow (`src/main.cpp`)
Lines 248-260 of `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\src\main.cpp`:
```cpp
    case FRENANDO: {
      movimiento(FRENO_F, {0,0}); // Mantener el freno activo
      if (millis() - tiempoInicioFreno >= 150) { // 150ms de pausa estabilizadora
        if (estadoPostFreno == AVANZANDO) {
          iniciarAvanceCelda();
        } else {
          resetearErrorAnterior();
          resetearEncoders();
          estado = estadoPostFreno;
        }
      }
      break;
    }
```

### 1.2 Prohibited Features Search Results
- Grep search for `[` (array indexing) in `src/main.cpp`: **0 matches**.
- Grep search for `vector`, `grid`, `matriz`, `map`, `coord`, `histori`: **0 matches**.
- Grep search for pre-populated result/output artifacts: **0 matches**.

---

## 2. Logic Chain

1. **Genuineness & Absence of Facades:**
   - Observations 1.1.A through 1.1.E demonstrate that all state transitions, sensor readings, odometry evaluations, and motor commands interact directly with underlying hardware drivers (`ESP32Encoder`, `VL53L0X`, `ledc` PWM).
   - No mock values, no dummy constant-returning stubs, and no self-certifying bypasses exist in the audited code.

2. **Verification of Challenger Vulnerability Fix 1 (Early Front Stop Guard Removal):**
   - Observation 1.1.C shows `stopPorParedFrontal` directly evaluates `(sensadoActual.distanciaCent >= DISTANCIA_MIN_VALIDA && sensadoActual.distanciaCent <= DISTANCIA_PARADA_FRENTE)`.
   - The previous guard `&& pulsosTotalesCelda > 100` is completely absent.
   - If an obstacle is detected at $\le 50\text{ mm}$ upon entering the cell (pulses $0 \dots 99$), the stop condition evaluates to `true` on the very first loop iteration, preventing blind collision.

3. **Verification of Challenger Vulnerability Fix 2 (Sensor Offset & Lower Bound Cota):**
   - Observation 1.1.A defines `DISTANCIA_MIN_VALIDA -20`.
   - In `sensoresDistancia.cpp`, `distanciaCent = rawCent - OFSET_CENT` with `OFSET_CENT = 50`. When physical distance approaches $0\text{ mm}$, raw sensor measurements ($30\dots 50\text{ mm}$) produce negative signed values in `[-20, 0]\text{ mm}`.
   - Observation 1.1.C bounds both `stopPorParedFrontal` and `paredAlFrente` with `>= DISTANCIA_MIN_VALIDA`.
   - Obstacles in physical bumper contact are correctly recognized, while negative noise/uninitialized invalid ranges ($<-20\text{ mm}$) are excluded.

4. **Verification of Challenger Vulnerability Fix 3 (Optical Noise Immunity via Latch):**
   - Observation 1.1.B introduces `bool aproximandoParedFrontal = false`, properly reinitialized to `false` in `iniciarAvanceCelda()`.
   - Observation 1.1.C shows that when the robot approaches the end of the cell (`pulsosActuales >= limitePulsosActual - 100`) and detects a front wall (`paredAlFrente == true`), `aproximandoParedFrontal` latches to `true`.
   - `stopPorEncoders` requires `!aproximandoParedFrontal`.
   - If a transient optical ToF spike $> 120\text{ mm}$ occurs while passing 800 pulses, `aproximandoParedFrontal` remains `true`, preventing `stopPorEncoders` from prematurely stopping the robot at 70 mm.
   - The robot continues smoothly until physical stopping threshold $\le 50\text{ mm}$ is reached (or watchdog triggers at 1050 pulses).

5. **Requirement R1 (Lateral Falling Edge Reset & Fallback):**
   - Observation 1.1.C verifies that if a lateral wall was present (`habiaParedIzq` / `habiaParedDer`) and drops below threshold, once `pulsosTotalesCelda > PULSOS_MIN_DETECCION_FLANCO` (150 pulses), `flancoDetectado` latches to `true`, encoders are reset, and `limitePulsosActual` becomes `PULSOS_CELDA_MEDIA` (400 pulses).
   - If no lateral wall was detected from the start, `limitePulsosActual` remains `PULSOS_CELDA` (800 pulses fallback), satisfying R1 precisely.

6. **Requirement R3 (Post-turn PID Blindness & Smooth Transfer):**
   - Observation 1.1.C verifies `correccion = 0` for `pulsosTotalesCelda < PULSOS_GRACIA_PID` (100 pulses).
   - `calcularCorreccion` is called each cycle so `errorAnterior` tracks current error without jump.
   - Post-edge resets preserve `pulsosTotalesCelda` via `pulsosBaseCelda`, preventing secondary blindness mid-cell.

7. **Requirement R4 (Strict Absence of Mapping & FRENANDO State):**
   - Observation 1.2 confirms absolute absence of matrix grids, coordinate trackers, or history storage.
   - Observation 1.1.C and 1.1.E confirm that all stopping triggers in `AVANZANDO` route through `FRENANDO` for 150 ms before entering `DECISION`, maintaining clean cooperative execution.

8. **Telemetry Logging Fix:**
   - Observation 1.1.D confirms `DECISION` now uses `String(...) + " | " + ...` concatenation matching the `enviarString(String str)` signature in `logger.h`, eliminating pointer arithmetic bugs on literal strings.

---

## 3. Caveats

- **No caveats.** Hardware drivers, state machine flow, timing boundaries, signed/unsigned type interactions, and edge conditions were thoroughly inspected against ground-truth requirements.

---

## 4. Conclusion

The work product implemented in `src/main.cpp` and `src/config.h` adheres strictly and authentically to all requirements specified in `ORIGINAL_REQUEST.md` (R1, R2, R3, R4) and successfully remedies all 3 vulnerabilities highlighted by Challenger 1.

There are zero facade implementations, zero mock shortcuts, zero hardcoded test strings, zero mapping structures, and zero architectural regressions.

**Final Audit Verdict:** **CLEAN**

---

## 5. Verification Method

To independently verify this verdict:
1. **Source Inspection:**
   - Inspect `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\src\config.h`:
     Confirm line 69 defines `#define DISTANCIA_MIN_VALIDA -20`.
   - Inspect `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\src\main.cpp`:
     Confirm lines 34, 49, 128-149, 156-165, and 178 implement the validated logic.
2. **Prohibition Check:**
   - Execute regex/grep search across `src/`:
     Ensure no occurrences of multi-dimensional arrays `[][]`, coordinates `(x, y)`, or mapping structures.
3. **Conditions of Invalidation:**
   This audit verdict would be invalidated if:
   - Any commit re-introduces the `pulsosTotalesCelda > 100` check into `stopPorParedFrontal`.
   - Any commit alters `DISTANCIA_MIN_VALIDA` such that readings in `[-20, 0]\text{ mm}` are ignored.
   - Any coordinate or grid-based mapping mechanism is added to the codebase.
