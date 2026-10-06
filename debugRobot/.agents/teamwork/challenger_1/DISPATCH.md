## 2026-10-06T00:23:51Z
You are Challenger 1 (teamwork_preview_challenger) for Milestone M1 of the Micromouse Odometry Correction project.
Your working directory is:
c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\.agents\teamwork\challenger_1

MANDATORY FIRST STEP:
Read the authoritative user request at:
c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\.agents\teamwork\ORIGINAL_REQUEST.md

Read the project scope and worker report:
- Project Plan: c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\.agents\teamwork\orchestrator_1\PROJECT.md
- Worker Handoff: c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\.agents\teamwork\worker_m1\handoff.md

SOURCE CODE TO TEST/CHALLENGE:
Inspect the actual source code at:
- c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\src\main.cpp
- c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\src\config.h

OBJECTIVE:
Empirically and adversarially stress-test the logic implemented in main.cpp for R1 and R2.
Write a standalone test harness, simulator, or oracle script (e.g., Python or C++ unit test runner in your working directory) to simulate sensor and encoder traces and evaluate:
1. Falling edge scenarios: left wall dropping, right wall dropping, both dropping simultaneously, wall absent from start.
2. Odometry pulse counts: does the robot stop at 400 pulses after falling edge? Does it stop at 800 pulses when no wall exists?
3. Front wall approach: does robot advance past 800 pulses if front wall is detected until <= 50 mm? Does it brake safely if front wall is <= 50 mm early?
4. Corner cases: noisy sensor spikes, zero/negative readings, watchdog trigger at 1050 pulses.

OUTPUT REQUIREMENTS:
- Save your verification scripts in your working directory.
- Document test cases, outputs, and empirical proof in c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\.agents\teamwork\challenger_1\handoff.md.
- State your verdict: APPROVE or REQUEST_CHANGES.
- Send a completion message to parent.
