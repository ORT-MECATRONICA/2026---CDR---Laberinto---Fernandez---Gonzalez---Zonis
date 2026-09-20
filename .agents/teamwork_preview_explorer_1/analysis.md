# Informe de Investigación: Navegación, Máquina de Estados y Control PID (Micromouse)

**Especialista:** teamwork_preview_explorer (Navigation Specialist)  
**Fecha:** 2026-09-20T00:47:00Z  
**Directorio del Proyecto:** `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\bahiaBlanca`

---

## 1. Resumen Ejecutivo

El presente informe detalla el análisis arquitectónico y funcional de los módulos de navegación (`rightHand.cpp`, `rightHand.h`), control de motores (`puenteH.cpp`), sensado de encoders (`encoders.cpp`) y control PID (`PID.cpp`, `PID.h`) en el proyecto micromouse `bahiaBlanca`.

Se identificaron deficiencias críticas en la implementación actual:
1. **Máquina de estados incompleta**: Solo el subestado `AVANZANDO` está implementado. Al transicionar a cualquier otro subestado (`PREPARANDOME_PARA_GIRAR_DER`, etc.), la ejecución cae fuera del switch y el robot queda sin control de motores.
2. **Debounce defectuoso**: El contador usa una cascada `if-else if` mutuamente excluyente y **nunca resetea los contadores** a cero cuando la condición deja de cumplirse.
3. **Inversión de lógica en giro**: Al detectar apertura derecha (`counterDer > 5`), transiciona erróneamente a `PREPARANDOME_PARA_GIRAR_IZQ`.
4. **Ausencia de detección de callejón sin salida (Dead-End)**: No existe evaluación simultánea de pared izquierda, derecha y frontal ni estado de giro de 180°.
5. **Inversión de signo en corrección PID**: El cálculo actual de error en `PID.cpp` (`distanciaIzq - distanciaDer`) tiene el signo invertido respecto a la aplicación en `rightHand.cpp`, provocando que el robot acelere la rueda hacia la pared más cercana en lugar de alejarse.
6. **Vulnerabilidad ante huecos (ausencia de guard clause)**: El PID actual intenta centrarse cuando un sensor lee una apertura lateral grande antes de superar el umbral, tirando al robot hacia el vértice de la pared.
7. **Presencia de números mágicos**: Varios umbrales (`+ 50`, `5`, `-50, 50`) están hardcodeados sin definir en `config.h`.

---

## 2. Mapa de Archivos Relevantes y Contexto de Ejecución

- `src/main.cpp`: Inicializa subsistemas y en `loop()` ejecuta `switch(estadoActual)`. Cuando `estadoActual == RIGHT_HAND`, ejecuta `estadoActual = right_hand();`.
- `src/main.h`: Define `enum ESTADOS { HUB, RIGHT_HAND, LEFT_HAND, MAPEO, CALIBRAR };`.
- `src/config.h`: Define pines, velocidades (`VEL_BASE_*`, `VEL_GIRO_*`), ganancias PID (`KP`, `KD`), umbrales de pared (`UMBRAL_PARED_ESTADO_NORMAL`, `UMBRAL_PARED_FRENTE`) y parámetros de pulsos/tiempos (`PULSOS_90_GRADOS`, `PULSOS_AVANCE_PREGIRO`, etc.).
- `src/hardware/movimiento/puenteH.h` / `puenteH.cpp`: Control de motores mediante PWM en ESP32 (`movimiento(MOVIMIENTOS, VELOCIDAD)`). Posee funciones bloqueantes (`girar90GradosBloqueante`, `avanzarBloqueante`) que **no deben ser usadas** por la máquina de estados no bloqueante.
- `src/hardware/encoders/encoders.h` / `encoders.cpp`: Lectura no bloqueante de encoders por cuadratura (`ESP32Encoder`): `verPulsosEncoderA()`, `verPulsosEncoderB()`, `resetearEncoders()`.
- `src/hardware/sensoresDistancia/sensoresDistancia.h` / `sensoresDistancia.cpp`: Sensado no bloqueante con 3 sensores VL53L0X. Devuelve struct `sensado { uint16_t distanciaCent; uint16_t distanciaDer; uint16_t distanciaIzq; }`.
- `src/hardware/movimiento/PID.h` / `PID.cpp`: Cálculo de corrección de dirección proporcional-derivativa.
- `src/maquinaEstados/rightHand.h` / `rightHand.cpp`: Implementación de la regla de la mano derecha.

---

## 3. Análisis Detallado Punto por Punto

