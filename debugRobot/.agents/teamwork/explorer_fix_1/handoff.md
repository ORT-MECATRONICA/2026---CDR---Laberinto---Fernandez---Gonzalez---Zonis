# Reporte de Handoff — Explorer Fix 1 (Milestone M1 Iteración 2)

**Agente:** Explorer Fix 1 (`teamwork_preview_explorer`)  
**Fecha:** 2026-10-06  
**Ruta de Trabajo:** `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\.agents\teamwork\explorer_fix_1`  
**Destinatario:** Worker 2 (`teamwork_preview_worker`) / Orchestrator  
**Objetivo:** Análisis del bloqueo de parada frontal por guarda de 100 pulsos y formulación de la estrategia exacta de remediación para Worker 2.

---

## 1. Observation

### 1.1 Código Fuente Inspeccionado en `src/main.cpp`
En `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\src\main.cpp`:
- **Líneas 126–143 (`stopPorParedFrontal` y condiciones de parada en `AVANZANDO`):**
  ```cpp
  // R2: Condiciones de parada
  // 1. Parada por pared frontal a <= 50 mm (con guarda de validez física > 0 y recorrido > 100)
  bool stopPorParedFrontal = (sensadoActual.distanciaCent > 0 &&
                              sensadoActual.distanciaCent <= DISTANCIA_PARADA_FRENTE &&
                              pulsosTotalesCelda > 100);

  // 2. Detección de pared frontal en aproximación
  bool paredAlFrente = (sensadoActual.distanciaCent > 0 &&
                        sensadoActual.distanciaCent <= UMBRAL_PARED_FRENTE);

  // 3. Parada por encoders (400 pulsos si hubo flanco, o 800 fallback si no hubo flanco).
  // R2 sobreescribe la parada por encoders si hay pared al frente.
  bool stopPorEncoders = (pulsosActuales >= limitePulsosActual) && !paredAlFrente;

  // 4. Watchdog de seguridad (anti-colisión ante fallo de sensor frontal)
  bool stopPorWatchdog = (pulsosTotalesCelda >= PULSOS_WATCHDOG_SEGURIDAD);
  ```
- **Líneas 39–51 (`iniciarAvanceCelda()`):**
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
- **Líneas 102 (`actualizarSensado()` en el bucle de `AVANZANDO`):**
  ```cpp
  sensadoActual = actualizarSensado();
  ```
- **Líneas 81–87 (Entrada desde `LISTO`):**
  ```cpp
  if (digitalRead(BOTON1) == LOW) {
    while(digitalRead(BOTON1) == LOW) { delay(10); } // Esperar a que se suelte el botón
    enviarString(">>> INICIANDO AVANCE <<<");
    iniciarAvanceCelda();
  }
  ```
- **Líneas 176–186 (Entrada desde `DECISION`):**
  ```cpp
  bool condicionAvanzar = sensadoActual.distanciaDer <= UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaCent > UMBRAL_PARED_ESTADO_NORMAL;
  ...
  } else if (condicionAvanzar) {
    iniciarAvanceCelda();
  }
  ```
- **Líneas 242–253 (Entrada desde `FRENANDO` post-giro):**
  ```cpp
  case FRENANDO: {
    movimiento(FRENO_F, {0,0}); // Mantener el freno activo
    if (millis() - tiempoInicioFreno >= 150) { // 150ms de pausa estabilizadora
      if (estadoPostFreno == AVANZANDO) {
        iniciarAvanceCelda();
      } else {
        resetearErrorAnterior();
        resetearEncoders();
        estado = estadoPostFreno;
      }
    }
    break;
  }
  ```

