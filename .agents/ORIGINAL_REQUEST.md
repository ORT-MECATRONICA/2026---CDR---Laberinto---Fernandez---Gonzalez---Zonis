# Original User Request

## 2026-09-20T00:39:30Z

# Teamwork Project Prompt — Draft

> Status: Ready for launch — awaiting user approval
> Goal: Craft prompt → get user approval → delegate to teamwork_preview
> Requested team: Full team

Reescribir el algoritmo de resolución de laberintos de `rightHand.cpp` para un robot micromouse. Implementar un filtro de mediana para los sensores VL53L0X, un sistema de "debounce" para las transiciones de estado, y la detección correcta de callejones sin salida (giro de 180 grados).

Working directory: c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\bahiaBlanca
Integrity mode: development

## Requirements

### R1. Filtrado de hardware (Filtro de Mediana)
Modificar el módulo `sensoresDistancia` para aplicar un Filtro de Mediana a las lecturas crudas del láser VL53L0X. Este valor filtrado debe ser el que consuman los algoritmos de navegación.

### R2. Algoritmo de Navegación y Transiciones (Debounce)
Reescribir `rightHand.cpp` para que sea una máquina de estados no bloqueante que retorne el estado principal. Las transiciones entre estados internos (ej: decidir doblar) deben usar un sistema de "debounce" exigiendo $N$ lecturas consecutivas para evitar falsos positivos.

### R3. Detección de Callejón Sin Salida (180 grados)
Implementar la lógica para detectar un callejón sin salida evaluando simultáneamente la presencia de paredes en los sensores izquierdo, derecho y central, y transicionar al estado de giro correspondiente.

### R4. Lógica de centrado en intersecciones (Estados previos al giro)
Completar todos los estados restantes en `rightHand.cpp`. Al detectar un camino abierto, el robot no debe girar instantáneamente. Debe transicionar a los estados de preparación (ej: `PREPARANDOME_PARA_GIRAR_DER`) que aseguren que el robot avance la distancia justa para ubicar su eje de rotación en el centro de la intersección antes de iniciar el giro.

### R5. Protección del PID ante ausencias de pared
Ajustar la lógica de cálculo del error del PID. Si un sensor lateral detecta una distancia excesivamente grande (indicando un hueco/camino abierto), el algoritmo PID debe omitir esa pared y no intentar corregir hacia ese lado, evitando que el robot se desvíe o se estrelle contra el vértice del pasillo.

## Acceptance Criteria

### Verificación de compilación (Programática)
- [ ] Ejecutar compilación de PlatformIO (ej. `pio run`). El proyecto debe compilar exitosamente sin errores de redefinición de variables ni errores de sintaxis en los archivos modificados.

### Revisión de Lógica y Calidad (Agente Juez / Rúbrica)
- [ ] El filtro de "debounce" implementado reinicia correctamente sus contadores cuando la condición se rompe.
- [ ] La detección del callejón sin salida evalúa la presencia de las 3 paredes simultáneamente.
- [ ] El PID cuenta con una cláusula de guarda matemática para ignorar grandes vacíos.
- [ ] Calidad de Producción: No existen números mágicos en el código (todos en `config.h`).
