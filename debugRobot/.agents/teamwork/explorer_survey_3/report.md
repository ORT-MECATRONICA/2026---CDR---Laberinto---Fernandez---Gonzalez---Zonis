# Reporte de Investigación Técnica: Requisitos R3, R4 y Análisis de Riesgos Teóricos Cruzados

**Proyecto:** debugRobot (MicroMouse ESP32)  
**Agente:** Explorer 3 (`teamwork_preview_explorer`)  
**Directorio de Trabajo:** `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\.agents\teamwork\explorer_survey_3`  
**Fecha:** 2026-10-06  
**Objetivo:** Análisis exhaustivo de los requisitos R3 (Gracia post-giro / Ceguera de PID) y R4 (Ausencia estricta de mapeo y preservación del estado FRENANDO), junto con una auditoría teórica cruzada de riesgos (variables no inicializadas, realimentaciones positivas, división por cero, ruido/timeout de sensores, desbordamiento de encoders).

---

## 1. Análisis Profundo del Requisito R3: Gracia Post-Giro (Ceguera del PID)

### 1.1 Estado Actual del PID: Cálculo e Inyección a Velocidades Base
En el firmware actual (`src/main.cpp`, líneas 72-93):
- El cálculo y aplicación del PID ocurre exclusivamente dentro del estado `AVANZANDO`:
  ```cpp
  pulsosActuales = (abs(verPulsosEncoderA()) + abs(verPulsosEncoderB()))/2;
  
  if (pulsosActuales < PULSOS_CELDA) {
    sensadoActual = actualizarSensado();
    int16_t correccion = calcularCorreccion(sensadoActual);
    
    // Motor A (izquierda en struct) está físicamente en rueda DERECHA
    // Motor B (derecha en struct) está físicamente en rueda IZQUIERDA
    velocidadActual.izquierda = constrain(VEL_BASE_DER - correccion, 0, 255);
    velocidadActual.derecha = constrain(VEL_BASE_IZQ + correccion, 0, 255);
    
    movimiento(AVANZAR, {velocidadActual.izquierda, velocidadActual.derecha});
  }
  ```
- La función `calcularCorreccion(sensadoActual)` se ubica en `src/hardware/movimiento/PID.cpp` (líneas 7-33):
  ```cpp
  int16_t calcularCorreccion(sensado mediciones){
      bool hayIzq = mediciones.distanciaIzq < (UMBRAL_PARED_ESTADO_NORMAL + 50);
      bool hayDer = mediciones.distanciaDer < (UMBRAL_PARED_ESTADO_NORMAL + 50);
      int16_t error = 0;
      if (hayIzq && hayDer) {
          error = (int16_t)mediciones.distanciaDer - (int16_t)mediciones.distanciaIzq;
      } else if (hayIzq) {
          error = - (int16_t)mediciones.distanciaIzq;
      } else if (hayDer) {
          error = (int16_t)mediciones.distanciaDer;
      } else {
          error = 0;
      }
      int16_t correccion = (KP * error) + (KD * (error - errorAnterior));
      errorAnterior = error; 
      return constrain(correccion, -25,25);
  }
  ```
- Constantes asociadas en `src/config.h`:
  - `KP = 0.5`, `KI = 0`, `KD = 0.3`.
  - `VEL_BASE_DER = 45`, `VEL_BASE_IZQ = 45`.
  - La corrección modulada se restringe a $[-25, 25]$, sumándose a una rueda y restándose a la otra.

### 1.2 Transición desde Estados de Giro hacia AVANZANDO
En `src/main.cpp`, las secuencias de giro (`GIRANDO_DER`, `GIRANDO_IZQ`, `GIRANDO_180`) finalizan de manera no bloqueante cuando la cuenta de pulsos alcanza el umbral de calibración:
- `GIRANDO_DER` (líneas 131-137): `pulsosActuales >= PULSOS_GIRO_90_DER`
- `GIRANDO_IZQ` (líneas 146-152): `pulsosActuales >= PULSOS_GIRO_90_IZQ`
- `GIRANDO_180` (líneas 161-167): `pulsosActuales >= PULSOS_GIRO_180`

