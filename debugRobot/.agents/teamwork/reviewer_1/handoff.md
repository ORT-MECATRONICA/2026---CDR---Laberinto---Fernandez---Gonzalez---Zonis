# Reporte de Revisión y Desafío Adversarial — Milestone M1: Odometría y Mecánica de Estados (debugRobot)

**Agente:** Reviewer 1 (`teamwork_preview_reviewer`)  
**Roles:** Reviewer (Evaluación Objetiva) & Critic (Desafío Adversarial)  
**Fecha:** 2026-10-06  
**Ruta de Trabajo:** `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\.agents\teamwork\reviewer_1`  
**Veredicto:** **APPROVE**

---

## 1. Observation

### 1.1 Archivos e Implementación Inspeccionados
Se realizó una inspección estática exhaustiva de los siguientes artefactos:
- `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\src\config.h`
- `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\src\main.cpp`
- `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\src\hardware\movimiento\PID.cpp`
- `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\src\hardware\sensoresDistancia\sensoresDistancia.cpp`
- `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\src\hardware\encoders\encoders.cpp`
- `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\.agents\teamwork\worker_m1\handoff.md`

### 1.2 Hallazgos de Código Específicos
1. **Configuración de Parámetros en `src/config.h` (Líneas 66-73):**
   ```cpp
   #define PULSOS_CELDA 800
   #define PULSOS_CELDA_MEDIA 400
   #define DISTANCIA_PARADA_FRENTE 50
   #define PULSOS_GRACIA_PID 100
   #define PULSOS_MIN_DETECCION_FLANCO 150
   #define PULSOS_WATCHDOG_SEGURIDAD 1050
   #define PULSOS_PREGIRO_90_DER 300
   #define PULSOS_PREGIRO_90_IZQ 280
   ```
   Todas las constantes requeridas están definidas con semántica clara y tipado implícito compatible.

2. **Inicialización Homogénea en `src/main.cpp` (Líneas 39-51):**
   ```cpp
   void iniciarAvanceCelda() {
     resetearEncoders();
     resetearErrorAnterior();
     limitePulsosActual = PULSOS_CELDA;
     flancoDetectado = false;
     habiaParedIzq = false;
     habiaParedDer = false;
     pulsosBaseCelda = 0;
     graciaPIDFinalizada = false;
     sensadoActual = actualizarSensado();
     enviarString(">>> AVANZANDO <<<");
     estado = AVANZANDO;
   }
   ```
   Se invoca en las tres rutas de entrada a `AVANZANDO`:
   - Arranque inicial desde `LISTO` (Línea 85).
   - Avance recto desde `DECISION` (Línea 185).
   - Transición post-giro desde `FRENANDO` (Línea 246).

3. **Mecánica R1 — Reseteo por Flanco Lateral en `src/main.cpp` (Líneas 104-124):**
   ```cpp
   if (!flancoDetectado) {
     if (sensadoActual.distanciaIzq > 0 && sensadoActual.distanciaIzq <= UMBRAL_PARED_ESTADO_NORMAL) {
       habiaParedIzq = true;
     }
     if (sensadoActual.distanciaDer > 0 && sensadoActual.distanciaDer <= UMBRAL_PARED_ESTADO_NORMAL) {
       habiaParedDer = true;
     }

     bool flancoIzq = habiaParedIzq && (sensadoActual.distanciaIzq > UMBRAL_PARED_ESTADO_NORMAL);
     bool flancoDer = habiaParedDer && (sensadoActual.distanciaDer > UMBRAL_PARED_ESTADO_NORMAL);

     if ((flancoIzq || flancoDer) && pulsosTotalesCelda > PULSOS_MIN_DETECCION_FLANCO) {
       flancoDetectado = true;
       pulsosBaseCelda += pulsosActuales;
       resetearEncoders();
       pulsosActuales = 0;
       limitePulsosActual = PULSOS_CELDA_MEDIA;
       enviarString(">>> FLANCO DETECTADO: RESET A 400 PULSOS <<<");
     }
   }
   ```