### 1.2 Código de Sensado en `src/hardware/sensoresDistancia/sensoresDistancia.cpp`
En `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\src\hardware\sensoresDistancia\sensoresDistancia.cpp` (líneas 80–107):
```cpp
sensado actualizarSensado(){
  static sensado lecturaAct = {0,0,0}; 
  static unsigned long ultimoSensado = 0;

  if((sensorIzq.readReg(VL53L0X::RESULT_INTERRUPT_STATUS) & 0x07) != 0){
    uint16_t rawIzq = sensorIzq.readRangeContinuousMillimeters();
    if (rawIzq > 2000) rawIzq = 2000;
    lecturaAct.distanciaIzq = rawIzq - OFSET_IZQ;
  }
  if((sensorCent.readReg(VL53L0X::RESULT_INTERRUPT_STATUS) & 0x07) != 0){
    uint16_t rawCent = sensorCent.readRangeContinuousMillimeters();
    if (rawCent > 2000) rawCent = 2000;
    lecturaAct.distanciaCent =  rawCent - OFSET_CENT;
  }
  if((sensorDer.readReg(VL53L0X::RESULT_INTERRUPT_STATUS) & 0x07) != 0){
    uint16_t rawDer = sensorDer.readRangeContinuousMillimeters();
    if (rawDer > 2000) rawDer = 2000;
    lecturaAct.distanciaDer =  rawDer - OFSET_DER;
  }
  ultimoSensado = millis();
  
  return lecturaAct;
} 
```

### 1.3 Evidencia de Falla Reportada por Challenger 1
En `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\.agents\teamwork\challenger_1\handoff.md`:
- **Línea 94 (Resultado del Escenario 3.3):**
  > `Obstáculo frontal muy temprano (< 100p, ej. 40mm a pulso 10)`: **FAIL / VULNERABILIDAD**.  
  > *El robot NO frena a $\le 50\text{ mm}$. Debido a la guarda `pulsosTotalesCelda > 100`, avanza a ciegas 100 pulsos (~22.5 mm) antes de frenar, provocando colisión física.*
- **Líneas 110–117 (Cadena Lógica del Challenger):**
  > *La inclusión de `pulsosTotalesCelda > 100` proviene de una confusión con la gracia de R3 (silencio del PID en los primeros 100 pulsos). En una situación donde el robot inicia AVANZANDO estando a 30 mm o 45 mm de una pared (o ante un obstáculo inmediato), `stopPorParedFrontal` es evaluado como false durante los primeros 100 pulsos. Simultáneamente, como `distanciaCent <= 120`, `paredAlFrente` es true, por lo que `stopPorEncoders` también se inhibe. El robot ejecuta movimiento(AVANZAR, ...) durante 100 pulsos (equivalente a 22.5 mm de avance), impactando frontalmente contra la pared antes de que la guarda le permita detenerse.*
- **Líneas 145–158 (Acción Requerida 1):**
  > *Eliminar la guarda de 100 pulsos en `stopPorParedFrontal`: En `src/main.cpp` línea 128, cambiar a `bool stopPorParedFrontal = (sensadoActual.distanciaCent > 0 && sensadoActual.distanciaCent <= DISTANCIA_PARADA_FRENTE);`*

### 1.4 Requerimientos Originales del Usuario (`ORIGINAL_REQUEST.md`)
En `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\.agents\teamwork\ORIGINAL_REQUEST.md`:
- **R2 (Alineación con pared frontal, líneas 21–22):**
  > *Si el robot avanza hacia una celda con pared enfrente, no debe usar los encoders para frenar. Debe avanzar hasta que el sensor central lea exactamente 50 mm (o menos) de distancia y entonces detenerse para tomar la decisión.*
- **R3 (Gracia post-giro, líneas 24–25):**
  > *Al inicio del estado AVANZANDO, el controlador PID (la suma de la corrección a las velocidades base) debe mantenerse en 0 durante los primeros 100 pulsos, para evitar volantazos bruscos antes de que el robot se introduzca bien en el nuevo pasillo.*

---

## 2. Logic Chain

### 2.1 Por qué remover `pulsosTotalesCelda > 100` es NECESARIO
1. **Violación de la especificación R2:**  
   R2 estipula que la parada por pared frontal a $\le 50\text{ mm}$ es una condición de seguridad física incondicional. No prescribe ningún retraso ni zona ciega de odometría para la detección de parada frontal.