### 3.1. Estructura Actual de `rightHand`

#### Estados declarados vs implementados
En `rightHand.h` se declaran:
```cpp
enum SUBESTADOS {
    AVANZANDO,
    PREPARANDOME_PARA_GIRAR_DER,
    GIRANDO_DER,
    PREPARANDOME_PARA_GIRAR_IZQ,
    GIRANDO_IZQ,
    FIN,
};
```
Sin embargo, en `rightHand.cpp` el `switch(estadoActual)` contiene **únicamente** `case AVANZANDO:`.
Los estados `PREPARANDOME_PARA_GIRAR_DER`, `GIRANDO_DER`, `PREPARANDOME_PARA_GIRAR_IZQ`, `GIRANDO_IZQ` y `FIN` **no tienen implementación**.
Tampoco existe estado alguno para el giro de 180° (callejón sin salida), como `PREPARANDOME_PARA_GIRAR_180` o `GIRANDO_180`.

#### Naturaleza bloqueante vs no bloqueante
- En `main.cpp`, `right_hand()` es llamado en cada ciclo del loop: `estadoActual = right_hand();`.
- Para que la arquitectura sea verdaderamente no bloqueante, `right_hand()` debe retornar inmediatamente en cada ciclo, permitiendo que el microcontrolador actualice sensores, verifique botones o comunicaciones de telemetría.
- Aunque actualmente `AVANZANDO` no tiene `delay()`, `puenteH.cpp` cuenta con funciones bloqueantes basadas en `delay(TIEMPO_90_GRADOS)` y `delay(TIEMPO_AVANZAR_BLOQUEANTE)`. Invocar dichas funciones en los estados de giro bloquearía el ESP32. Por ende, todos los giros y avances de preparación deben ser gobernados por contadores de pulsos de encoders (`verPulsosEncoderA()`) o diferencias de tiempo no bloqueantes (`millis() - tiempoInicio`).

---

### 3.2. R2: Máquina de Estados No Bloqueante con Debounce

#### Problemas del Debounce actual
En `rightHand.cpp` se observa:
```cpp
if(sensadoActual.distanciaDer > UMBRAL_PARED_ESTADO_NORMAL){
    filtroSensores.counterDer++;
} else if (sensadoActual.distanciaCent > UMBRAL_PARED_ESTADO_NORMAL){
    filtroSensores.counterCent++;
} else if (sensadoActual.distanciaIzq > UMBRAL_PARED_ESTADO_NORMAL){
    filtroSensores.counterIzq++;
}
```
1. **Estructura if-else if excluyente**: Si `distanciaDer` supera el umbral, `counterCent` y `counterIzq` jamás se evalúan en ese ciclo. Esto impide detectar intersecciones complejas (cruces en T, aperturas simultáneas) o callejones sin salida.
2. **Ausencia de reset**: Si en un ciclo no se cumple la condición, el contador correspondiente **no se reinicia a 0**. Esto acumula lecturas espurias a lo largo de decenas de ciclos, disparando transiciones por ruido y violando el principio de lecturas consecutivas.
3. **Inversión de giro**:
   ```cpp
   if(filtroSensores.counterDer > 5){
       estadoActual = PREPARANDOME_PARA_GIRAR_IZQ; // <-- ERROR: Debería ser DERECHA
   }
   ```
4. **Número mágico**: El valor `5` está hardcodeado directamente en el código fuente.

#### Arquitectura Propuesta de Debounce Robusto
1. **Definición de constantes en `config.h`**:
   ```cpp
   #define DEBOUNCE_LECTURAS 5
   ```
2. **Estructura de filtrado desacoplada**:
   ```cpp
   struct FILTRO_DEBOUNCE {
       uint8_t apertDer;
       uint8_t apertIzq;
       uint8_t paredFrente;
       uint8_t callejon;
   };
   ```
3. **Lógica de evaluación independiente con reinicio**:
   En cada ciclo de `AVANZANDO`:
   ```cpp
   bool apertDer   = (sensadoActual.distanciaDer > UMBRAL_PARED_ESTADO_NORMAL);
   bool paredFrente = (sensadoActual.distanciaCent <= UMBRAL_PARED_FRENTE);
   bool apertIzq   = (sensadoActual.distanciaIzq > UMBRAL_PARED_ESTADO_NORMAL);
   bool callejon   = (!apertDer && paredFrente && !apertIzq);

   // Apertura derecha
   if (apertDer) { filtro.apertDer++; } 
   else          { filtro.apertDer = 0; }

   // Callejón sin salida
   if (callejon) { filtro.callejon++; } 
   else          { filtro.callejon = 0; }

   // Pared frontal
   if (paredFrente) { filtro.paredFrente++; } 
   else             { filtro.paredFrente = 0; }

   // Apertura izquierda
   if (apertIzq) { filtro.apertIzq++; } 
   else          { filtro.apertIzq = 0; }
   ```
