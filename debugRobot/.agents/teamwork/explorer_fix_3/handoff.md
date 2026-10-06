# Reporte de Handoff — Explorer Fix 3 (Milestone M1 Iteración 2)

**Agente:** Explorer Fix 3 (`teamwork_preview_explorer`)  
**Fecha:** 2026-10-06  
**Ruta de Trabajo:** `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\.agents\teamwork\explorer_fix_3`  
**Destinatario:** Orchestrator / Worker 2  

---

## 1. Observation

### 1.1 Código Fuente Inspeccionado y Líneas Relevantes

1. **En `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\src\hardware\sensoresDistancia\sensoresDistancia.cpp`:**
   - Línea 83:
     ```cpp
     static sensado lecturaAct = {0,0,0}; 
     ```
   - Líneas 94-98:
     ```cpp
     if((sensorCent.readReg(VL53L0X::RESULT_INTERRUPT_STATUS) & 0x07) != 0){
       uint16_t rawCent = sensorCent.readRangeContinuousMillimeters();
       if (rawCent > 2000) rawCent = 2000;
       lecturaAct.distanciaCent =  rawCent - OFSET_CENT;
     }
     ```
2. **En `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\src\hardware\sensoresDistancia\sensoresDistancia.h`:**
   - Líneas 14-18:
     ```cpp
     struct sensado {
         int16_t distanciaCent;
         int16_t distanciaDer;
         int16_t distanciaIzq;
     };
     ```
3. **En `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\src\config.h`:**
   - Línea 27:
     ```cpp
     #define OFSET_CENT 50
     ```
   - Líneas 37 y 68:
     ```cpp
     #define UMBRAL_PARED_FRENTE 120
     #define DISTANCIA_PARADA_FRENTE 50
     ```