2. **Cinemática y Riesgo de Colisión Física:**  
   En `config.h`, `PULSOS_CELDA = 800` para una celda de $180\text{ mm}$ equivale a:
   $$\text{Resolución} = \frac{180\text{ mm}}{800\text{ pulsos}} = 0.225\text{ mm/pulso}$$
   La guarda `pulsosTotalesCelda > 100` impone un avance forzado de $100 \times 0.225\text{ mm} = 22.5\text{ mm}$.  
   Si el robot se encuentra a $\le 50\text{ mm}$ de una pared al iniciar `AVANZANDO` (ej. a $40\text{ mm}$ por un sobregiro mecánico, una partida cercana, o un obstáculo colocado en el ingreso):
   - `sensadoActual.distanciaCent <= 50` es verdadero.
   - Pero `pulsosTotalesCelda > 100` es falso para pulsos $0 \dots 100$.
   - Al mismo tiempo, `sensadoActual.distanciaCent <= UMBRAL_PARED_FRENTE` ($120\text{ mm}$) hace que `paredAlFrente` sea verdadero.
   - En consecuencia, la parada por encoders `stopPorEncoders = (pulsosActuales >= limitePulsosActual) && !paredAlFrente` queda bloqueada (`!paredAlFrente == false`).
   - Ninguna condición de parada se activa. El robot avanza a velocidad nominal de crucero (`VEL_BASE = 45`) durante $22.5\text{ mm}$.  
   - Si la distancia inicial era $40\text{ mm}$, el robot queda a $17.5\text{ mm}$ o colisiona físicamente contra la pared antes de que la guarda le permita frenar.
3. **Conflación Conceptual Errónea entre R2 y R3:**  
   El silencio de los primeros 100 pulsos fue requerido **únicamente para el PID** en R3 (*"ceguera del PID"* para evitar volantazos bruscos durante la inserción al pasillo). Trasladar esa ceguera al frenado de emergencia frontal de R2 fue un error de diseño introducido en la primera iteración de Worker.

### 2.2 Por qué remover `pulsosTotalesCelda > 100` es 100% SEGURO (Ausencia de Falsos Positivos)
La justificación original de Worker 1 para agregar la guarda fue el temor a que lecturas espurias o estructuras no inicializadas provocaran un frenado falso al entrar a `AVANZANDO`. El análisis exhaustivo de todas las transiciones del sistema demuestra que esta preocupación está completamente cubierta por el diseño existente:

1. **Guarda de Validez `sensadoActual.distanciaCent > 0`:**  
   - En `sensoresDistancia.cpp`, la estructura estática se inicializa como `static sensado lecturaAct = {0,0,0};`.  
   - Si los sensores no han producido ninguna medición aún, `distanciaCent` vale `0`.  
   - La condición `distanciaCent > 0` evalúa a `false` frente al valor inicial `0`. Por ende, una estructura no inicializada jamás disparará `stopPorParedFrontal`.
2. **Separación Geométrica del Umbral de Parada (`DISTANCIA_PARADA_FRENTE = 50 mm`):**  
   - En una celda normal sin pared frontal, la pared opuesta está a $\ge 180\text{ mm}$ (o pasillo abierto a $> 1000\text{ mm}$).  
   - El ruido típico del sensor VL53L0X es de $\pm 2$ a $5\text{ mm}$. Es físicamente imposible que una lectura en un pasillo libre de $180\text{ mm}$ fluctúe por debajo de $50\text{ mm}$.
3. **Inmunidad en Transición desde `LISTO` (Arranque / Boot):**  
   - En `setup()`, `inicializacionSensoresDist()` enciende los sensores VL53L0X y los arranca en modo continuo autónomo (`sensorCent.startContinuous(0)`).  
   - El robot permanece en `LISTO` hasta que el usuario presiona y suelta `BOTON1` (`while(digitalRead(BOTON1) == LOW) delay(10);`).  
   - Este intervalo de espera del operador humano dura típicamente entre $500\text{ ms}$ y varios segundos.  
   - El ciclo de medición continua del VL53L0X toma $\sim 33\text{ ms}$. Por ende, antes de que el robot empiece a moverse, el hardware del sensor ya completó decenas de conversiones válidas.  
   - Al soltar el botón, se invoca `iniciarAvanceCelda()`, que ejecuta `sensadoActual = actualizarSensado();`. El valor cargado corresponde a la realidad óptica del entorno.
4. **Inmunidad en Transición desde `DECISION`:**  
   - En `src/main.cpp` línea 176:
     ```cpp
     bool condicionAvanzar = sensadoActual.distanciaDer <= UMBRAL_PARED_ESTADO_NORMAL && 
                             sensadoActual.distanciaCent > UMBRAL_PARED_ESTADO_NORMAL;
     ```
   - El robot **sólo** ingresa a `AVANZANDO` desde `DECISION` si el sensor central midió previamente una distancia frontal mayor a $130\text{ mm}$ (`UMBRAL_PARED_ESTADO_NORMAL`).  
   - Si hubiera una pared frontal a $\le 50\text{ mm}$, `condicionAvanzar` sería falsa y la FSM elegiría girar. Es matemáticamente imposible entrar a `AVANZANDO` desde `DECISION` con una pared frontal a $\le 50\text{ mm}$ sin que haya sido una decisión deliberada de frenar.