4. **Transición jerárquica (Regla de la Mano Derecha)**:
   - **Prioridad 1 (Girar a la derecha si hay apertura a la derecha)**:
     Si `filtro.apertDer >= DEBOUNCE_LECTURAS`: reiniciar contadores, reiniciar encoders y pasar a `PREPARANDOME_PARA_GIRAR_DER`.
   - **Prioridad 2 (Seguir recto si no hay pared al frente)**:
     Si no hay apertura derecha y `filtro.paredFrente == 0`: continuar en `AVANZANDO` aplicando PID.
   - **Prioridad 3 (Girar a la izquierda si frente cerrado y apertura a la izquierda)**:
     Si `filtro.paredFrente >= DEBOUNCE_LECTURAS` y `apertIzq`: reiniciar contadores, reiniciar encoders y pasar a `PREPARANDOME_PARA_GIRAR_IZQ`.
   - **Prioridad 4 (Callejón sin salida si 3 paredes presentes)**:
     Si `filtro.callejon >= DEBOUNCE_LECTURAS`: reiniciar contadores, reiniciar encoders y pasar a `GIRANDO_180`.

---

### 3.3. R3: Detección de Callejón Sin Salida (180 Grados)

#### Condición física y lógica
Un callejón sin salida se define por la presencia simultánea de obstáculos en las 3 direcciones de avance:
1. Pared derecha: `sensadoActual.distanciaDer <= UMBRAL_PARED_ESTADO_NORMAL`
2. Pared frontal: `sensadoActual.distanciaCent <= UMBRAL_PARED_FRENTE`
3. Pared izquierda: `sensadoActual.distanciaIzq <= UMBRAL_PARED_ESTADO_NORMAL`

#### Integración en la máquina de estados
1. **Nuevo subestado en `SUBESTADOS`**:
   `GIRANDO_180` (o `PREPARANDOME_PARA_GIRAR_180` si se requiere frenado o ajuste previo).
2. **Evaluación simultánea**:
   ```cpp
   bool esCallejon = (sensadoActual.distanciaDer <= UMBRAL_PARED_ESTADO_NORMAL) &&
                     (sensadoActual.distanciaCent <= UMBRAL_PARED_FRENTE) &&
                     (sensadoActual.distanciaIzq <= UMBRAL_PARED_ESTADO_NORMAL);
   ```
3. **Filtrado por debounce**:
   Se incrementa `filtro.callejon` solo si las 3 paredes persisten simultáneamente. Si cualquiera de ellas desaparece, `filtro.callejon = 0;`.
4. **Ejecución del giro no bloqueante**:
   Al alcanzar `DEBOUNCE_LECTURAS`, se frena momentáneamente o se inicia el giro de 180°:
   ```cpp
   case GIRANDO_180: {
       movimiento(GIRAR_DER, {VEL_GIRO_IZQ, VEL_GIRO_DER});
       if (abs(verPulsosEncoderA()) >= PULSOS_180_GRADOS) {
           movimiento(FRENO_F, {0, 0});
           resetearEncoders();
           resetearErrorAnterior();
           estadoActual = AVANZANDO; // o POST_GIRO
       }
       break;
   }
   ```
   Donde `PULSOS_180_GRADOS` se define en `config.h` (típicamente `2 * PULSOS_90_GRADOS` = 220).

---

### 3.4. R4: Centrado en Intersecciones (Estados Previos al Giro)

#### Dinámica geométrica del Micromouse
- Las celdas estándar de laberinto miden 180 mm x 180 mm.
- Los sensores laterales (VL53L0X) detectan la terminación de una pared lateral en el instante en que el sensor rebasa la esquina.
- El eje motriz (ruedas) del robot está situado típicamente detrás de los sensores.
- Si el robot ejecuta el giro en el instante exacto en que detecta la apertura:
  1. La cola del robot o la rueda interior golpeará la arista de la pared de la esquina.
  2. El robot quedará desalineado respecto al centro de la nueva celda.
- Por esta razón, existen en `config.h`:
  `#define PULSOS_AVANCE_PREGIRO 250`
  `#define PULSOS_AVANCE_PREGIRO_IZQ 100`
  `#define TIEMPO_AVANCE_PREGIRO 400`

