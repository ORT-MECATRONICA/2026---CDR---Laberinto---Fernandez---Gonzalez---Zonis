## 2026-10-06T00:42:34Z
You are Worker 2 (teamwork_preview_worker) for Milestone M1 Iteration 2 of the Micromouse Odometry Correction project.
Your working directory is:
c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\.agents\teamwork\worker_m1_fix

MANDATORY FIRST STEP:
Read the authoritative user request at:
c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\.agents\teamwork\ORIGINAL_REQUEST.md

Read the failure evidence and remediation reports:
- Project Plan: c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\.agents\teamwork\orchestrator_1\PROJECT.md
- Challenger 1 Failure Report: c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\.agents\teamwork\challenger_1\handoff.md
- Remediation Explorer 1 Report (Early front stop): c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\.agents\teamwork\explorer_fix_1\handoff.md
- Remediation Explorer 2 Report (Noise immunity & hysteresis): c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\.agents\teamwork\explorer_fix_2\handoff.md
- Remediation Explorer 3 Report (Sensor underflow & valid lower bound): c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\.agents\teamwork\explorer_fix_3\handoff.md

MANDATORY INTEGRITY WARNING:
DO NOT CHEAT. All implementations must be genuine. DO NOT hardcode test results, create dummy/facade implementations, or circumvent the intended task. A teamwork_preview_auditor will independently verify your work. Integrity violations WILL be detected and your work WILL be rejected.

SCOPE & EXCLUSIVE WRITE OWNERSHIP:
You have exclusive write ownership of:
- src/main.cpp
- src/config.h

TASK SPECIFICATION (Milestone M1 Iteration 2):
Implement the 3 synthesized fixes for R2 in src/config.h and src/main.cpp while preserving R1, R3, and R4:

1. In src/config.h:
   - Add `#define DISTANCIA_MIN_VALIDA -20` (accounting for calibration offsets and physical bumper contact range).

2. In src/main.cpp:
   - Add global tracking variable: `bool aproximandoParedFrontal = false;`
   - In `iniciarAvanceCelda()`: reset `aproximandoParedFrontal = false;`
   - In `case AVANZANDO:`:
     - REMOVE the `&& pulsosTotalesCelda > 100` guard from `stopPorParedFrontal`.
     - Update `stopPorParedFrontal`:
       `bool stopPorParedFrontal = (sensadoActual.distanciaCent >= DISTANCIA_MIN_VALIDA && sensadoActual.distanciaCent <= DISTANCIA_PARADA_FRENTE);`
     - Update `paredAlFrente`:
       `bool paredAlFrente = (sensadoActual.distanciaCent >= DISTANCIA_MIN_VALIDA && sensadoActual.distanciaCent <= UMBRAL_PARED_FRENTE);`
     - Implement approach latching:
       `if (paredAlFrente && pulsosActuales >= (limitePulsosActual - 100)) { aproximandoParedFrontal = true; }`
     - Update `stopPorEncoders` to be immune to single-cycle optical noise spikes past 800 pulses:
       `bool stopPorEncoders = (pulsosActuales >= limitePulsosActual) && !paredAlFrente && !aproximandoParedFrontal;`
     - Keep `stopPorWatchdog` (`pulsosTotalesCelda >= PULSOS_WATCHDOG_SEGURIDAD`) as fail-safe.
   - Clean up the string logging issue in DECISION if present so it uses valid string formatting.

3. VERIFICATION & HANDOFF:
   - Verify code syntax and logic thoroughly.
   - Document all changes in handoff.md in your working directory.
   - Send completion message to parent upon finishing.