4. **Mecánica R2 — Alineación con Pared Frontal y Watchdog en `src/main.cpp` (Líneas 126-148):**
   ```cpp
   bool stopPorParedFrontal = (sensadoActual.distanciaCent > 0 &&
                               sensadoActual.distanciaCent <= DISTANCIA_PARADA_FRENTE &&
                               pulsosTotalesCelda > 100);

   bool paredAlFrente = (sensadoActual.distanciaCent > 0 &&
                         sensadoActual.distanciaCent <= UMBRAL_PARED_FRENTE);

   bool stopPorEncoders = (pulsosActuales >= limitePulsosActual) && !paredAlFrente;

   bool stopPorWatchdog = (pulsosTotalesCelda >= PULSOS_WATCHDOG_SEGURIDAD);

   if (stopPorParedFrontal || stopPorEncoders || stopPorWatchdog) {
     enviarString(">>> INGRESO A DECISIÓN (FRENANDO) <<<");
     movimiento(FRENO_F, {0,0});
     tiempoInicioFreno = millis();
     estadoPostFreno = DECISION;
     estado = FRENANDO;
   }
   ```

5. **Mecánica R3 — Gracia PID y Transferencia Suave en `src/main.cpp` (Líneas 150-166):**
   ```cpp
   int16_t correccion = calcularCorreccion(sensadoActual);
   if (!graciaPIDFinalizada) {
     if (pulsosTotalesCelda < PULSOS_GRACIA_PID) {
       correccion = 0;
     } else {
       graciaPIDFinalizada = true;
     }
   }

   velocidadActual.izquierda = constrain(VEL_BASE_DER - correccion, 0, 255);
   velocidadActual.derecha = constrain(VEL_BASE_IZQ + correccion, 0, 255);

   movimiento(AVANZAR, {velocidadActual.izquierda, velocidadActual.derecha});
   ```

6. **Mecánica R4 — Ausencia de Mapeo y Freno Unificado:**
   No se introdujo ninguna estructura matricial, mapa de celdas ni almacenamiento de coordenadas X/Y. Todas las condiciones de frenado convergen de forma uniforme en `FRENANDO` con pausa mecánica no bloqueante de 150 ms.

7. **Observación Menor Externa (Código preexistente no atribuible a Worker 1):**
   En `src/main.cpp` línea 172 (`case DECISION:`):
   `enviarString(sensadoActual.distanciaDer + " | " + sensadoActual.distanciaCent + sensadoActual.distanciaIzq);`
   Existe una concatenación errónea en C++ donde sumar enteros a un literal de cadena `" | "` realiza aritmética de punteros en lugar de concatenación de objetos `String`. Este bug ya existía en la base antes de M1 y no afecta la lógica de control, pero se documenta como recomendación para futuros hitos.

---

## 2. Logic Chain

### 2.1 Cadena Lógica R1: Reseteo por Flanco Lateral
- **Observación 1.2.3:** `habiaParedIzq` se establece en `true` únicamente si `sensadoActual.distanciaIzq > 0 && <= 130`.
- **Inferencia:** Si una celda inicia abierta lateralmente (distancia > 130 mm desde el pulso 0), `habiaParedIzq` permanece en `false`, impidiendo falsos disparos.
- **Inferencia:** Cuando una pared previamente confirmada desaparece (`distancia > 130`), la transición lógica se detecta sin recurrir a llamadas bloqueantes (`delay`).
- **Inferencia:** La guarda `pulsosTotalesCelda > 150` asegura que oscilaciones causadas por la entrada a la celda o rebordes de postes adyacentes no provoquen reseteos tempranos.
- **Inferencia:** El cerrojo `flancoDetectado = true` inhabilita de por vida dentro de la celda el bloque `if (!flancoDetectado)`, previniendo loops infinitos de reseteo.
- **Inferencia:** Al disparar el flanco, `pulsosActuales` se resetea a 0 y `limitePulsosActual` se actualiza a 400 pulsos. Por lo tanto, el robot avanza exactamente 400 pulsos desde la caída de la pared lateral hasta el centro de la celda.
- **Inferencia:** Si no hubo pared desde el inicio, `limitePulsosActual` retiene su valor por defecto de 800 pulsos (`PULSOS_CELDA`), cumpliendo el fallback requerido.