#### Ciclo de Estados de Giro No Bloqueante
Para giro a la derecha:
1. `AVANZANDO`: Detecta apertura derecha confirmada por debounce.
   - Acción: `resetearEncoders()`, `estadoActual = PREPARANDOME_PARA_GIRAR_DER;`.
2. `PREPARANDOME_PARA_GIRAR_DER`:
   - Acción no bloqueante:
     ```cpp
     movimiento(AVANZAR, {VEL_BASE_IZQ, VEL_BASE_DER});
     if (abs(verPulsosEncoderA()) >= PULSOS_AVANCE_PREGIRO) {
         movimiento(FRENO_F, {0, 0});
         resetearEncoders();
         estadoActual = GIRANDO_DER;
     }
     ```
3. `GIRANDO_DER`:
   - Acción no bloqueante:
     ```cpp
     movimiento(GIRAR_DER, {VEL_GIRO_IZQ, VEL_GIRO_DER});
     if (abs(verPulsosEncoderA()) >= PULSOS_90_GRADOS) {
         movimiento(FRENO_F, {0, 0});
         resetearEncoders();
         resetearErrorAnterior();
         estadoActual = POST_GIRO_AVANZAR;
     }
     ```
4. `POST_GIRO_AVANZAR`:
   - Permite al robot avanzar una pequeña distancia (`PULSOS_AVANZAR_POST_GIRO`, e.g. 200 pulsos) para penetrar en el nuevo pasillo antes de que el sensor lateral vuelva a interpretar la intersección previa como un nuevo giro falso.
   - Al finalizar, pasa a `AVANZANDO`.

---

### 3.5. R5: Protección del PID ante Ausencias de Pared (Guard Clause)

#### Ubicación del Cálculo del PID
El PID se implementa en `hardware/movimiento/PID.cpp` mediante la función:
```cpp
int16_t calcularCorreccion(sensado mediciones);
```
En `rightHand.cpp`, se invoca en el estado `AVANZANDO`:
```cpp
long error = calcularCorreccion(sensadoActual);
movimiento(AVANZAR, {
    .izquierda = (int16_t)(VEL_BASE_IZQ + error),
    .derecha = (int16_t)(VEL_BASE_DER - error)
});
```

#### Análisis del Error y Signos en `PID.cpp` vs `rightHand.cpp`
**Hallazgo Crítico:** En `rightHand.cpp`:
- A la rueda izquierda se le suma `error` (`VEL_BASE_IZQ + error`).
- A la rueda derecha se le resta `error` (`VEL_BASE_DER - error`).
- Si `error > 0`, la rueda izquierda gira más rápido que la derecha $\implies$ el robot vira a la **DERECHA**.
- Si `error < 0`, la rueda derecha gira más rápido que la izquierda $\implies$ el robot vira a la **IZQUIERDA**.

Ahora observemos `PID.cpp` actual:
```cpp
if (hayIzq && hayDer) {
    error = (int16_t)mediciones.distanciaIzq - (int16_t)mediciones.distanciaDer;
}
```
Si el robot está demasiado cerca de la pared DERECHA:
`distanciaDer` es pequeña (e.g. 20 mm) y `distanciaIzq` es grande (e.g. 70 mm).
`error = 70 - 20 = +50`.
Al ser `error > 0`, en `rightHand.cpp` la rueda izquierda acelera y la derecha desacelera, ¡haciendo que el robot vire a la **DERECHA**, chocando directamente contra la pared derecha!
**Conclusión de Signos:** Para virar alejándose de la pared:
- Si está cerca de la derecha, debe virar a la izquierda $\implies error < 0$.
- Por lo tanto, la fórmula correcta para centrarse entre ambas paredes debe ser:
  $$\text{error} = \text{distanciaDer} - \text{distanciaIzq}$$
  O bien invertir la suma/resta en las velocidades de los motores.