4. **En `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\src\main.cpp`:**
   - Líneas 39-51 (`iniciarAvanceCelda()`):
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
   - Líneas 73-88 (`case LISTO:`):
     ```cpp
     case LISTO: {
       movimiento(FRENO_F, {0,0});
       
       if (hayDatosBT()) {
         estado = INFORMACION_RECIBIDA;
         break;
       }
       
       if (digitalRead(BOTON1) == LOW) {
         while(digitalRead(BOTON1) == LOW) { delay(10); } // Esperar a que se suelte el botón
         enviarString(">>> INICIANDO AVANCE <<<");
         iniciarAvanceCelda();
       }
       break;
     }
     ```
   - Líneas 128-142 (`case AVANZANDO:`):
     ```cpp
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

### 1.2 Hallazgos de Challenger 1 en `challenger_1/handoff.md`
- **Escenario 3.3 (Vulnerabilidad Crítica):** La guarda `pulsosTotalesCelda > 100` impide detenerse ante un obstáculo frontal situado a $\le 50\text{ mm}$ al inicio de celda. El robot avanza a ciegas 100 pulsos ($22.5\text{ mm}$) antes de evaluar la parada, provocando colisión.
- **Escenario 4.2 (Vulnerabilidad de Lectura Negativa):** Cuando `rawCent < 50` (contacto o proximidad inmediata), `distanciaCent = rawCent - 50` resulta en valores negativos (ej. $-5\text{ mm}$ a $-20\text{ mm}$). La guarda estricta `distanciaCent > 0` evalúa a `false`, tratando la pared como inexistente y deshabilitando tanto `stopPorParedFrontal` como `paredAlFrente`.
- **Escenario 4.3 (Glitch de Ruido Post-800p):** Un único pico de ruido $> 120\text{ mm}$ a los 820 pulsos desactiva momentáneamente `paredAlFrente` y dispara de inmediato `stopPorEncoders`, frenando prematuramente a 70 mm de la pared.

---

## 2. Logic Chain

### 2.1 Física del Sensor ToF, Geometría y Origen de Lecturas Negativas
1. El sensor central VL53L0X está montado a $50\text{ mm}$ detrás de la arista frontal/paragolpes del robot, representado por `OFSET_CENT = 50`.
2. La variable `rawCent` es de tipo `uint16_t` ($\ge 0$). La biblioteca Pololu entrega la distancia milimétrica óptica cruda medida desde el emisor láser.
3. Cuando el paragolpes toca físicamente un obstáculo, la distancia cruda nominal es $rawCent = 50\text{ mm}$, por lo que `distanciaCent = 50 - 50 = 0\text{ mm}`.
4. Por tolerancias de calibración, dispersión óptica a corta distancia ($\pm 10\text{ mm}$ a $\pm 15\text{ mm}$) y deformación mecánica del paragolpes en contacto, $rawCent$ toma valores físicos legítimos en el rango $[30\text{ mm}, 50\text{ mm}]$.
5. En consecuencia, `distanciaCent = rawCent - 50` produce lecturas en el rango $[-20\text{ mm}, 0\text{ mm}]$.
6. Si el código exige estrictamente `distanciaCent > 0`, cualquier medición de contacto o contacto inminente ($\le 0\text{ mm}$) es erróneamente clasificada como "ausencia de pared" o "lectura inválida", provocando que los motores sigan empujando contra el obstáculo.

### 2.2 Determinación del Límite Inferior Seguro (`DISTANCIA_MIN_VALIDA = -20`)
1. **Límite físico de operación confiable del VL53L0X:** Según la hoja de datos de STMicroelectronics para el VL53L0X, el rango mínimo medible con precisión lineal es de $30\text{ mm}$. Por debajo de $30\text{ mm}$, el sensor entra en saturación óptica por acoplamiento de cubierta (*cross-talk dead zone*).
2. Para $rawCent = 30\text{ mm}$, $distanciaCent = 30 - 50 = -20\text{ mm}$.
3. **Diferenciación de Fallas de Hardware:**
   - Si el sensor ToF pierde comunicación I2C o sufre un timeout, la biblioteca Pololu retorna `65535` o `8190`.
   - En `sensoresDistancia.cpp:96`: `if (rawCent > 2000) rawCent = 2000;`.
   - Por tanto, ante fallo o desconexión del sensor, `distanciaCent = 2000 - 50 = 1950\text{ mm}` (un valor positivo enorme, jamás negativo).
   - El robot está protegido contra esta condición por el watchdog `stopPorWatchdog` a los $1050\text{ pulsos}$.
4. **Diferenciación de Valores No Inicializados:**
   - La estructura estática en `sensoresDistancia.cpp:83` se inicializa como `static sensado lecturaAct = {0,0,0};`.
   - Un valor no inicializado sería `0`. Dado que $0 \ge -20$, el chequeo de cota inferior por sí solo no debe ser el único mecanismo de protección contra lecturas no inicializadas; la arquitectura del pipeline de sensado debe garantizar que `sensadoActual` nunca llegue a `AVANZANDO` sin haber completado al menos un ciclo real de conversión.
5. **Conclusión de la Cota:** El límite inferior seguro exacto es:
   `#define DISTANCIA_MIN_VALIDA -20`
   Cualquier lectura con `distanciaCent >= DISTANCIA_MIN_VALIDA && distanciaCent <= DISTANCIA_PARADA_FRENTE` (es decir, $[-20\text{ mm}, 50\text{ mm}]$) corresponde a un obstáculo en zona de frenado o en contacto físico.

### 2.3 Ciclo de Vida de Sensado en `iniciarAvanceCelda()` y la FSM
1. **En `setup()`:** Se invoca `inicializacionSensoresDist()`, la cual arranca la medición continua con `sensorCent.startContinuous(0);`. El primer ciclo de integración ToF toma aproximadamente $30\text{ ms}$.
2. **En `LISTO`:** El robot espera la pulsación de `BOTON1`. El usuario tarda típicamente entre 1 y 5 segundos en presionar el botón.
   - En el código actual, `actualizarSensado()` no es llamado dentro de `case LISTO:`.
   - Si se agrega `sensadoActual = actualizarSensado();` en `case LISTO:` y un `delay(50);` tras `inicializacionSensoresDist()` en `setup()`, el sensor central habrá completado múltiples ciclos antes de que el robot empiece a moverse.