En todos los casos, la transición **NO ingresa directamente a `AVANZANDO`**, sino que pasa por el estado `FRENANDO`:
```cpp
movimiento(FRENO_F, {0,0});
tiempoInicioFreno = millis();
estadoPostFreno = AVANZANDO;
estado = FRENANDO;
```
Dentro de `case FRENANDO:` (líneas 170-179):
```cpp
movimiento(FRENO_F, {0,0}); // Freno dinámico activo durante 150 ms
if (millis() - tiempoInicioFreno >= 150) {
  resetearErrorAnterior();  // Pone errorAnterior = 0
  resetearEncoders();        // Pone encoders A y B en 0
  if (estadoPostFreno == AVANZANDO) enviarString(">>> AVANZANDO <<<");
  estado = estadoPostFreno; // Conmuta a AVANZANDO
}
```
**Observación Clave:** Al entrar a `AVANZANDO` luego de un giro, los encoders inician exactamente en `0`, y `errorAnterior` inicia en `0`.

### 1.3 Rastreo de los Primeros 100 Pulsos y Forzado de Corrección Cero
El requisito R3 exige que durante los primeros 100 pulsos dentro de `AVANZANDO`, la corrección PID sea estrictamente `0` (ambas ruedas a su velocidad base simétrica `45` PWM), evitando volantazos causados por lecturas transitorias de las aristas o esquinas de la celda entrante.

#### Riesgos y Trampas en la Implementación de R3:
1. **Pico Derivativo (Derivative Kick) al reactivar el PID:**
   - La fórmula derivativa es $K_d \cdot (e_k - e_{k-1})$.
   - Si durante los 100 pulsos se silencia el cálculo y `errorAnterior` permanece en `0`, en el pulso 101 el robot tomará una lectura con un error de posición (por ejemplo $e_{101} = 24\text{ mm}$).
   - La componente derivativa calculará $0.3 \times (24 - 0) = +7.2$, generando un tirón brusco instantáneo en lugar de una derivada suave.
   - **Solución Recomendada (Bumpless Transfer):** Existen dos alternativas limpias:
     - *Opción A (Filtro en la salida):* Permitir que `calcularCorreccion()` se compute internamente en cada ciclo (actualizando `errorAnterior`), pero forzar la variable local de control a cero:
       ```cpp
       int16_t correccion = 0;
       if (pulsosActuales >= 100) {
           correccion = calcularCorreccion(sensadoActual);
       } else {
           // Se actualiza el historial de error para que cuando se supere 100 pulsos
           // no exista un salto artificial (error - 0)
           calcularCorreccion(sensadoActual); // o función actualizarErrorPrecedente()
           correccion = 0;
       }
       ```
     - *Opción B (Inicialización en la frontera del pulso 100):* Usar una bandera de gracia `graciaPostGiroActiva`. En el ciclo exacto en que `pulsosActuales >= 100`, se llama a una función `sincronizarPID(errorActual)` que fije `errorAnterior = errorActual`, garantizando que $\Delta e = 0$ en el primer ciclo activo.
2. **Windup Integral:**
   - En la configuración actual, $K_i = 0$. Por lo tanto, no hay acumulador integral activo ni riesgo de saturación integral. Sin embargo, para mantener arquitectura limpia, si a futuro se habilitase $K_i$, el acumulador debe mantenerse en cero durante la ceguera.
3. **Interferencia entre R3 y R1 (Flanco de Caída Lateral):**
   - El requisito R1 especifica que al detectar la caída de pared lateral (< 130 mm a > 130 mm), los encoders se resetean a 0 para avanzar 400 pulsos hacia el centro.
   - **RIESGO CRÍTICO:** Si la condición de gracia post-giro se programa ingenuamente como `if (pulsosActuales < 100) correccion = 0;`, entonces cuando R1 resetee los encoders a la mitad de la celda, ¡el PID volverá a quedar ciego durante 100 pulsos (el 25% del recorrido restante)!
   - **Solución de Aislamiento:** Implementar una bandera explícita:
     ```cpp
     bool graciaPID = true; // Se activa al salir de FRENANDO hacia AVANZANDO
     ```
     Una vez que `pulsosActuales >= 100`, se desactiva `graciaPID = false;`.
     Si posteriormente R1 resetea los encoders en esa celda, `graciaPID` permanece en `false`, evitando cegar el PID indebidamente a mitad del pasillo.

---

## 2. Análisis del Requisito R4: Ausencia Estricta de Mapeo y Preservación de FRENANDO

### 2.1 Verificación de Ausencia de Mapeo en el Código Base
Se realizó un relevamiento completo de los archivos fuente:
- No existen matrices 2D ni representaciones cartesianas ($X, Y$) de la cuadrícula.
- No hay pilas ni colas de FloodFill, nodos de grafos, historiales de celdas visitadas ni registros de orientación absoluta (Norte, Sur, Este, Oeste).
- **Constantes heredadas en `src/config.h`:**
  - Líneas 7-11: `#define X_SIZE 10`, `#define Y_SIZE 10`, `#define X_START 5`, `#define Y_START 5`.
  - Estas directivas están huérfanas: no son referenciadas en ninguna parte de `src/main.cpp` ni en los módulos de hardware.
  - La navegación es 100% reactiva mediante odometría incremental y lectura sensorial directa. Se confirma el estricto cumplimiento de R4.

