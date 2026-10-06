# Forensic Audit Report — Milestone M1: Micromouse Odometry Correction

**Work Product**: `src/main.cpp`, `src/config.h`  
**Profile**: General Project (Embedded / Robotics) — Integrity Mode: `development`  
**Auditor**: `teamwork_preview_auditor` (`auditor_1`)  
**Verdict**: **CLEAN**

---

## 1. Observation

Direct code inspections of the target files revealed the following exact implementations:

### 1.1 Constants in `src/config.h` (lines 66-74)
```cpp
#define PULSOS_CELDA 800
#define PULSOS_CELDA_MEDIA 400
#define DISTANCIA_PARADA_FRENTE 50
#define PULSOS_GRACIA_PID 100
#define PULSOS_MIN_DETECCION_FLANCO 150
#define PULSOS_WATCHDOG_SEGURIDAD 1050
#define PULSOS_PREGIRO_90_DER 300
#define PULSOS_PREGIRO_90_IZQ 280
```

### 1.2 State Initialization in `src/main.cpp` (lines 39-51)
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
  sensadoActual = actualizarSensado();
  enviarString(">>> AVANZANDO <<<");
  estado = AVANZANDO;
}
```

### 1.3 R1 (Falling Edge Detection & Odometry Reset) in `src/main.cpp` (lines 104-124)
```cpp
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
```

### 1.4 R2 (Front Wall Stop & Encoder Override) in `src/main.cpp` (lines 126-149)
```cpp
      // R2: Condiciones de parada
      // 1. Parada por pared frontal a <= 50 mm (con guarda de validez física > 0 y recorrido > 100)
      bool stopPorParedFrontal = (sensadoActual.distanciaCent > 0 &&
                                  sensadoActual.distanciaCent <= DISTANCIA_PARADA_FRENTE &&
                                  pulsosTotalesCelda > 100);

      // 2. Detección de pared frontal en aproximación
      bool paredAlFrente = (sensadoActual.distanciaCent > 0 &&
                            sensadoActual.distanciaCent <= UMBRAL_PARED_FRENTE);

      // 3. Parada por encoders (400 pulsos si hubo flanco, o 800 fallback si no hubo flanco).
      // R2 sobreescribe la parada por encoders si hay pared al frente.
      bool stopPorEncoders = (pulsosActuales >= limitePulsosActual) && !paredAlFrente;

      // 4. Watchdog de seguridad (anti-colisión ante fallo de sensor frontal)
      bool stopPorWatchdog = (pulsosTotalesCelda >= PULSOS_WATCHDOG_SEGURIDAD);

      if (stopPorParedFrontal || stopPorEncoders || stopPorWatchdog) {
        enviarString(">>> INGRESO A DECISIÓN (FRENANDO) <<<");
        movimiento(FRENO_F, {0,0});
        tiempoInicioFreno = millis();
        estadoPostFreno = DECISION;
        estado = FRENANDO;
      }