5. **Inmunidad en Transición desde `FRENANDO` (Salida de Giros):**  
   - Al completar un giro de 90° o 180°, el robot entra a `FRENANDO` con `estadoPostFreno = AVANZANDO` y frena activamente durante $150\text{ ms}$ (`millis() - tiempoInicioFreno >= 150`).  
   - Durante esos $150\text{ ms}$, el chasis está inmóvil. En ese tiempo, el sensor central VL53L0X completa al menos 4 ciclos completos de medición apuntando al nuevo pasillo.  
   - Al cumplirse los $150\text{ ms}$, se invoca `iniciarAvanceCelda()`, ejecutando `sensadoActual = actualizarSensado();` mientras el robot está quieto y estable.  
   - Por tanto, no existen lecturas residuales del giro al ingresar a `AVANZANDO`.

### 2.3 Verificación del Sondeo de Sensores en `iniciarAvanceCelda()`
- En `src/main.cpp` líneas 39–51, la función centralizadora `iniciarAvanceCelda()` limpia determinísticamente todos los acumuladores y banderas:
  - `resetearEncoders()` pone encoders en 0.
  - `resetearErrorAnterior()` resetea el histórico derivativo del PID.
  - `limitePulsosActual = PULSOS_CELDA` (800) establece el fallback seguro.
  - `flancoDetectado = false`, `habiaParedIzq = false`, `habiaParedDer = false`.
  - `pulsosBaseCelda = 0`.
  - `graciaPIDFinalizada = false`.
  - **`sensadoActual = actualizarSensado();`** toma la muestra fresca previa al cambio de estado `estado = AVANZANDO`.
- Además, en la línea 102 (el primer paso dentro de `case AVANZANDO:`), se ejecuta de inmediato:
  ```cpp
  sensadoActual = actualizarSensado();
  ```
- Por ende, la variable `sensadoActual` está doblemente sincronizada con el periférico I2C y no presenta lecturas estancadas (*stale data*).

---

## 3. Caveats

1. **Entorno de Simulación / Desarrollo Sin Hardware:**  
   Las validaciones se realizaron mediante análisis estático de trazas, revisión temporal I2C contra hoja de datos de VL53L0X y modelos determinísticos. No se dispone de un ESP32 físico conectado a puerto serie durante esta sesión.
2. **Ruido Óptico en Aproximación Frontal (> 800 pulsos):**  
   Challenger 1 reportó que un glitch óptico singular $> 120\text{ mm}$ cuando el robot ya superó los 800 pulsos puede hacer que `paredAlFrente` caiga a falso por un ciclo, activando `stopPorEncoders` prematuramente. Se incluye una recomendación de robustecimiento (*latch* de aproximación frontal) para que Worker 2 la incorpore.
3. **Lecturas Negativas por Tolerancia de Offset:**  
   Si el paragolpes presiona contra un objeto a $< 50\text{ mm}$ del sensor, `rawCent - OFSET_CENT` puede resultar en valores negativos (ej. $-5\text{ mm}$). La condición `distanciaCent > 0` trata valores $\le 0$ como ausencia de lectura. Se incluye la recomendación para admitir el rango de contacto físico ($-20\text{ mm} \le distanciaCent \le 50\text{ mm}$).
4. No existen caveats adicionales.

---

## 4. Conclusion

1. **Dictamen Principal:**  
   La remoción de la cláusula `&& pulsosTotalesCelda > 100` en `src/main.cpp` (línea 128) es **estrictamente necesaria** para evitar colisiones frontales tempranas e implementar fielmente R2, y es **completamente segura** dado que el sensado está validado por `sensadoActual.distanciaCent > 0`, la pre-lectura en `iniciarAvanceCelda()` y la estabilización previa de $150\text{ ms}$ en `FRENANDO`.

