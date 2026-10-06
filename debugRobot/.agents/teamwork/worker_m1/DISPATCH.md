## 2026-10-06T00:13:48Z
You are Worker 1 (teamwork_preview_worker) for Milestone M1 of the Micromouse Odometry Correction project.
Your working directory is:
c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\.agents\teamwork\worker_m1

MANDATORY FIRST STEP:
Read the authoritative user request at:
c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\.agents\teamwork\ORIGINAL_REQUEST.md

Read the project scope and explorer findings:
- Master Project plan: c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\.agents\teamwork\orchestrator_1\PROJECT.md
- Explorer 1 Report (Codebase & FSM): c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\.agents\teamwork\explorer_survey_1\report.md
- Explorer 2 Report (R1 & R2 Dynamics & Edge Cases): c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\.agents\teamwork\explorer_survey_2\report.md
- Explorer 3 Report (R3, R4 & Stability / Bumpless Transfer): c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\.agents\teamwork\explorer_survey_3\report.md

MANDATORY INTEGRITY WARNING:
DO NOT CHEAT. All implementations must be genuine. DO NOT hardcode test results, create dummy/facade implementations, or circumvent the intended task. A teamwork_preview_auditor will independently verify your work. Integrity violations WILL be detected and your work WILL be rejected.

SCOPE & EXCLUSIVE WRITE OWNERSHIP:
You have exclusive write ownership of:
- src/main.cpp
- src/config.h (only for adding constants if needed)
DO NOT touch any other source files unless strictly necessary.

TASK SPECIFICATION (Milestone M1):
Implement requirements R1, R2, R3, R4 in src/main.cpp:
1. R1 (Reseteo por flanco lateral):
   - In AVANZANDO, track if lateral walls were detected (habiaParedIzq / habiaParedDer with distance <= UMBRAL_PARED_ESTADO_NORMAL, 130 mm).
   - If a lateral wall was present and disappears (reading > 130 mm):
     - Trigger only once per cell (latch: flancoDetectado = true).
     - Guard against entrance transients (e.g. pulsosActuales > 150).
     - Reset odometry and set pulse target to 400 pulses to stop in the center of the next cell.
   - If no lateral wall was detected from the start of the cell, fall back to the original 800 pulses count.
2. R2 (Alineación con la pared frontal):
   - In AVANZANDO, if moving toward a front wall (distanciaCent <= UMBRAL_PARED_FRENTE), do NOT stop at 800 or 400 pulses. Advance until front distance is <= 50 mm (sensadoActual.distanciaCent > 0 && sensadoActual.distanciaCent <= 50 && pulsosTotalesCelda > 100) and stop.
   - Front stop takes precedence over encoder stop (R2 overrides R1).
   - Include a safety watchdog pulse limit (e.g. 1050 pulses) to prevent collision if the sensor fails.
3. R3 (Gracia post-giro / Ceguera de PID):
   - At the beginning of AVANZANDO, PID correction added to wheel speeds must be held strictly at 0 during the first 100 pulses.
   - Crucial: Decouple this from R1's mid-cell encoder reset! R1's reset must NOT re-blind the PID. Use a dedicated latch (e.g. graciaPIDFinalizada) or total cell pulse counter.
   - Implement bumpless transfer for the PID: ensure errorAnterior does not cause a derivative kick (Kd * (error - 0)) when transitioning past 100 pulses (e.g. update errorAnterior or reset it during grace period).
4. R4 (Ausencia estricta de mapeo & FRENANDO unificado):
   - NO mapping matrices, X/Y tracking, or coordinate history.
   - All stop conditions in AVANZANDO (800 pulses fallback, 400 pulses post-edge, or <= 50 mm front stop) MUST cleanly transition through the FRENANDO state (movimiento(FRENO_F, {0,0}); tiempoInicioFreno = millis(); estadoPostFreno = DECISION; estado = FRENANDO;).
   - Ensure clean variable initialization on every entry into AVANZANDO (from FRENANDO, LISTO, DECISION).

VERIFICATION & BUILD:
- Try building with PlatformIO (`pio run` or `platformio run`). If the environment permits, verify that compilation passes with 0 errors. If PlatformIO CLI is not in PATH or fails, verify code syntax and integrity rigorously.
- Document in detail all changes, verification steps, and test results in handoff.md in your working directory.
- Send a completion message to parent upon finishing.