### 2.2 Cadena Lógica R2: Alineación con Pared Frontal
- **Observación 1.2.4:** `stopPorParedFrontal` exige de manera estricta `sensadoActual.distanciaCent > 0`, `distanciaCent <= 50` y `pulsosTotalesCelda > 100`.
- **Inferencia:** La condición `> 0` neutraliza valores nulos o negativos que puedan derivarse de offsets de calibración o lecturas fallidas del VL53L0X.
- **Inferencia:** `pulsosTotalesCelda > 100` descarta ruidos ópticos de arranque inmediatamente después de un giro.
- **Inferencia:** La presencia de `paredAlFrente` (`distanciaCent <= 120`) niega `!paredAlFrente`, inhabilitando `stopPorEncoders` e impidiendo que el robot frene prematuramente a 400 u 800 pulsos cuando se aproxima a una pared frontal.
- **Inferencia:** Si el sensor frontal falla o no detecta la pared, `stopPorWatchdog` (`pulsosTotalesCelda >= 1050`) fuerza una parada preventiva, eliminando el riesgo de colisión o daño por sobrecorriente en motores atascados.

### 2.3 Cadena Lógica R3: Silencio PID y Transferencia Suave
- **Observación 1.2.5:** `calcularCorreccion(sensadoActual)` se ejecuta incondicionalmente en cada iteración del bucle en `AVANZANDO`.
- **Inferencia:** La variable estática `errorAnterior` en `PID.cpp` se actualiza ciclo a ciclo con el error real del robot durante los primeros 100 pulsos.
- **Inferencia:** Al forzar `correccion = 0` para `pulsosTotalesCelda < 100`, los motores operan exclusivamente a su velocidad base sin volantazos.
- **Inferencia:** Al transcurrir el pulso 100, la derivada calculada $(e_k - e_{k-1})$ se evalúa contra el ciclo inmediato anterior y no contra cero, logrando una transferencia continua y libre de picos derivados (*bumpless transfer*).
- **Inferencia:** Dado que `pulsosTotalesCelda = pulsosBaseCelda + pulsosActuales` y `pulsosBaseCelda >= 150`, un reseteo de encoders por R1 a mitad de celda nunca regresa `pulsosTotalesCelda` por debajo de 100 ni reactiva la ceguera del PID, garantizando un desacoplamiento perfecto.

### 2.4 Cadena Lógica R4: Integridad de la Arquitectura FSM
- **Observación 1.2.2 y 1.2.4:** Todas las condiciones de frenado (`stopPorParedFrontal`, `stopPorEncoders`, `stopPorWatchdog`) fijan `movimiento(FRENO_F, {0,0})`, marcan `estadoPostFreno = DECISION` y transicionan a `FRENANDO`.
- **Inferencia:** El robot disipa su energía cinética durante 150 ms en un estado pasivo antes de evaluar los sensores para la toma de decisión, garantizando estabilidad dinámica y lecturas ópticas en reposo.

---

## 3. Evaluaciones Adversariales y Escenarios Críticos (Critic Role)

### 3.1 Prueba de Estrés: Ciclo de Vida del Reseteo de Odometría
- **Escenario:** El robot avanza por un pasillo con pared a la izquierda. Al pulso 420, la pared desaparece.
- **Comportamiento analizado:**
  1. En el ciclo $k$: se detecta `flancoIzq = true`.
  2. `flancoDetectado` pasa a `true`.
  3. `pulsosBaseCelda` se incrementa a 420.
  4. Se invoca `resetearEncoders()`, `pulsosActuales = 0`, `limitePulsosActual = 400`.
  5. En el mismo ciclo $k$: `stopPorEncoders` evalúa `0 >= 400` ($\text{false}$).
  6. En el ciclo $k+1$: `!flancoDetectado` es $\text{false}$; el bloque de reseteo no se re-ejecuta.
  7. El robot avanza hasta que `pulsosActuales >= 400` (pulso total acumulado: $420 + 400 = 820$).
  8. `stopPorEncoders` se activa a 400 pulsos del borde físico.
- **Resultado:** **PASS**. No existe riesgo de bucle de reseteo ni parada prematura.

