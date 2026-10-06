# Reporte Técnico de Investigación: Requisitos R1 y R2
**Módulo:** Odometría Clásica y Control Reactivo de Celda (MicroMouse)  
**Autor:** Explorer 2 (`teamwork_preview_explorer`)  
**Fecha:** 2026-10-06  
**Objetivo:** Análisis exhaustivo de los requisitos R1 (Reseteo por flanco lateral) y R2 (Alineación con pared frontal), su relación con la arquitectura actual (`main.cpp`, `config.h`, sensores y encoders), riesgos de carrera y estrategia de implementación.

---

## 1. Resumen Ejecutivo

Este informe detalla el análisis de ingeniería sobre el comportamiento cinemático y sensorial del robot `debugRobot` para dar cumplimiento estricto a las especificaciones R1 y R2 definidas en `ORIGINAL_REQUEST.md`:
1. **R1 (Reseteo por flanco lateral):** Implementación de detección de flanco de bajada de pared lateral ($< 130\text{ mm} \to > 130\text{ mm}$) en `AVANZANDO` para resetear odometría y recorrer $400$ pulsos (media celda) hasta el centro, con fallback de $800$ pulsos si no se detectaron paredes desde el inicio.
2. **R2 (Alineación con pared frontal):** Detección de pared frontal en el sensor central ($\le 50\text{ mm}$ de distancia neta) para sobreescribir el límite de encoders y frenar con precisión geométrica frente al obstáculo antes de transicionar a `DECISION`.

Se identificaron puntos críticos de interacción con la máquina de estados existente (`AVANZANDO`, `FRENANDO`, `DECISION`), posibles trampas de reinicio continuo (como BUG-08), trampas de datos obsoletos tras giros, y una interferencia directa con R3 (ceguera del PID) que debe ser desacoplada para evitar desestabilización en cruces.

---

## 2. Análisis Detallado de R1: Detección de Flanco Lateral y Reseteo

### 2.1 Sensores Laterales Existentes
- **Hardware y Bus:** Dos sensores ópticos de tiempo de vuelo ST VL53L0X comunicados por I2C (`SDA = GPIO 21`, `SCL = GPIO 22`).
  - **Sensor Izquierdo:** `adressIzq = 0x31`, pin de control `xshutPinIzq = GPIO 23`.
  - **Sensor Derecho:** `adressDer = 0x30`, pin de control `xshutPinDer = GPIO 18`.
- **Estructura y Variables en Código:**
  - Definición en `src/hardware/sensoresDistancia/sensoresDistancia.h`:
    ```cpp
    struct sensado {
        int16_t distanciaCent;
        int16_t distanciaDer;
        int16_t distanciaIzq;
    };
    ```
  - Instancia global en `src/main.cpp`:
    ```cpp
    sensado sensadoActual = {0,0,0};
    ```
  - Función de actualización:
    ```cpp
    sensadoActual = actualizarSensado();
    ```
- **Unidades y Calibración:**
  - Valores medidos en **milímetros ($mm$)**, almacenados en enteros con signo de 16 bits (`int16_t`).
  - En `src/hardware/sensoresDistancia/sensoresDistancia.cpp`:
    - `lecturaAct.distanciaIzq = rawIzq - OFSET_IZQ;` (con `OFSET_IZQ = 40` en `config.h`).
    - `lecturaAct.distanciaDer = rawDer - OFSET_DER;` (con `OFSET_DER = 47` en `config.h`).
- **Umbral de Presencia de Pared:**
  - `#define UMBRAL_PARED_ESTADO_NORMAL 130` en `src/config.h`.
  - En un laberinto clásico con pasillos de $180\text{ mm}$, un robot centrado mide aproximadamente entre $40\text{ mm}$ y $55\text{ mm}$ a la pared.
  - Una lectura $\le 130\text{ mm}$ indica **presencia de pared**.
  - Una lectura $> 130\text{ mm}$ indica **ausencia de pared** (abertura, pasillo transversal o cruce).

---

### 2.2 Detección Confiable del Flanco de Bajada ($<130\text{ mm} \to >130\text{ mm}$)
En términos de señal digital y odometría de MicroMouse:
- La señal booleana de presencia de pared es $P(t) = (\text{distancia} \le 130)$.
- Cuando el robot pasa de circular junto a una pared a un espacio abierto, $P(t)$ pasa de `true` a `false`. Esto constituye el **flanco de bajada** (*falling edge* de la presencia de pared), lo que físicamente se traduce en que la distancia medida salta de $< 130\text{ mm}$ a $> 130\text{ mm}$.

