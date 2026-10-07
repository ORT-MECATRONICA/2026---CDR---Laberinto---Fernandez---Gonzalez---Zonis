# Original User Request

## 2026-10-06T00:02:17Z

# Teamwork Project Prompt — Draft

> Status: Launched
> Goal: Craft prompt → get user approval → delegate to teamwork_preview
> Requested team: [none — teamwork routes from the description]

Implementar tres mecánicas clásicas de corrección de odometría en la máquina de estados de un robot micromouse (C++) para evitar choques en las curvas. El equipo debe aplicar las mejoras sobre la estructura actual (main.cpp) de la manera más simple y robusta posible, e incluir una fuerte revisión teórica cruzada para garantizar cero bugs.

Working directory: c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot
Integrity mode: development

## Requirements

### R1. Reseteo por flanco lateral
En el estado AVANZANDO, si el robot viene detectando una pared lateral y esta desaparece (pasa de <130 a >130), debe resetear sus encoders y avanzar exactamente 400 pulsos (media celda) para frenar en el centro. Si no detecta paredes laterales desde el inicio, debe usar el conteo original de 800 pulsos como plan B.

### R2. Alineación con la pared frontal
Si el robot avanza hacia una celda con pared enfrente, no debe usar los encoders para frenar. Debe avanzar hasta que el sensor central lea exactamente 50 mm (o menos) de distancia y entonces detenerse para tomar la decisión.

### R3. Gracia post-giro (Ceguera del PID)
Al inicio del estado AVANZANDO, el controlador PID (la suma de la corrección a las velocidades base) debe mantenerse en 0 durante los primeros 100 pulsos, para evitar volantazos bruscos antes de que el robot se introduzca bien en el nuevo pasillo. 

### R4. Ausencia estricta de mapeo
Bajo ningún concepto se debe implementar lógica de mapeo (matrices, rastreo de coordenadas X/Y o historial). Solo aplicar las mecánicas de odometría.

## Acceptance Criteria

### Verificación Lógica (Review)
- [ ] R1: El código contempla correctamente la transición de estado "flanco de bajada" (distancia que baja de un valor a otro) sin bloquear el loop.
- [ ] R2: La condición de parada frontal sobreescribe el límite de encoders de forma segura (ej. no frena prematuramente si no hay pared).
- [ ] R3: Tras el reseteo de encoders al salir de un giro, el PID respeta el silencio de 100 pulsos.
- [ ] R4: La máquina de estados original sigue intacta en su arquitectura general, usando el estado `FRENANDO` introducido recientemente para estabilizar el robot.
- [ ] Un agente independiente revisó el código en busca de realimentaciones positivas ocultas o variables no inicializadas.

## 2026-10-07T15:12:25Z

# Teamwork Project Prompt

Review the entire `debugRobot` codebase to identify and document all potential bugs (logic errors, crashes, edge cases, and vulnerabilities). Do not modify any existing source code.

Working directory: c:\Users\devandroid\Documents\GitHub\Laberinto\debugRobot
Integrity mode: demo

## Requirements

### R1. Comprehensive Code Audit
Review all source files in the provided working directory. Identify logic errors, runtime crashes, unhandled edge cases, and security vulnerabilities. 

### R2. Read-Only Codebase
Absolutely no modifications may be made to the existing source files. 

### R3. Audit Methods
The team may execute static analysis tools or linters present in the environment, and consult external documentation to validate best practices.

## Acceptance Criteria

### Deliverable Format
- [ ] A detailed report is generated and saved as `audit_report.md` in the working directory.
- [ ] The report categorizes all found issues by severity (e.g., Critical, High, Medium, Low).
- [ ] Every documented bug includes the exact file path, line number, and a brief explanation of the issue.
