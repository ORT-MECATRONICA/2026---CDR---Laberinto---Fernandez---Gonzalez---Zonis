# Handoff Report — Adversarial Challenge of ug_report.md
**Instancia:** 	eamwork_preview_challenger (Instancia 2)  
**Fecha:** 2026-10-05  
**Veredicto:** **REQUEST_CHANGES**

---

## 1. Observation

A través de la inspección estática, análisis algorítmico y ejecución empírica de pruebas con la cadena de herramientas PlatformIO Core 6.2.0 y xtensa-esp32-elf-g++ sobre el repositorio debugRobot, se observaron los siguientes hechos directamente verificables:

1. **Bloqueo de Compilación Verificado Empíricamente:**
   Al ejecutar pio run sobre el proyecto original, el compilador abortó con código de error 1:
   `	ext
   xtensa-esp32-elf-g++: error: src/CIT.cpp: No such file or directory
   xtensa-esp32-elf-g++: fatal error: no input files
   compilation terminated.
   *** [.pio\build\esp32doit-devkit-v1\src\CIT.cpp.o] Error 1
   `
   Confirmando de manera empírica la observación de BUG-01 en ug_report.md.

2. **Saturación Crítica de Partición Flash (86.7%):**
   Al ejecutar la compilación aislando los archivos transitorios mediante uild_src_filter = +<*> -<CIT*.cpp> -<*.txt> -<pruebaEncoders>, el enlazador completó con éxito pero arrojó:
   `	ext
   RAM:   [=         ]  12.4% (used 40588 bytes from 327680 bytes)
   Flash: [========= ]  86.7% (used 1135869 bytes from 1310720 bytes)
   `
   Dejando únicamente 174,851 bytes (174 KB) de Flash disponible en la partición por defecto pp0 de 1.25 MB. Esta métrica y su riesgo de desbordamiento de enlazador ante futuras expansiones de código fueron omitidos en ug_report.md.

3. **Definición Omitida de UMBRAL_PARED_FRENTE:**
   En src/config.h:37:
   `cpp
   #define UMBRAL_PARED_FRENTE 120
   `
   En src/main.cpp:64-66:
   `cpp
   bool condicionAvanzar = sensadoActual.distanciaDer < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaCent > UMBRAL_PARED_ESTADO_NORMAL;
   bool condicionGiroIzq = sensadoActual.distanciaDer < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaCent < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaIzq > UMBRAL_PARED_ESTADO_NORMAL;
   bool condicionGiro180 = sensadoActual.distanciaDer < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaCent < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaIzq < UMBRAL_PARED_ESTADO_NORMAL;
   `
   UMBRAL_PARED_FRENTE no se utiliza en ninguna parte de src/main.cpp. En su lugar, se compara distanciaCent contra UMBRAL_PARED_ESTADO_NORMAL (130 mm).

4. **Inconsistencia Espacial FSM vs PID:**
   En src/main.cpp:63:
   `cpp
   bool condicionGiroDer = sensadoActual.distanciaDer > UMBRAL_PARED_ESTADO_NORMAL; // 130
   `
   En src/hardware/movimiento/PID.cpp:8-10:
   `cpp
   bool hayDer = mediciones.distanciaDer < (UMBRAL_PARED_ESTADO_NORMAL + 50); // 180
   `
   Existe una brecha de 49 mm (entre 131 mm y 179 mm) en la que la FSM dictamina que la pared derecha no existe, mientras que el lazo PID dictamina que la pared sí existe y aplica corrección proporcional-derivativa completa.

5. **Omisión de default: en puenteH.cpp:**
   En src/hardware/movimiento/puenteH.cpp:18-67, switch (movimiento) contiene case AVANZAR:, RETROCEDER:, GIRAR_DER:, GIRAR_IZQ:, FRENO_F:, pero carece por completo de sentencia default:.

6. **Falta de Reset de PWM en inicializarMotores():**
   En src/hardware/movimiento/puenteH.cpp:6-16, se configuran los pines y canales con ledcSetup y ledcAttachPin, pero no se invocan llamadas a ledcWrite(0, 0) ni digitalWrite(pin, LOW).

7. **Silencio de UART a Pesar de Serial.begin(115200):**
   En src/hardware/logger/logger.cpp:17, se inicializa Serial.begin(115200). En src/main.cpp, todos los registros se despachan con enviarString() exclusivamente a SerialBT. En todo el archivo src/main.cpp no existe una sola sentencia Serial.print.