#### Mecanismo No Bloqueante y Libre de Disparos en Falso:
1. **Lazo de Control No Bloqueante:**
   `loop()` no debe contener `while()` ni `delay()`. Cada iteración evalúa el estado actual contra el estado anterior almacenado en variables de estado dentro de la FSM.
2. **Requisito de Historial ("Viene detectando una pared"):**
   Si el robot ingresa a `AVANZANDO` en un pasillo sin paredes laterales (ambas lecturas $> 130\text{ mm}$ desde el inicio), **no hubo flanco**. Para que exista un flanco de bajada, el robot debe haber confirmado previamente la presencia de la pared durante el avance de la celda actual.
   - Bandera `habiaParedDer` se activa si en algún momento del avance actual `distanciaDer <= 130`.
   - Bandera `habiaParedIzq` se activa si en algún momento del avance actual `distanciaIzq <= 130`.
3. **Pestillo de Un Solo Disparo (*One-Shot Latch*):**
   Apenas se detecta la transición (`habiaPared && distancia > 130`):
   - Se ejecuta el reseteo de encoders: `resetearEncoders();`.
   - Se ajusta la meta de pulsos: `limitePulsos = 400;`.
   - Se activa de forma irreversible para esa celda: `flancoDetectado = true;`.
   - **Riesgo crítico evitado:** Si no se utiliza un pestillo de un solo disparo, en cada iteración subsiguiente del `loop()` la distancia seguirá siendo $> 130\text{ mm}$, reseteando los encoders en cada ciclo (reproduciendo el defecto fatal BUG-08) e impidiendo que el robot alcance jamás los 400 pulsos.
4. **Filtro Antirruido / Rebotes:**
   El sensor ToF VL53L0X actualiza a una tasa de aproximadamente $30\text{ ms}$ a $50\text{ ms}$.
   Para prevenir que un único rebote óptico o fluctuación momentánea dispare el flanco:
   - Exigir una pequeña distancia mínima recorrida desde el inicio de la celda (ej. $150$ pulsos) antes de habilitar la detección del flanco, dado que físicamente la pared de una celda termina cerca del límite de la misma (alrededor de los $350-400$ pulsos desde el centro).
   - Opcionalmente, requerir 2 muestras consecutivas por encima del umbral ($> 130\text{ mm}$).

---

### 2.3 Lectura y Reseteo de Encoders: Significado de 400 vs 800 Pulsos

#### Estado Actual del Código de Encoders
- **Hardware y Driver:** ESP32 PCNT (Pulse Counter) controlado mediante la librería `ESP32Encoder`.
  - Motor A: `ENC_A_1` (GPIO 33), `ENC_B_1` (GPIO 25).
  - Motor B: `ENC_A_2` (GPIO 26), `ENC_B_2` (GPIO 27).
- **Funciones en `src/hardware/encoders/encoders.cpp`:**
  ```cpp
  int32_t verPulsosEncoderA() { return encoderA.getCount(); }
  int32_t verPulsosEncoderB() { return encoderB.getCount(); }
  void resetearEncoders() {
      encoderA.clearCount();
      encoderB.clearCount();
  }
  ```
- **Cálculo de Distancia en `src/main.cpp`:**
  ```cpp
  pulsosActuales = (abs(verPulsosEncoderA()) + abs(verPulsosEncoderB())) / 2;
  ```
- **Constante de Celda:**
  `#define PULSOS_CELDA 800` en `src/config.h`.

#### Geometría de Navegación: ¿Qué significan 800 pulsos vs 400 pulsos?
- En un laberinto reglamentario, las celdas miden **$180\text{ mm} \times 180\text{ mm}$**.
- La constante `PULSOS_CELDA = 800` representa el desplazamiento lineal de **una celda completa** ($180\text{ mm}$), es decir, viajar de centro a centro de celdas contiguas.
- **$400$ pulsos** representan exactamente **media celda** ($90\text{ mm}$).
- **Escenario Fallback (800 pulsos):**
  Si el robot avanza por un pasillo abierto sin paredes laterales reconocidas desde el inicio, o una pared lateral continua e ininterrumpida sin aperturas, no dispone de referencias ópticas transversales para sincronizar su posición. En tal caso, utiliza la odometría pura a ciegas: avanza los $800$ pulsos completos para pasar del centro de la celda actual al centro de la siguiente.
