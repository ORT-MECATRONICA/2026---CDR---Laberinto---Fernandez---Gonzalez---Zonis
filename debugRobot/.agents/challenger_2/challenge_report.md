# Informe de Desafío Adversarial (Adversarial Challenge Report)
## Auditoría del Reporte Maestro: ug_report.md
**Agente Evaluador:** 	eamwork_preview_challenger (Instancia 2)  
**Fecha:** 2026-10-05  
**Proyecto:** debugRobot (MicroMouse ESP32)  
**Veredicto General:** **REQUEST_CHANGES** (Se identificaron omisiones y falsos negativos de severidad Alta y Media)

---

## 1. Challenge Summary

**Overall risk assessment**: **HIGH**

Si bien ug_report.md identifica con gran acierto 29 defectos primarios esenciales (incluyendo el bloqueo de compilación de src/CITÉ.cpp, la omisión de reak; en la FSM, la inversión de polaridad cinemática y la falta de setpoint en el PID), la auditoría adversarial demostró que el reporte presenta **omisiones significativas (falsos negativos)** en aspectos críticos de control dinámico, inicialización de hardware de potencia, saturación de particiones de memoria flash y arquitectura de telemetría.

---

## 2. Challenges & Omissions Identificadas

### [High] Challenge 1: Omisión de UMBRAL_PARED_FRENTE (120 mm) y Sustitución Inválida en la FSM
- **Suposición Desafiada:** ug_report.md asume que todas las reglas de umbral de paredes en la FSM se limitan al defecto BUG-10 (desigualdades estrictas).
- **Escenario de Falla:**
  En src/config.h:37:
  `cpp
  //Es el umbral (MM) para que el robot gire si la pared está frente a él
  #define UMBRAL_PARED_FRENTE 120
  `
  Sin embargo, en src/main.cpp:64-66:
  `cpp
  bool condicionAvanzar = sensadoActual.distanciaDer < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaCent > UMBRAL_PARED_ESTADO_NORMAL;
  bool condicionGiroIzq = sensadoActual.distanciaDer < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaCent < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaIzq > UMBRAL_PARED_ESTADO_NORMAL;
  bool condicionGiro180 = sensadoActual.distanciaDer < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaCent < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaIzq < UMBRAL_PARED_ESTADO_NORMAL;
  `
  El sensor frontal posee un offset mecánico sustancialmente diferente (OFSET_CENT = 80 mm frente a 40 y 47 mm de los laterales). El desarrollador definió deliberadamente UMBRAL_PARED_FRENTE = 120 mm para el sensor central, pero en main.cpp fue omitido y reemplazado por UMBRAL_PARED_ESTADO_NORMAL = 130 mm.
- **Blast Radius:**
  Cuando el robot avanza hacia una pared frontal y la distancia medida se sitúa entre 121 y 130 mm, el robot interpreta prematuramente que el frente está cerrado (distanciaCent < 130), abortando el avance y forzando un giro no deseado a una celda incompleta. UMBRAL_PARED_FRENTE queda como código muerto inerte.
- **Mitigación:**
  Actualizar main.cpp para comparar distanciaCent exclusivamente contra UMBRAL_PARED_FRENTE.

---

### [High] Challenge 2: Discrepancia Crítica de Detección de Pared entre FSM (130 mm) y PID (180 mm)
- **Suposición Desafiada:** ug_report.md no analiza la coherencia de umbrales espaciales entre el subsistema de navegación y el subsistema de control.
- **Escenario de Falla:**
  En src/main.cpp:63:
  `cpp
  bool condicionGiroDer = sensadoActual.distanciaDer > UMBRAL_PARED_ESTADO_NORMAL; // 130 mm
  `
  Si distanciaDer > 130 mm, la FSM asume que **NO hay pared**.
  En contraste, en src/hardware/movimiento/PID.cpp:8-10:
  `cpp
  bool hayDer = mediciones.distanciaDer < (UMBRAL_PARED_ESTADO_NORMAL + 50); // 130 + 50 = 180 mm!
  `
  Para el controlador PID, cualquier lectura inferior a 180 mm se considera **presencia válida de pared**.