### 2.2 Dinámica y Funcionamiento del Estado FRENANDO
El estado `FRENANDO` fue incorporado recientemente como un buffer de disipación inercial y sincronización estática:
- **Mecanismo:**
  1. El estado emisor comanda `movimiento(FRENO_F, {0,0})`.
  2. Registra la marca de tiempo: `tiempoInicioFreno = millis();`.
  3. Define el estado destino: `estadoPostFreno = ...;`.
  4. Conmuta a `estado = FRENANDO;`.
- **Comportamiento en `FRENANDO`:**
  - Mantiene `movimiento(FRENO_F, {0,0})` en cada iteración del bucle.
  - Comprueba de forma no bloqueante: `if (millis() - tiempoInicioFreno >= 150)`.
  - Al expirar los 150 ms:
    - Ejecuta `resetearErrorAnterior();` (blanqueo de estado PID).
    - Ejecuta `resetearEncoders();` (blanqueo de contadores PCNT).
    - Conmuta directamente a `estado = estadoPostFreno;`.

### 2.3 Transición Homogénea desde AVANZANDO hacia FRENANDO
Bajo la arquitectura de los requisitos R1, R2 y R4, el estado `AVANZANDO` tiene tres condiciones de detención:
1. **Odometría Fallback (Plan B):** Transcurridos 800 pulsos (`PULSOS_CELDA`) sin eventos sensoriales.
2. **Odometría por Flanco Lateral (R1):** Transcurridos 400 pulsos desde la detección del flanco de caída.
3. **Alineación con Pared Frontal (R2):** Cuando el sensor central detecta pared a $\le 50\text{ mm}$.

**Regla de Oro Arquitectónica:** En **CUALQUIERA** de las tres condiciones de detención, la salida de `AVANZANDO` debe converger de manera idéntica y estandarizada hacia `FRENANDO`:
```cpp
movimiento(FRENO_F, {0,0});
tiempoInicioFreno = millis();
estadoPostFreno = DECISION;
estado = FRENANDO;
```
**Justificación:**
- Evita el cabeceo mecánico y el deslizamiento inercial (*wheel slip*) que contaminaría las lecturas en `DECISION`.
- En `DECISION`, el robot debe encontrarse en reposo absoluto para que los sensores VL53L0X midan sin aberración por movimiento y se evalúe con total precisión el orden de prioridad de giro.
- Asegura que `resetearEncoders()` y `resetearErrorAnterior()` se ejecuten de manera determinística antes de iniciar la siguiente maniobra.

---

## 3. Matriz de Riesgos Teóricos Cruzados y Recomendaciones de Seguridad