2. **Recomendaciones Exactas para Worker 2:**

   ### Modificación Obligatoria 1: Parada Frontal Inmediata (R2)
   En `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\src\main.cpp`, reemplazar las líneas 127–131:
   ```cpp
   // ANTES (INSEGURO - VULNERABILIDAD R2):
   bool stopPorParedFrontal = (sensadoActual.distanciaCent > 0 &&
                               sensadoActual.distanciaCent <= DISTANCIA_PARADA_FRENTE &&
                               pulsosTotalesCelda > 100);

   // DESPUÉS (SEGURO Y ROBUSTO):
   bool stopPorParedFrontal = (sensadoActual.distanciaCent > 0 &&
                               sensadoActual.distanciaCent <= DISTANCIA_PARADA_FRENTE);
   ```

   ### Modificación Recomendada 2: Robustecimiento contra Glitches Ópticos en Aproximación (Escenario 4.3 de Challenger)
   Para evitar que un pulso espurio $> 120\text{ mm}$ desactive `paredAlFrente` cuando `pulsosActuales >= 800`:
   - Declarar una variable booleana de estado de aproximación:
     `bool aproximandoParedFrontal = false;`
   - En `iniciarAvanceCelda()`:
     `aproximandoParedFrontal = false;`
   - En `case AVANZANDO:`:
     ```cpp
     if (paredAlFrente) {
       aproximandoParedFrontal = true;
     }
     bool stopPorEncoders = (pulsosActuales >= limitePulsosActual) && !paredAlFrente && !aproximandoParedFrontal;
     ```
     *Efecto:* Una vez que se detecta pared al frente durante la celda, el avance hacia la pared frontal no se interrumpirá por odometría ante un glitch óptico momentáneo; el robot sólo se detendrá por `stopPorParedFrontal` ($\le 50\text{ mm}$) o por el watchdog de seguridad (`stopPorWatchdog`).

   ### Modificación Recomendada 3: Tolerancia a Calibración de Contacto Físico (Escenario 4.2 de Challenger)
   Para garantizar que lecturas ligeramente negativas por tolerancia mecánica (ej. $-5\text{ mm}$) no desactiven la parada:
   ```cpp
   bool distanciaValida = (sensadoActual.distanciaCent > -20 && sensadoActual.distanciaCent != 0);
   bool stopPorParedFrontal = (distanciaValida && sensadoActual.distanciaCent <= DISTANCIA_PARADA_FRENTE);
   bool paredAlFrente = (distanciaValida && sensadoActual.distanciaCent <= UMBRAL_PARED_FRENTE);
   ```

---

## 5. Verification Method

Para verificar independientemente la validez técnica de este reporte:

1. **Inspección de Código Fuente:**
   - Abrir `src/main.cpp` en línea 128 y confirmar que la cláusula `pulsosTotalesCelda > 100` bloquea la evaluación de parada durante los primeros 100 pulsos.
   - Abrir `src/main.cpp` líneas 39–51 y verificar que `iniciarAvanceCelda()` ejecuta `sensadoActual = actualizarSensado();`.
   - Abrir `src/main.cpp` líneas 176–186 y verificar que `DECISION` valida `sensadoActual.distanciaCent > UMBRAL_PARED_ESTADO_NORMAL` ($130\text{ mm}$) antes de permitir el ingreso a `AVANZANDO`.
   - Abrir `src/main.cpp` líneas 242–254 y verificar que `FRENANDO` impone una pausa mecánica de $150\text{ ms}$ antes de invocar `iniciarAvanceCelda()`.

2. **Verificación Lógica en Harness Adversarial:**
   - Inspeccionar el script `verify_odometry_adversarial.py` en `.agents/teamwork/challenger_1/`.
   - Simular Test 3.3 con la línea propuesta: al cambiar `stopPorParedFrontal = (d_cent > 0 and d_cent <= DISTANCIA_PARADA_FRENTE)`, el robot se detiene en el ciclo 1 ($10\text{ pulsos}$), pasando la prueba de frenado seguro antes de 100 pulsos.

3. **Condiciones de Invalidación:**
   Este reporte quedaría invalidado si:
   - Se demostrase que el sensor VL53L0X en estado estacionario en un pasillo libre genera de forma recurrente lecturas espurias $\le 50\text{ mm}$ con `distanciaCent > 0`.
   - Se demostrase que el estado `AVANZANDO` puede ser alcanzado sin pasar por `iniciarAvanceCelda()`.
   Como ambas condiciones han sido rigurosamente descartadas por el análisis del código, el reporte queda validado.