- **Blast Radius:**
  En el rango de 131 mm a 179 mm (un ancho de 49 mm, representativo de esquinas o bifurcaciones en celdas de 180 mm):
  1. Si se transita por una esquina abierta, el sensor lateral detecta la pared lejana del pasillo continuo a ~150 mm.
  2. La FSM comanda avance o giro, pero el PID calcula: error = (int16_t)150 -> corrección = +25.
  3. El robot acelera la rueda izquierda (70 PWM) y frena la derecha (20 PWM), virando bruscamente hacia la derecha contra el pilar de la esquina abierta en lugar de mantener la línea central.
- **Mitigación:**
  Unificar el criterio de presencia de pared en una función común de percepción y acotar la ventana de seguimiento de pared a distancias físicas coherentes con el ancho del pasillo (ej. $\le 90\text{ mm}$).

---

### [High] Challenge 3: Riesgo de Desbordamiento de Flash por Partición por Defecto con BluetoothSerial
- **Suposición Desafiada:** ug_report.md evalúa Bluetooth únicamente desde la perspectiva de buffer flooding y fragmentación de heap, pero omite el tamaño binario en Flash.
- **Escenario de Falla:**
  La compilación empírica con pio run arrojó:
  Flash: [========= ] 86.7% (used 1,135,869 bytes from 1,310,720 bytes).
  La pila Bluedroid de Bluetooth Clásico (BluetoothSerial) consume más de 1.1 MB de código ejecutable. Con el esquema de partición por defecto del ESP32 (default.csv, tamaño de app0 = 1.25 MB), solo quedan **174 KB libres** de memoria Flash.
- **Blast Radius:**
  Cualquier intento de incorporar algoritmos reales de MicroMouse (FloodFill, matrices de mapeo 16x16, persistencia en NVS/Preferences o FreeRTOS) provocará un fallo fatal de enlace:
  irmware.elf section .flash.text will not fit in region app0.
- **Mitigación:**
  Declarar explícitamente en platformio.ini:
  oard_build.partitions = huge_app.csv (que reserva 3 MB para la aplicación) o reemplazar BluetoothSerial por UART físico / BLE.

---

### [Medium] Challenge 4: Ausencia de Cláusula default: en puenteH.cpp:switch (movimiento)
- **Suposición Desafiada:** Se asume que el actuador de puente H siempre recibe enums válidos.
- **Escenario de Falla:**
  En src/hardware/movimiento/puenteH.cpp:18-67, la función movimiento evalúa un switch (movimiento) sin etiqueta default:.
  Si por corrupción de memoria, carrera de estado o paquete de telemetría corrupto se pasa un valor fuera del rango [0..4], el bloque se salta sin modificar ni los pines de dirección ni los canales PWM.
- **Blast Radius:**
  Los motores continúan girando a la velocidad y sentido previo en bucle abierto sin capacidad de parada de emergencia, provocando colisiones físicas a velocidad de marcha.
- **Mitigación:**
  Añadir default: movimiento(FRENO_F, {0,0}); break; para garantizar detención ante estados no previstos.

---

### [Medium] Challenge 5: Falta de Puesta a Cero de Pines y Canales PWM en inicializarMotores()
- **Suposición Desafiada:** inicializarMotores() deja los actuadores en estado seguro tras el arranque.
- **Escenario de Falla:**
  En src/hardware/movimiento/puenteH.cpp:6-16:
  `cpp
  void inicializarMotores(){
      pinMode(AIN1, OUTPUT);
      pinMode(AIN2, OUTPUT);
      pinMode(BIN1, OUTPUT);
      pinMode(BIN2, OUTPUT);
      ledcSetup(0, 20000, 8);
      ledcSetup(1, 20000, 8);
      ledcAttachPin(PWMA, 0);
      ledcAttachPin(PWMB, 1);
  }
  `
  No se ejecuta digitalWrite(pin, LOW) en las 4 líneas de dirección ni ledcWrite(ch, 0) en los canales PWM.
- **Blast Radius:**
  Durante el arranque del microcontrolador, transitorios eléctricos o estados residuales del registro LEDC pueden generar impulsos espurios de tracción antes de que loop() tome el control.
- **Mitigación:**
  Llamar explícitamente a movimiento(FRENO_F, {0,0}); al final de inicializarMotores().

---