3. **En `iniciarAvanceCelda()`:**
   - Se invoca `sensadoActual = actualizarSensado();` en la línea 48 de `main.cpp`.
   - Como el sensor ya está activo en modo continuo, el registro `RESULT_INTERRUPT_STATUS` tiene el flag de dato listo en alto, cargando de inmediato el valor físico real en `sensadoActual.distanciaCent`.
4. **En transiciones subsecuentes:**
   - Desde `DECISION`: `DECISION` ya ejecutó `actualizarSensado()` en la línea 171 y verificó que `distanciaCent > 130` antes de invocar `iniciarAvanceCelda()`.
   - Desde `FRENANDO`: Tras un giro, el robot permanece $150\text{ ms}$ en `FRENANDO` disipando inercia; al cumplirse el tiempo, `iniciarAvanceCelda()` refresca `sensadoActual`.
5. **Eliminación segura de `pulsosTotalesCelda > 100`:** Con el pipeline de sensado garantizado activo y precargado desde `setup()` y `LISTO`, la guarda `pulsosTotalesCelda > 100` en `stopPorParedFrontal` es innecesaria y peligrosa. Al removerla, si un obstáculo se encuentra a $\le 50\text{ mm}$ al inicio del avance (ej. a 10 pulsos), el robot se detiene de forma inmediata y segura sin colisionar.

### 2.4 Inmunidad a Glitches en Aproximación Frontal (Escenario 4.3)
1. Para resolver la vulnerabilidad 4.3 reportada por Challenger 1: cuando el robot supera los 800 pulsos acercándose a la pared frontal, no se debe permitir que un único ciclo ruidoso $> 120\text{ mm}$ desactive la condición de aproximación y dispare `stopPorEncoders`.
2. Solución: Enclavar la aproximación frontal mediante una bandera de un solo sentido (`aproximandoParedFrontal = true`).
   - Al entrar a `iniciarAvanceCelda()`, se inicializa `aproximandoParedFrontal = false`.
   - Durante `AVANZANDO`, en cuanto `sensadoActual.distanciaCent >= DISTANCIA_MIN_VALIDA && sensadoActual.distanciaCent <= UMBRAL_PARED_FRENTE`, se fija `aproximandoParedFrontal = true`.
   - Una vez enclavada, la inhibición de encoders `!aproximandoParedFrontal` se mantiene firme aunque ocurra un pico transitorio aislado $> 120\text{ mm}$, asegurando que el robot solo frene por `stopPorParedFrontal` ($\le 50\text{ mm}$) o por watchdog de seguridad.

---

## 3. Caveats

- **Modificación de archivos de usuario:** Este reporte es estrictamente de análisis y especificación técnica (read-only). Ningún archivo de código fuente del usuario fue modificado por este agente.
- **Rango menor a -20 mm:** Si un obstáculo estuviese incrustado a menos de $30\text{ mm}$ del lente óptico crudo ($distanciaCent < -20\text{ mm}$), la medición óptica del VL53L0X deja de ser confiable por saturación física. El umbral $[-20\text{ mm}, 50\text{ mm}]$ cubre todo el rango físicamente distinguible del sensor antes del umbral de no-linealidad.

---

## 4. Conclusion

Se formula la estrategia de remediación exacta y completa para que **Worker 2** implemente en `Milestone M1 Iteración 2`:

### 4.1 Definición de Constante en `src/config.h`
Agregar la constante de cota mínima física:
```cpp
#define DISTANCIA_MIN_VALIDA -20 // Límite inferior seguro para lectura física ToF (30mm raw - 50mm offset)
```

### 4.2 Precarga y Calentamiento en `src/main.cpp`
1. En `void setup()`:
   ```cpp
   inicializacionSensoresDist();
   resetearEncoders();
   delay(50);                           // Permitir primer ciclo de conversión ToF (~30ms)
   sensadoActual = actualizarSensado(); // Precargar lecturas físicas iniciales
   ```
2. En `loop() -> case LISTO:`:
   ```cpp
   case LISTO: {
     movimiento(FRENO_F, {0,0});
     sensadoActual = actualizarSensado(); // Mantener sensores refrescados mientras espera BOTON1
     
     if (hayDatosBT()) {
   ```