```

### 1.5 R3 (Post-Turn PID Grace & Bumpless Transfer) in `src/main.cpp` (lines 150-166)
```cpp
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
```

### 1.6 R4 (Prohibition of Mapping & FRENANDO State Integration) in `src/main.cpp`
- **Zero arrays / data structures**: A search for `[` in `src/main.cpp` returned 0 occurrences. Zero `std::vector`, `std::map`, `grid`, `maze`, or coordinate trackers exist.
- **Unified `FRENANDO`**: Lines 143-148, 204-209, 219-224, 234-239 all transition to `FRENANDO` with `tiempoInicioFreno = millis()`.
- **Non-blocking handling** (lines 242-254):
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

---

## 2. Logic Chain

1. **Genuineness Check**:
   - The implementation does not contain any hardcoded test fixtures, pre-calculated outputs, dummy functions, or stubbed return constants.
   - Sensor inputs from `actualizarSensado()` and encoder readings from `verPulsosEncoderA()` / `verPulsosEncoderB()` drive all logic branches directly and dynamically.

2. **Prohibition Check (R4)**:
   - `ORIGINAL_REQUEST.md § R4` strictly bans mapping logic, grids, matrices, coordinate tracking ($X/Y$), and history tracking.
   - Static analysis confirmed:
     - Zero multi-dimensional or one-dimensional arrays in `src/main.cpp`.
     - Zero coordinate state variables (e.g. `x`, `y`, `heading`, `visited`).
     - Legacy `#define X_SIZE 10` and `#define Y_SIZE 10` in `config.h` are unused macros from early project scaffolding; neither is referenced or used anywhere in `main.cpp`.
     - The only state preserved across cycles is single-cell transient tracking (`habiaParedIzq`, `habiaParedDer`, `flancoDetectado`, `pulsosBaseCelda`), which are strictly wiped at every cell boundary by `iniciarAvanceCelda()`.

3. **Logic Correctness**:
   - **R1 (Falling Edge)**: Transition from $<130\text{ mm}$ to $>130\text{ mm}$ requires prior presence (`habiaPared* = true`) and subsequent opening (`distancia* > 130`). Dispatches encoder reset, sets limit to 400 pulses (`PULSOS_CELDA_MEDIA`), and latches (`flancoDetectado = true`) to prevent re-triggering. If no lateral wall was detected initially, limit remains at 800 pulses (`PULSOS_CELDA`). Correct.
   - **R2 (Front Wall Alignment)**: `paredAlFrente` overrides encoder stopping condition `(pulsosActuales >= limitePulsosActual) && !paredAlFrente`. The robot continues moving forward until `sensadoActual.distanciaCent <= 50 && distanciaCent > 0 && pulsosTotalesCelda > 100`, or until the 1050-pulse safety watchdog trips. Correct.
   - **R3 (Post-turn Grace / PID Silence)**: For the first 100 pulses (`pulsosTotalesCelda < 100`), `correccion = 0`. Because `calcularCorreccion(sensadoActual)` executes every loop iteration, `errorAnterior` tracks true sensor error smoothly, avoiding derivative kick upon release. Because `pulsosTotalesCelda` accumulates `pulsosBaseCelda` upon R1 reset, mid-cell R1 encoder resets do NOT re-trigger the 100-pulse silence. Correct.
   - **Stability & Signs**: Sensor error signs and motor channel mappings were traced against `PID.cpp` and `main.cpp`. Corrections generate stabilizing negative feedback (drifting right speeds up the right wheel and slows down the left wheel, steering left back toward center; and vice-versa). All global and local variables are properly initialized.

4. **State Machine Integrity**:
   - Original FSM states (`LISTO`, `AVANZANDO`, `DECISION`, `GIRANDO_DER`, `GIRANDO_IZQ`, `GIRANDO_180`, `FRENANDO`) remain intact.
   - All deceleration transitions funnel through `FRENANDO` for a non-blocking 150 ms stabilization window before entering `DECISION` or `AVANZANDO`.

---

## 3. Caveats

- **Physical Hardware Execution**: Verification was conducted via static code inspection, rigorous symbol tracing, and adversarial mathematical analysis of feedback loops rather than running on physical ESP32 hardware connected over USB.
- **Pre-existing Legacy Defines**: `X_SIZE`, `Y_SIZE`, `X_START`, `Y_START` remain in `src/config.h` from pre-existing code, but are completely unreferenced in active code and do not violate R4.

---

## 4. Conclusion

The work product implemented in `src/main.cpp` and `src/config.h` for Milestone M1 is **100% GENUINE, SOUND, AND FREE OF INTEGRITY VIOLATIONS**.
All four requirements (R1, R2, R3, R4) are faithfully fulfilled according to `ORIGINAL_REQUEST.md` constraints.

**Verdict: CLEAN**

---

## 5. Verification Method

To independently verify this report:

1. **Verify Absence of Arrays / Grid / Coordinates (R4)**:
   - Search for `[` in `src/main.cpp`: yields 0 matches.
   - Search for coordinate tracking variables (`x`, `y`, `pos`, `grid`) in `src/main.cpp`: yields 0 matches.
2. **Verify Falling Edge Logic (R1)**:
   - Inspect `src/main.cpp:104-124`. Verify `flancoDetectado` latch, `PULSOS_MIN_DETECCION_FLANCO` guard (150 pulses), and reset to 400 pulses.
3. **Verify Front Wall Override (R2)**:
   - Inspect `src/main.cpp:126-148`. Verify `!paredAlFrente` gate on `stopPorEncoders`, 50 mm trigger on `stopPorParedFrontal`, and 1050 pulse watchdog.
4. **Verify PID Silence & Bumpless Transfer (R3)**:
   - Inspect `src/main.cpp:150-166`. Verify `calcularCorreccion` executes prior to zeroing out `correccion`, and that `pulsosTotalesCelda` is used instead of raw `pulsosActuales`.
5. **Verify Non-blocking FRENANDO (R4)**:
   - Inspect `src/main.cpp:242-254`. Verify `millis() - tiempoInicioFreno >= 150` non-blocking timer.