### [Medium] Challenge 6: Telemetría UART Silenciosa a Pesar de Serial.begin(115200)
- **Suposición Desafiada:** El sistema cuenta con registro serial activo.
- **Escenario de Falla:**
  En src/hardware/logger/logger.cpp:17, se inicializa Serial.begin(115200). Sin embargo, en todo src/main.cpp, todos los mensajes se envían únicamente mediante enviarString() (SerialBT.println()). No existe una sola llamada a Serial.print() en el lazo principal.
- **Blast Radius:**
  Cuando un ingeniero conecta el robot mediante cable USB al Monitor Serial de PlatformIO (monitor_speed = 115200), la consola permanece completamente muda durante toda la ejecución. Si no hay un smartphone o PC conectado por Bluetooth, el sistema es completamente inobservable.
- **Mitigación:**
  Hacer que enviarString() replique la salida tanto a SerialBT como a Serial.println(str).

---

### [Medium] Challenge 7: Asincronismo Temporal entre Sensores VL53L0X en ctualizarSensado()
- **Suposición Desafiada:** La lectura de los tres sensores es instantánea y coherente temporalmente.
- **Escenario de Falla:**
  En sensoresDistancia.cpp:89-103, cada sensor se evalúa por sondeo no sincronizado. Debido a la deriva de los osciladores internos de los sensores (periodo de medición de ~30-33 ms), en una misma iteración de control se puede estar comparando una distancia izquierda de hace 1 ms con una distancia derecha de hace 32 ms.
- **Blast Radius:**
  A 0.5 m/s, 31 ms de diferencia equivalen a 15.5 mm de avance lineal del robot. La resta {\text{der}} - d_{\text{izq}}$ no representa la posición actual centrada, sino una falsa distorsión angular que provoca oscilaciones de centrado.
- **Mitigación:**
  Sincronizar el ciclo de lectura o usar el pin de interrupción por hardware de los sensores para registrar muestras en el mismo instante espacial.

---

## 3. Stress Test Results

| Prueba / Escenario | Comportamiento Esperado | Comportamiento Real Observado / Simulado | Resultado |
|---|---|---|---|
| **Compilación con fuentes completas (pio run)** | Detección de fallo de compilación | Aborto inmediato por src/CITÉ.cpp (BUG-01) | **CONFIRMADO** |
| **Compilación filtrando archivos transitorios** | Compilación exitosa del binario | Éxito (irmware.bin generado, Flash al 86.7%) | **CONFIRMADO** |
| **Consistencia Umbral Frontal (125 mm)** | Reconocer frente abierto (> 120) | Reconoce frente cerrado (< 130) por omisión de UMBRAL_PARED_FRENTE | **FALLO (BUG OMITIDO)** |
| **Consistencia Pared FSM vs PID (150 mm)** | Criterio unificado de pared ausente | FSM evalúa Ausente; PID evalúa Presente e induce error de 150 mm | **FALLO (BUG OMITIDO)** |
| **Límite de Flash de Partición ESP32** | Espacio holgado para algoritmo de laberinto | Solo 174 KB disponibles antes de overflow de app0 | **RIESGO CRÍTICO OMITIDO** |
| **Monitor Serial USB a 115200 baudios** | Telemetría en tiempo real por cable | Mudo total en main.cpp (solo transmite a Bluetooth) | **FALLO (BUG OMITIDO)** |

---

## 4. Unchallenged Areas

- **Capa Física de Silicio ESP32 (BUG-28, BUG-29):** La advertencia del pin MTDI (GPIO 12) y el botón sin pull-up en GPIO 34 son correctas y se confirman con la hoja de datos técnica de Espressif ESP32 Technical Reference Manual.
- **Defectos Cinemáticos (BUG-13, BUG-14, BUG-19):** El factor / 2 en encoders, la polaridad invertida en el puente H y el valor erróneo de 300 pulsos para 180° son inobjetables y están sólidamente fundamentados.

---

## 5. Veredicto y Recomendaciones de Acción

**Veredicto:** **REQUEST_CHANGES**

Para que el reporte maestro ug_report.md alcance el estándar de calidad exhaustivo exigido:
1. Incorporar los **7 defectos adicionales identificados** (Challenge 1 al 7).
2. Asignarles IDs formales (BUG-30 a BUG-36).
3. Actualizar la matriz de severidad y el plan de remediación en fases para incluir la reconfiguración de la tabla de particiones (huge_app.csv) y la unificación de umbrales espaciales FSM/PID.