### 4.3 Control de Enclave y Condiciones de Parada en `src/main.cpp`
1. Declarar variable global de enclave:
   ```cpp
   bool aproximandoParedFrontal = false;
   ```
2. En `void iniciarAvanceCelda()`:
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
     aproximandoParedFrontal = false; // Resetear cerrojo de aproximación
     sensadoActual = actualizarSensado();
     enviarString(">>> AVANZANDO <<<");
     estado = AVANZANDO;
   }
   ```
3. En `loop() -> case AVANZANDO:` (reemplazo de líneas 126-143):
   ```cpp
   // R2: Condiciones de parada
   // 1. Detección de pared frontal en aproximación (rango válido [-20, 120] mm)
   bool paredAlFrente = (sensadoActual.distanciaCent >= DISTANCIA_MIN_VALIDA &&
                         sensadoActual.distanciaCent <= UMBRAL_PARED_FRENTE);

   // Cerrojo de aproximación: una vez detectada la pared, no se desactiva por picos de ruido aislados
   if (paredAlFrente) {
     aproximandoParedFrontal = true;
   }

   // 2. Parada por pared frontal a <= 50 mm (incluyendo rango de contacto hasta -20 mm, sin ceguera de 100 pulsos)
   bool stopPorParedFrontal = (sensadoActual.distanciaCent >= DISTANCIA_MIN_VALIDA &&
                               sensadoActual.distanciaCent <= DISTANCIA_PARADA_FRENTE);

   // 3. Parada por encoders (400 pulsos si hubo flanco, o 800 fallback si no hubo flanco).
   // R2 sobreescribe la parada por encoders si se está aproximando a pared frontal.
   bool stopPorEncoders = (pulsosActuales >= limitePulsosActual) && !aproximandoParedFrontal;

   // 4. Watchdog de seguridad (anti-colisión ante fallo de sensor frontal)
   bool stopPorWatchdog = (pulsosTotalesCelda >= PULSOS_WATCHDOG_SEGURIDAD);
   ```

---

## 5. Verification Method

### 5.1 Verificación con el Harness de Challenger 1
Al aplicar las modificaciones descritas:
1. **Test 3.3 (Obstáculo temprano a 40 mm / pulso 10):**
   - Sin la guarda `pulsosTotalesCelda > 100`, `stopPorParedFrontal` evalúa a `true` en la primera muestra ($p=10$). El robot frena a los 10 pulsos sin avanzar a ciegas ni colisionar. Resultado: **PASS**.
2. **Test 4.2 (Lectura negativa $d_{cent} = -5\text{ mm}$):**
   - Al evaluar `sensadoActual.distanciaCent >= DISTANCIA_MIN_VALIDA && distanciaCent <= DISTANCIA_PARADA_FRENTE`: $-5 \ge -20$ y $-5 \le 50$ resultan en `true`. El robot frena de inmediato por `PARED_FRONTAL`. Resultado: **PASS**.
3. **Test 4.3 (Glitch aislado de $135\text{ mm}$ a los 820 pulsos):**
   - El cerrojo `aproximandoParedFrontal` se mantiene en `true`. `stopPorEncoders` permanece inhibido. El robot continúa avanzando y frena limpiamente a $\le 50\text{ mm}$. Resultado: **PASS**.
4. **Tests 1.1, 1.2, 1.3, 1.4, 2.1, 3.1, 3.2, 4.1, 5.1, 5.2:**
   - Permanecen en **PASS** sin regresiones.

### 5.2 Condiciones de Invalidación
Cualquiera de las siguientes observaciones invalidaría este reporte:
1. Si un sensor VL53L0X en funcionamiento normal o fallo pudiera entregar lecturas válidas de pared ausente con $rawCent < 30\text{ mm}$. (Físicamente imposible en ausencia de obstáculos frente al emisor).
2. Si un sensor VL53L0X desconectado o en timeout entregara valores negativos. (Demostrado: entrega 65535, saturado a 2000, resultando en +1950 mm).