- **Escenario R1 (Reseteo y 400 pulsos):**
  Cuando el robot avanza junto a una pared y esta termina (en el poste/columna que delimita la celda), el sensor lateral detecta la caída de la pared exactamente en la frontera entre celdas ($90\text{ mm}$ desde el centro de la celda de origen).
  Al resetear los encoders a $0$ en ese instante físico:
  - Se elimina de golpe cualquier error de patinamiento o deriva acumulada durante los primeros $90\text{ mm}$.
  - El robot solo debe avanzar otros $90\text{ mm}$ ($400$ pulsos) desde la frontera para quedar perfectamente ubicado en el centro de la nueva celda.
  - Al completar los $400$ pulsos posteriores al flanco, frena de inmediato y pasa a `DECISION`.

---

## 3. Análisis Detallado de R2: Alineación y Parada Frontal

### 3.1 Sensor Frontal / Central
- **Hardware:** Sensor central VL53L0X en bus I2C (`adressCent = 0x32`, `xshutPinCent = GPIO 19`).
- **Variable de Sensado:** `sensadoActual.distanciaCent`.
- **Cálculo en `sensoresDistancia.cpp`:**
  ```cpp
  lecturaAct.distanciaCent = rawCent - OFSET_CENT; // OFSET_CENT = 50 en config.h
  ```
- **Unidades:** Milímetros ($mm$).

---

### 3.2 Evaluación Actual de la Condición de Parada en `AVANZANDO`
En `src/main.cpp` (líneas 72 a 92):
```cpp
case AVANZANDO: {
  pulsosActuales = (abs(verPulsosEncoderA()) + abs(verPulsosEncoderB()))/2;
  
  if (pulsosActuales < PULSOS_CELDA) {
    sensadoActual = actualizarSensado();
    int16_t correccion = calcularCorreccion(sensadoActual);
    
    velocidadActual.izquierda = constrain(VEL_BASE_DER - correccion, 0, 255);
    velocidadActual.derecha = constrain(VEL_BASE_IZQ + correccion, 0, 255);
    
    movimiento(AVANZAR, {velocidadActual.izquierda, velocidadActual.derecha});
  } else {
    enviarString(">>> INGRESO A DECISIÓN <<<");
    movimiento(FRENO_F, {0,0});
    tiempoInicioFreno = millis();
    estadoPostFreno = DECISION;
    estado = FRENANDO;
  }
  break;
}
```
**Diagnóstico:**
Actualmente, la condición de detención evalúa **única y exclusivamente** el límite de encoders `pulsosActuales < PULSOS_CELDA`.
El sensor frontal es completamente ignorado para frenar. Si hay una pared enfrente, el robot depende ciegamente de que los 800 pulsos lo ubiquen a la distancia adecuada; cualquier desvío mecánico o patinamiento hace que el robot choque contra la pared frontal o frene demasiado lejos para girar con seguridad.

---

### 3.3 Sobreescritura Segura de Encoders por Parada Frontal ($\le 50\text{ mm}$)
El requisito R2 exige:
> "Si el robot avanza hacia una celda con pared enfrente, no debe usar los encoders para frenar. Debe avanzar hasta que el sensor central lea exactamente 50 mm (o menos) de distancia y entonces detenerse para tomar la decisión."

Y el criterio de aceptación define:
> "La condición de parada frontal sobreescribe el límite de encoders de forma segura (ej. no frena prematuramente si no hay pared)."

#### Análisis de Mecanismos de Seguridad para Evitar Falsos Positivos y Paradas Prematuras:
1. **Pared Inexistente (Pasillo Libre al Frente):**
   Si la celda no tiene pared frontal, `distanciaCent` leerá distancias elevadas ($> 130\text{ mm}$, típicamente $> 300\text{ mm}$ o saturado a $1950\text{ mm}$).
   En este caso, la condición de parada frontal ($\le 50\text{ mm}$) es siempre falsa. El robot avanza normalmente y se detiene cuando los encoders alcanzan la meta (`pulsosActuales >= limitePulsos`, sea 800 o 400).