### 3.2 Prueba de Estrés: Retroalimentación y Signo de Corrección PID
- **Escenario:** El robot se desvía hacia la pared DERECHA (`distanciaDer = 30`, `distanciaIzq = 70`).
- **Comportamiento analizado:**
  - En `PID.cpp`: `error = 30 - 70 = -40`.
  - `correccion = KP * error = 0.5 * (-40) = -20`.
  - En `main.cpp`:
    - `velocidadActual.izquierda = VEL_BASE_DER - correccion = 45 - (-20) = 65`.
    - `velocidadActual.derecha = VEL_BASE_IZQ + correccion = 45 + (-20) = 25`.
  - De acuerdo con el mapeo de hardware documentado: el motor A (`velocidadActual.izquierda`) acciona mecánicamente la rueda DERECHA.
  - La rueda derecha acelera a 65 y la izquierda reduce a 25 $\implies$ el robot vira a la IZQUIERDA (alejándose de la pared derecha).
- **Resultado:** **PASS**. La retroalimentación es estrictamente negativa (estabilizadora), sin inversión de signo.

### 3.3 Prueba de Estrés: Fallo y Desconexión del Sensor Frontal
- **Escenario:** El sensor VL53L0X central se bloquea o retorna permanentemente una lectura fantasma de 90 mm mientras el robot avanza hacia una pared.
- **Comportamiento analizado:**
  - Como $90 \le 120$, `paredAlFrente` se mantiene en $\text{true}$, suprimiendo permanentemente `stopPorEncoders`.
  - Como la lectura no desciende a $\le 50$, `stopPorParedFrontal` nunca se dispara.
  - Al alcanzar `pulsosTotalesCelda == 1050`, la guarda `stopPorWatchdog` se dispara de forma incondicional.
  - El robot ejecuta `FRENO_F` y transiciona a `FRENANDO`.
- **Resultado:** **PASS**. El watchdog previene colisiones catastróficas y bloqueo de motores.

---

## 4. Auditoría de Integridad (Integrity Check)

Conforme a las directrices de integridad del revisor:
1. **Resultados de prueba o salidas hardcodeadas:** Ninguna detectada. El comportamiento se calcula dinámicamente mediante lecturas de hardware.
2. **Implementaciones fachada (*facades*) o dummies:** Ninguna detectada. Toda la lógica manipula registros y periféricos reales del microcontrolador.
3. **Atajos o elusión de requerimientos:** Ninguno. Se abordaron en su totalidad R1, R2, R3 y R4 sin recurrir a bibliotecas externas superfluas.
4. **Artefactos o registros de verificación fabricados:** Ninguno. La evidencia técnica coincide exactamente con el código fuente.
5. **Autocertificación no verificada:** Descartada mediante análisis estático independiente de este revisor.

---

## 5. Caveats

- **Pruebas en Hardware Físico:** La verificación se realizó mediante inspección estática rigurosa, trazado formal de lógica de estados y comprobación cruzada de tipos. No se dispuso de enlace USB con la placa física ESP32 durante la sesión.
- **Reflexión Óptica Extrema:** El comportamiento de los sensores ToF en presencia de espejos o superficies negras absorbentes no forma parte del alcance del firmware evaluado.
- No se identifican otros caveats relevantes.

---

## 6. Conclusion

La solución provista en `src/main.cpp` y `src/config.h` para el Milestone M1:
1. Resuelve con precisión el reseteo por flanco lateral con cerrojo antirrebote y fallback a 800 pulsos (R1).
2. Garantiza la detención frontal a $\le 50\text{ mm}$ con guarda $> 0\text{ mm}$, supresión de odometría ante pared frontal y watchdog de seguridad a 1050 pulsos (R2).
3. Implementa el periodo de gracia de 100 pulsos con desacoplamiento de odometría y transferencia suave (*bumpless*) en el PID (R3).
4. Mantiene la pureza de la arquitectura FSM y la estricta ausencia de mapas de memoria o matrices (R4).

**Veredicto Final:** **APPROVE**.

---

## 7. Verification Method

Para verificar independientemente este reporte:
1. Inspeccionar `src/config.h` (líneas 66-72) y validar las constantes numéricas.
2. Inspeccionar `src/main.cpp`:
   - Líneas 39-51 (`iniciarAvanceCelda`).
   - Líneas 104-124 (lógica R1).
   - Líneas 126-148 (lógica R2 y watchdog).
   - Líneas 150-166 (lógica R3).
   - Líneas 242-254 (estado `FRENANDO`).
3. Comprobar que ninguna rama en `src/main.cpp` contenga variables no inicializadas ni funciones bloqueantes en el estado de marcha.