8. **Verificación de Integridad de Código Fuente:**
   Al ejecutar git status, se comprobó que ningún archivo fuente del proyecto ha sido modificado por el presente agente durante el proceso de auditoría y prueba.

---

## 2. Logic Chain

1. A partir de la Observación 1, se verifica que la afirmación de ug_report.md sobre el fallo de compilación inducido por src/CITÉ.cpp es 100% verídica y reproducible.
2. A partir de la Observación 2, se demuestra que la inclusión de BluetoothSerial consume el 86.7% de la partición de 1.25 MB. Sin la directiva oard_build.partitions = huge_app.csv en platformio.ini, la incorporación del algoritmo de laberinto (requisito esencial de MicroMouse) desbordará la memoria flash disponible, constituyendo un riesgo crítico no advertido en ug_report.md.
3. A partir de la Observación 3, se constata que UMBRAL_PARED_FRENTE (120 mm) fue ignorado en src/main.cpp, usando en su lugar 130 mm. Esto induce virajes prematuros ante distancias frontales entre 121 y 130 mm, constituyendo un defecto lógico de navegación omitido en el reporte.
4. A partir de la Observación 4, se deduce que un robot navegando por un cruce o bifurcación experimentará perturbaciones dinámicas severas: la FSM tomará la decisión de girar o avanzar en espacio abierto mientras el controlador PID intentará simultáneamente corregir trayectoria contra una pared lejana inexistente, defecto omitido en ug_report.md.
5. A partir de las Observaciones 5, 6 y 7, se concluye que existen vulnerabilidades adicionales de seguridad en el actuador del puente H (falta de parada segura ante estados no enumerados y falta de puesta a cero en inicialización) e inobservabilidad del sistema vía UART.
6. En consecuencia, aunque ug_report.md es exhaustivo en los 29 defectos documentados, adolece de omisiones de severidad Alta y Media que deben integrarse para que el informe sea técnicamente completo y riguroso.

---

## 3. Caveats

- No se dispuso del chasis físico ni del circuito impreso (PCB) para medir oscilogramas en tiempo real; las comprobaciones cinemáticas y dinámicas se basaron en análisis matemático estático y modelos cinemáticos de tracción diferencial.
- Se asume que el robot compite en laberintos estándar MicroMouse (IEEE/RoboCore) con celdas de 180 mm x 180 mm.

---

## 4. Conclusion

**Veredicto:** **REQUEST_CHANGES**

Se solicita formalmente al redactor del reporte maestro ug_report.md la incorporación de las siguientes correcciones y adiciones:
1. Incorporar la omisión del umbral frontal UMBRAL_PARED_FRENTE frente a UMBRAL_PARED_ESTADO_NORMAL en main.cpp:64-66.
2. Documentar la discrepancia y conflicto de control entre el umbral de pared de la FSM (130 mm) y el umbral del lazo PID (180 mm en PID.cpp:8-10).
3. Registrar el riesgo inminente de desbordamiento de memoria Flash (86.7% ocupada) y la recomendación de partición huge_app.csv en platformio.ini.
4. Añadir las observaciones de robustez de motor (default: en puenteH.cpp:switch y puesta a cero en inicializarMotores()).
5. Recomendar la duplicación de telemetría hacia Serial físico en enviarString() para permitir depuración por cable USB a 115200 baudios.

---

## 5. Verification Method

Para verificar independientemente los hallazgos documentados:

1. **Verificación de Compilación y Tamaño Flash:**
   Ejecutar desde el directorio del proyecto:
   `powershell
   & "C:\Users\devandroid\.platformio\penv\Scripts\pio.exe" run -c ".agents\challenger_2\test_platformio.ini" -d "."
   `
   Comprobar en la salida que la memoria Flash alcanza el 86.7% (used 1135869 bytes from 1310720 bytes).
2. **Verificación de Umbrales FSM vs PID:**
   Inspeccionar src/config.h (Línea 37), src/main.cpp (Líneas 64-66) y src/hardware/movimiento/PID.cpp (Líneas 8-10).
3. **Verificación de Inmutabilidad de Fuentes:**
   Ejecutar git status y verificar que los archivos fuente originales no fueron modificados durante esta fase.