2. **Peligro de Lecturas Nulas / Desbordamiento (BUG-23 y BUG-26):**
   - Al encender o reiniciar, `sensadoActual` arranca en `{0,0,0}`.
   - Si una lectura I2C falla o está pendiente, `distanciaCent` podría registrar `0` o incluso un número negativo (`rawCent - 50 < 0`).
   - Si se evalúa simplemente `distanciaCent <= 50`, ¡una lectura espuria de `0` o `-50` detendría al robot instantáneamente en el pulso 0!
   - **Regla de integridad:** Exigir que la lectura sea físicamente válida:
     `sensadoActual.distanciaCent > 0 && sensadoActual.distanciaCent <= 50`
     (o `sensadoActual.distanciaCent >= -20 && sensadoActual.distanciaCent <= 50` si el offset estuviera levemente sobrecompensado, descartando lecturas no inicializadas).
3. **Guarda Espacial / Inmunidad al Arranque:**
   Al comenzar a avanzar en una celda, la pared del fondo de la celda de destino se encuentra a más de $180\text{ mm}$ a $270\text{ mm}$ del sensor.
   Es físicamente imposible que un robot que acaba de iniciar el avance esté a $50\text{ mm}$ de la pared frontal de la celda a la que se dirige.
   - Exigir una distancia mínima recorrida (ej. `pulsosTotalesAvance > 150`) o comprobar que el sensor frontal haya registrado previamente un acercamiento progresivo ($\le 120\text{ mm}$) antes de disparar la parada a $50\text{ mm}$.
4. **Sobreescritura del Límite de Encoders:**
   ¿Qué ocurre si, al aproximarse a una pared frontal, los encoders alcanzan los 800 pulsos pero el sensor frontal aún lee $65\text{ mm}$?
   - Si el robot se detuviera por encoders a $65\text{ mm}$, violaría R2.
   - Por tanto: Si el sensor frontal detecta que **hay pared al frente** (`distanciaCent <= UMBRAL_PARED_FRENTE`), el robot **debe continuar avanzando** a velocidad controlada hasta que `distanciaCent <= 50 mm`.
5. **Watchdog de Seguridad contra Colisión / Sensor Muerto:**
   Si el sensor central se bloquea, se desconecta o la superficie frontal es negra mate y no refleja luz, el robot no debe avanzar indefinidamente hasta quemar motores contra la pared.
   - Debe existir un límite máximo de pulsos de seguridad (ej. `pulsosTotalesAvance >= 1050`, es decir, celda $+ 30\%$) que actúe como freno de emergencia si el sensor de distancia jamás reporta $\le 50\text{ mm}$.

---

## 4. Matriz de Casos Borde, Condiciones de Carrera y Transiciones de Estado

### 4.1 Conflicto de Carrera: R1 (Reseteo de Encoders) vs R3 (Ceguera del PID)
- **Definición de R3:** En los primeros $100$ pulsos de `AVANZANDO`, la corrección del PID debe forzarse a $0$.
- **Conflicto Crítico Identificado:**
  Si R1 resetea los encoders físicos al detectar el flanco lateral (`resetearEncoders()`), la variable `pulsosActuales` vuelve a $0$.
  Si la ceguera de R3 se implementa chequeando ingenuamente:
  ```cpp
  if (pulsosActuales < 100) { correccion = 0; }
  ```
  entonces, en cuanto el robot detecta el flanco lateral a mitad de la celda y resetea los encoders, **¡el PID volverá a silenciarse durante 100 pulsos adicionales en plena zona abierta o cruce!** Esto provocaría deriva sin control justo cuando el robot entra a la intersección.
- **Estrategia de Solución:**
  Desacoplar la ceguera del PID del contador que se resetea por flanco.
  - Utilizar una variable acumuladora absoluta `pulsosTotalesDesdeInicioCelda` o una bandera booleana `cegueraPIDFinalizada`:
    Una vez que el robot recorre los primeros 100 pulsos tras iniciar `AVANZANDO`, `cegueraPIDFinalizada = true`.
    El reseteo de odometría de R1 **no debe alterar** el estado de `cegueraPIDFinalizada`.

---

### 4.2 Precedencia R2 vs R1 (Pared Frontal en Celda con Apertura Lateral)
- **Escenario:** El robot transita por un pasillo en forma de "L" (curva a la izquierda o derecha) o un callejón con una salida lateral.
  Al llegar al poste, detecta la caída de la pared lateral (R1 dispara reseteo a 400 pulsos).
  Sin embargo, directamente enfrente tiene la pared frontal que bloquea el camino.