#### Vulnerabilidad ante Huecos y Cláusula de Guarda Matemática
En el código actual de `PID.cpp`:
```cpp
bool hayIzq = mediciones.distanciaIzq < (UMBRAL_PARED_ESTADO_NORMAL + 50);
bool hayDer = mediciones.distanciaDer < (UMBRAL_PARED_ESTADO_NORMAL + 50);
```
1. `+ 50` es un número mágico.
2. Si un lado se abre (por ejemplo en una intersección), durante el tramo de entrada la lectura sube de 50 mm a 140 mm. Como $140 < 150$, `hayDer` sigue siendo `true`. El error se vuelve masivo y el robot pega un volantazo hacia el hueco, incrustándose en el vértice del muro.
3. **Cláusula de Guarda Matemática**:
   Se debe definir en `config.h`:
   ```cpp
   #define UMBRAL_PARED_VALIDA_PID 110       // Distancia máxima para considerar pared en PID
   #define DISTANCIA_OBJETIVO_PARED_IZQ 40   // Distancia de referencia pared izq
   #define DISTANCIA_OBJETIVO_PARED_DER 47   // Distancia de referencia pared der
   #define MAX_CORRECCION_PID 50             // Saturación máxima de corrección
   ```
   Estructura de la guarda en `calcularCorreccion`:
   ```cpp
   int16_t calcularCorreccion(sensado mediciones) {
       bool paredIzqValida = (mediciones.distanciaIzq > 0) && (mediciones.distanciaIzq <= UMBRAL_PARED_VALIDA_PID);
       bool paredDerValida = (mediciones.distanciaDer > 0) && (mediciones.distanciaDer <= UMBRAL_PARED_VALIDA_PID);

       int16_t error = 0;

       if (paredIzqValida && paredDerValida) {
           // Ambas paredes presentes: error diferencial para centrado
           error = (int16_t)mediciones.distanciaDer - (int16_t)mediciones.distanciaIzq;
       } else if (paredIzqValida && !paredDerValida) {
           // Hueco a la derecha: ignorar completamente sensor derecho. Referenciar solo a pared izquierda.
           // Si distanciaIzq < DISTANCIA_OBJETIVO (muy cerca de izq), debe virar a der (error > 0).
           error = (int16_t)DISTANCIA_OBJETIVO_PARED_IZQ - (int16_t)mediciones.distanciaIzq;
       } else if (!paredIzqValida && paredDerValida) {
           // Hueco a la izquierda: ignorar completamente sensor izquierdo. Referenciar solo a pared derecha.
           // Si distanciaDer < DISTANCIA_OBJETIVO (muy cerca de der), debe virar a izq (error < 0).
           error = (int16_t)mediciones.distanciaDer - (int16_t)DISTANCIA_OBJETIVO_PARED_DER;
       } else {
           // Ninguna pared válida (área abierta / encrucijada): no corregir, avanzar recto
           error = 0;
           errorAnterior = 0; // Evita derivativo espurio
       }

       int16_t correccion = (int16_t)((KP * error) + (KD * (error - errorAnterior)));
       errorAnterior = error;

       return constrain(correccion, -MAX_CORRECCION_PID, MAX_CORRECCION_PID);
   }
   ```
   Esta guarda garantiza que:
   - Cualquier lectura superior a `UMBRAL_PARED_VALIDA_PID` se descarta de inmediato para ese lateral.
   - El robot no intentará compensar la apertura y mantendrá una trayectoria paralela a la pared opuesta.

---

## 4. Tabla Comparativa: Estado Actual vs Propuesta

| Aspecto | Estado Actual (`rightHand.cpp` / `PID.cpp`) | Propuesta Diseñada |
| :--- | :--- | :--- |
| **Control de Flujo** | Bloqueante en concepto, incompleto en switch | Máquina de estados puramente no bloqueante que retorna `ESTADOS` en cada ciclo |
| **Estados Soportados** | Solo `AVANZANDO` implementado | `AVANZANDO`, `PREPARANDOME_PARA_GIRAR_DER`, `GIRANDO_DER`, `PREPARANDOME_PARA_GIRAR_IZQ`, `GIRANDO_IZQ`, `GIRANDO_180`, `POST_GIRO_AVANZAR`, `FIN` |
| **Debounce** | Cascada `if-else if` sin reinicio; acumula lecturas espurias | Contadores individuales con reinicio a 0 si la condición se rompe; umbral `DEBOUNCE_LECTURAS` |
| **Callejón sin salida** | No detectado (cae en rama vacía) | Detección simultánea de las 3 paredes con debounce y transición a giro de 180° |
| **Centrado en cruces** | No existe avance previo ni estados de preparación | Avance no bloqueante por encoders (`PULSOS_AVANCE_PREGIRO`) antes de pivotar |
| **PID con huecos** | Se centra considerando distancias de hasta 150 mm; desvío hacia vértices | Cláusula de guarda que descarta el sensor abierto y mantiene distancia con la pared fija |
| **Signos PID** | Signo invertido (acelera hacia la pared más cercana) | Signos alineados para virar alejándose de la pared |
| **Constantes** | Números mágicos (`+ 50`, `5`, `-50, 50`) en código | Centralización 100% en `config.h` |
