# Project: Micromouse Odometry Correction (debugRobot)

## Architecture
- Single-module embedded C++ firmware for ESP32 MicroMouse (`debugRobot`).
- Reactive Finite State Machine (FSM) in `src/main.cpp`.
- Hardware drivers:
  - Encoders (`ESP32Encoder`, PCNT hardware quadrature).
  - Time-of-Flight sensors (3x VL53L0X, non-blocking polling).
  - Actuators: H-Bridge (PWM via LEDC channels 0 & 1, GPIO directions).
  - Controller: Proportional-Derivative (PD) lane centering.
- Navigation loop:
  `AVANZANDO` -> `FRENANDO` (150ms) -> `DECISION` (Right-hand rule) -> `GIRANDO_*` -> `FRENANDO` -> `AVANZANDO`.

## Feature Inventory
| # | Feature | Description | Milestone | Source |
|---|---------|-------------|-----------|--------|
| 1 | R1: Reseteo por flanco lateral | Detección de flanco de bajada (<130 a >130 mm). Si venía con pared y cae, resetear odometría y avanzar 400 pulsos hasta el centro. Fallback a 800 pulsos si no hubo pared lateral desde el inicio. Latch anti-rebote. | M1 | ORIGINAL_REQUEST § R1 |
| 2 | R2: Alineación con pared frontal | En AVANZANDO, si hay pared al frente, sobreescribir límite de encoders y avanzar hasta que distancia central <= 50 mm. Incluye guarda de cota mínima (`DISTANCIA_MIN_VALIDA -20`), enclavamiento contra ruido óptico (`aproximandoParedFrontal`) y watchdog a 1050 pulsos. | M1 | ORIGINAL_REQUEST § R2 |
| 3 | R3: Gracia post-giro (Ceguera PID) | Al inicio de AVANZANDO, corrección PID forzada a 0 durante los primeros 100 pulsos. Desacoplada del reseteo de R1 para evitar ceguera secundaria a mitad de celda. Transferencia suave (bumpless) para evitar derivative kick. | M1 | ORIGINAL_REQUEST § R3 |
| 4 | R4: Ausencia estricta de mapeo & FRENANDO | Prohibición absoluta de matrices, coordenadas X/Y o historial. Conservación de FRENANDO (150 ms) como salida unificada para todas las condiciones de parada de AVANZANDO. | M1 | ORIGINAL_REQUEST § R4 |

## Milestones
| # | Name | Scope | Dependencies | Status |
|---|------|-------|-------------|--------|
| 1 | M1: Odometry & State Machine Mechanics | Implement R1, R2, R3, R4 in `src/main.cpp` and `src/config.h` | Survey complete | DONE |

## Interface Contracts
- `src/main.cpp`:
  - `case AVANZANDO:` integrates:
    - R1: One-shot latch `flancoDetectado`, history flags `habiaParedIzq`, `habiaParedDer`, minimum entry distance (>150 pulses), reset to 400 pulses target. Fallback to 800 pulses if no wall was present from start.
    - R2: Front wall stop `sensadoActual.distanciaCent >= DISTANCIA_MIN_VALIDA && sensadoActual.distanciaCent <= DISTANCIA_PARADA_FRENTE` (instant stop from pulse 0), overriding encoder target via `!paredAlFrente && !aproximandoParedFrontal`. Approach latch `aproximandoParedFrontal = true` when nearing encoder limit. Safety watchdog at 1050 pulses.
    - R3: PID blindness `pulsosTotalesCelda < 100 ? 0 : calcularCorreccion(...)`, continuous error history update in `PID.cpp` for bumpless derivative transition.
    - R4: Unified transition to `FRENANDO` (150ms dynamic brake) with `estadoPostFreno = DECISION`.
  - State initialization on entry to `AVANZANDO`: `iniciarAvanceCelda()` resets all 9 control variables, encoders, and flags deterministically across all entry points (`LISTO`, `DECISION`, post-turn `FRENANDO`).

## Code Layout
- `src/main.cpp`: Master state machine, odometry mechanics, and FSM lifecycle.
- `src/config.h`: System constants (`PULSOS_CELDA 800`, `PULSOS_CELDA_MEDIA 400`, `DISTANCIA_PARADA_FRENTE 50`, `DISTANCIA_MIN_VALIDA -20`, `PULSOS_GRACIA_PID 100`, `PULSOS_MIN_DETECCION_FLANCO 150`, `PULSOS_WATCHDOG_SEGURIDAD 1050`).