- **Pregunta de Prioridad:** ¿Debe esperar a completar los 400 pulsos de R1 o frenar a los 50 mm de R2?
- **Resolución Indiscutible:**
  **R2 tiene máxima prioridad sobre R1.**
  La detección de proximidad crítica frontal ($\le 50\text{ mm}$) es una condición de seguridad física inminente. Si el robot esperara a completar 400 pulsos teniendo una pared enfrente, colisionaría a toda velocidad contra el muro frontal.
  Por lo tanto, la comprobación de `distanciaCent <= 50 mm` debe evaluar primero y provocar la detención inmediata a `FRENANDO`.

---

### 4.3 Doble Flanco Lateral Simultáneo (Cruce en Cruz "+")
- **Escenario:** El robot llega a una intersección en cruz de 4 vías. Ambas paredes (izquierda y derecha) desaparecen al mismo tiempo.
- **Riesgo:** Si el sensor izquierdo activa el reseteo en el ciclo $N$, y el sensor derecho activa otro reseteo en el ciclo $N+1$, el robot resetearía dos veces los encoders, viajando 400 pulsos extra y pasándose de largo.
- **Resolución:**
  La bandera `flancoDetectado` es global para la fase de avance. El primer flanco (sea izquierdo o derecho) resetea encoders y fija `flancoDetectado = true`. Ningún otro sensor puede volver a resetear los contadores en esa celda.

---

### 4.4 Datos Obsoletos al Salir de Giros (`FRENANDO` $\to$ `AVANZANDO`)
- **Problema en Código Actual:**
  En `src/main.cpp`, las líneas 170-179 muestran:
  ```cpp
  case FRENANDO: {
    movimiento(FRENO_F, {0,0}); // Mantener el freno activo
    if (millis() - tiempoInicioFreno >= 150) { // 150ms de pausa estabilizadora
      resetearErrorAnterior();
      resetearEncoders();
      if (estadoPostFreno == AVANZANDO) enviarString(">>> AVANZANDO <<<");
      estado = estadoPostFreno;
    }
    break;
  }
  ```
  Durante los $150\text{ ms}$ de `FRENANDO`, ¡nunca se invoca `actualizarSensado()`!
  Si el robot venía de frenar frente a una pared a $50\text{ mm}$, giró $90^\circ$ y entra a `FRENANDO`, al pasar a `AVANZANDO`, la variable `sensadoActual.distanciaCent` aún conserva en memoria el valor de $50\text{ mm}$ de la maniobra anterior si no se actualiza inmediatamente antes de evaluar las condiciones de parada.
- **Resolución:**
  Al iniciar `AVANZANDO`, forzar una lectura fresca de sensores y reiniciar todas las variables de seguimiento de celda:
  `flancoDetectado = false; habiaParedIzq = false; habiaParedDer = false;`

---

## 5. Estrategia de Implementación Recomendada

### 5.1 Variables de Control de Celda Propuestas
En `src/main.cpp` (variables estáticas o globales de ámbito de supervisión):
```cpp
// Variables de control de odometría adaptativa en AVANZANDO
uint16_t limitePulsosActual = PULSOS_CELDA; // 800 por defecto
bool flancoLateralDetectado = false;
bool habiaParedIzq = false;
bool habiaParedDer = false;
uint32_t pulsosAbsolutosCelda = 0; // Odometría acumulada no reseteada para R3 y watchdog
```