| ID de Riesgo | Naturaleza | Ubicación Potencial | Descripción del Fallo | Severidad | Mitigación Recomendada |
|---|---|---|---|---|---|
| **RSK-01** | Variable no inicializada | `src/main.cpp` | Banderas de estado para R1 (`flancoDetectado`, `metaPulsos`) o R3 (`graciaPID`) sin inicializar o sin rearmar al ingresar a `AVANZANDO`. | **CRÍTICA** | Declarar e inicializar explícitamente en el ámbito global o reiniciar obligatoriamente al entrar a `AVANZANDO` desde `FRENANDO`/`DECISION`. |
| **RSK-02** | Realimentación positiva | `src/hardware/movimiento/PID.cpp:17-23` (BUG-16) | Ausencia de consigna (*setpoint*) en centrado con una sola pared: $e = -distIzq$ o $e = distDer$. Si el robot se arrima a la pared izquierda, el error lo empuja más hacia la izquierda hasta colisionar. | **CRÍTICA** | Sustraer la distancia de referencia ideal: $e = \text{DIST\_OBJ} - distIzq$ y $e = distDer - \text{DIST\_OBJ}$. |
| **RSK-03** | Oscilación y reseteo infinito | `src/main.cpp` (R1) | Ruidos de lectura alrededor de 130 mm (ej. 129 mm $\to$ 131 mm $\to$ 128 mm) disparando múltiples reseteos de encoders consecutivos. El robot nunca completaría los 400 pulsos. | **CRÍTICA** | Incorporar un cerrojo biestable (*latch*): una vez detectado el flanco (`flancoDetectado = true`), no volver a evaluar el flanco hasta la siguiente celda. |
| **RSK-04** | Falsa parada frontal en arranque | `sensoresDistancia.cpp:83` vs `src/main.cpp` (R2) | `lecturaAct` inicializa en `{0,0,0}`. Si en el primer ciclo de avance el sensor central aún no completó su medición, `distanciaCent` reporta $\le 0\text{ mm}$, disparando parada frontal inmediata en celda vacía. | **ALTA** | Validar que la lectura sea físicamente admisible: `distanciaCent > 0 && distanciaCent <= 50`, o condicionar la parada a haber recorrido una distancia mínima (ej. `pulsosActuales > 100`). |
| **RSK-05** | Pico de par en reenganche PID | `PID.cpp:28` (R3) | Silenciar el PID durante 100 pulsos manteniendo `errorAnterior = 0`. Al pulso 101, la componente derivativa salta proporcional al error completo en lugar de la tasa de variación. | **MEDIA** | Actualizar `errorAnterior` durante el periodo de gracia o igualar `errorAnterior = error` en el primer ciclo post-gracia. |
| **RSK-06** | División por cero | Cálculo de $\Delta t$ o encoders | Si se normaliza la derivada por $\Delta t$ sin proteger `if (dt <= 0)`. | **MEDIA** | Forzar cota inferior `if (dt < 0.001f) dt = 0.001f;` o mantener cómputo a tasa periódica fija. En encoders, el denominador `2` es constante y seguro. |
| **RSK-07** | Lectura negativa / Underflow de offset | `sensoresDistancia.cpp:92,97,102` | Si un obstáculo está más cerca que el offset físico (ej. $rawCent = 30\text{ mm}$, $OFSET = 50\text{ mm}$), $distanciaCent = -20\text{ mm}$. Al ser `int16_t`, comparaciones ingenuas pueden fallar. | **MEDIA** | Realizar comparaciones con tipo con signo y acotar lecturas mínimas: `if (distanciaCent < 0) distanciaCent = 0;`. |
| **RSK-08** | Timeouts de ToF disfrazados | `sensoresDistancia.cpp:91,96,101` | El driver Pololu retorna 65535 en timeout; el código lo recorta a 2000 mm ($1960\text{ mm}$ con offset). El robot interpreta una falla de sensor como un pasillo infinito. | **ALTA** | Comprobar `timeoutOccurred()` o valores brutos $\ge 2000\text{ mm}$ antes de utilizarlos para decisiones de navegación. |
| **RSK-09** | Desbordamiento de contadores de encoders | `encoders.cpp:18-24`, `main.cpp:73` | Acumulación indefinida de cuentas en variables de 32 bits. | **BAJA** | Descartado como peligro: `ESP32Encoder` utiliza `int32_t` (hasta $\pm 2 \times 10^9$ pulsos) y `clearCount()` se ejecuta al inicio de cada celda y giro ($< 1000$ pulsos acumulados). |

---

## 4. Recomendaciones Arquitectónicas para la Implementación

1. **Estructura Interna de `case AVANZANDO:`:**
   Organizar el estado en tres etapas secuenciales no bloqueantes en cada vuelta de `loop()`:
   - **Etapa 1: Odometría y Lógica de Parada (R1 y R2):**
     - Evaluar sensor frontal: si `distanciaCent > 0 && distanciaCent <= 50`, gatillar frenado a `DECISION`.
     - Evaluar flancos laterales: si no se ha detectado flanco y se observa transición de pared presente a ausencia de pared, registrar `flancoDetectado = true` y resetear cuenta para medir 400 pulsos.
     - Evaluar límite de pulsos: si se alcanzan los 400 pulsos (con flanco) o los 800 pulsos (sin flanco), gatillar frenado a `DECISION`.
   - **Etapa 2: Control de Trayectoria y Ceguera PID (R3):**
     - Si `pulsosActuales < 100`, forzar `correccion = 0`.
     - Si `pulsosActuales >= 100`, aplicar `correccion = calcularCorreccion(sensadoActual)`.
   - **Etapa 3: Accionamiento de Motores:**
     - Aplicar `movimiento(AVANZAR, {velocidadActual.izquierda, velocidadActual.derecha});`.

2. **Preservación Incondicional de `FRENANDO`:**
   Cualquier evento de detención debe realizar:
   ```cpp
   movimiento(FRENO_F, {0,0});
   tiempoInicioFreno = millis();
   estadoPostFreno = DECISION;
   estado = FRENANDO;
   ```
   Garantizando que nunca se produzca una bifurcación no sincronizada hacia `DECISION`.