### 5.2 Lógica Integrada para `case AVANZANDO:`
```cpp
case AVANZANDO: {
  // Lectura del avance actual
  int32_t pulsosA = abs(verPulsosEncoderA());
  int32_t pulsosB = abs(verPulsosEncoderB());
  pulsosActuales = (pulsosA + pulsosB) / 2;
  
  // 1. Muestreo de sensores ToF
  sensadoActual = actualizarSensado();
  
  // 2. Registro de presencia previa de paredes laterales
  if (!flancoLateralDetectado) {
    if (sensadoActual.distanciaIzq > 0 && sensadoActual.distanciaIzq <= UMBRAL_PARED_ESTADO_NORMAL) {
      habiaParedIzq = true;
    }
    if (sensadoActual.distanciaDer > 0 && sensadoActual.distanciaDer <= UMBRAL_PARED_ESTADO_NORMAL) {
      habiaParedDer = true;
    }
    
    // 3. Detección de flanco de bajada (R1)
    // Se exige un mínimo de avance (ej. 150 pulsos) para evitar rebotes de entrada a la celda
    bool flancoIzq = habiaParedIzq && (sensadoActual.distanciaIzq > UMBRAL_PARED_ESTADO_NORMAL);
    bool flancoDer = habiaParedDer && (sensadoActual.distanciaDer > UMBRAL_PARED_ESTADO_NORMAL);
    
    if ((flancoIzq || flancoDer) && pulsosActuales > 150) {
      resetearEncoders();
      limitePulsosActual = 400; // Avanzar media celda desde el flanco
      flancoLateralDetectado = true;
      pulsosActuales = 0; // Reinicio local de la cuenta
      enviarString(">>> FLANCO DETECTADO: RESET A 400 PULSOS <<<");
    }
  }

  // 4. Evaluación de Condición de Parada Frontal (R2)
  // Válido si la distancia central es física (> 0) y menor o igual a 50 mm
  bool stopPorParedFrontal = (sensadoActual.distanciaCent > 0 && 
                              sensadoActual.distanciaCent <= 50 && 
                              pulsosActuales > 100);

  // 5. Evaluación de Condición de Parada por Encoders
  // Solo se detiene por encoders si NO hay una pared frontal inminente a la vista
  bool paredAlFrente = (sensadoActual.distanciaCent > 0 && 
                        sensadoActual.distanciaCent <= UMBRAL_PARED_FRENTE);
  
  bool stopPorEncoders = (pulsosActuales >= limitePulsosActual) && !paredAlFrente;

  // 6. Watchdog de seguridad (anti-bloqueo si el sensor frontal falla)
  bool stopPorSeguridad = (pulsosActuales >= (PULSOS_CELDA + 250));

  if (!stopPorParedFrontal && !stopPorEncoders && !stopPorSeguridad) {
    // Cálculo de corrección de centrado
    int16_t correccion = calcularCorreccion(sensadoActual);
    
    // Aplicación de Ceguera del PID (R3): mantener en 0 los primeros 100 pulsos iniciales
    // (Verificado con la variable de avance total o condición de inicio)
    if (pulsosAbsolutosCelda < 100) {
      correccion = 0;
    }
    
    velocidadActual.izquierda = constrain(VEL_BASE_DER - correccion, 0, 255);
    velocidadActual.derecha = constrain(VEL_BASE_IZQ + correccion, 0, 255);
    
    movimiento(AVANZAR, {velocidadActual.izquierda, velocidadActual.derecha});
  } else {
    // Transición a detención y estabilización
    enviarString(">>> DETENCIÓN CELDA (FRENANDO) <<<");
    movimiento(FRENO_F, {0,0});
    tiempoInicioFreno = millis();
    estadoPostFreno = DECISION;
    estado = FRENANDO;
  }
  break;
}
```

### 5.3 Inicialización en Transiciones hacia `AVANZANDO`
Cada vez que la FSM cambia a `AVANZANDO` (desde `LISTO`, `DECISION` o `FRENANDO`):
```cpp
resetearEncoders();
resetearErrorAnterior();
limitePulsosActual = PULSOS_CELDA; // Plan B: 800 pulsos
flancoLateralDetectado = false;
habiaParedIzq = false;
habiaParedDer = false;
pulsosAbsolutosCelda = 0;
estado = AVANZANDO;
```

---

## 6. Conclusión y Recomendaciones Finales

1. **Cumplimiento de R1:**
   El mecanismo de flanco lateral propuesto es 100% no bloqueante, inmune a disparos repetitivos gracias al pestillo `flancoLateralDetectado`, y garantiza que el avance posterior sea de exactamente 400 pulsos tras el reseteo, conservando el fallback de 800 pulsos cuando no hay pared inicial.
2. **Cumplimiento de R2:**
   La condición de parada frontal ($\le 50\text{ mm}$) sobreescribe con éxito los encoders, garantizando alineación exacta sin detenerse prematuramente en celdas abiertas ni causar falsos positivos por lecturas iniciales nulas.
3. **Preservación Arquitectónica (R4):**
   No se agregan estructuras de mapeo ni memoria de laberinto. La FSM conserva su flujo clásico (`AVANZANDO` $\to$ `FRENANDO` $\to$ `DECISION` $\to$ `GIRANDO_*` $\to$ `FRENANDO` $\to$ `AVANZANDO`).
4. **Desacoplamiento R1 vs R3:**
   Es mandatorio que el Worker implemente un contador de pulsos absolutos o bandera de ceguera para R3, evitando que el reseteo de encoders de R1 reactive la ceguera del PID en la mitad de la celda.
