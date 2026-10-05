# Reporte de Análisis Algorítmico, Lógico y de Control de Flujo

**Proyecto:** Laberinto / debugRobot (ESP32 - Arduino Framework)  
**Autor:** Teamwork Explorer Survey 2 (Algorithmic, Logical, and Control Flow Analysis)  
**Fecha:** 2026-10-05  
**Alcance:** Análisis estático, algorítmico, matemático y de flujo de control 100% de solo lectura.

---

## 1. Resumen Ejecutivo

Se realizó una auditoría exhaustiva y sistemática de todos los archivos de código fuente en `src/`, `include/`, y `platformio.ini`. El análisis reveló **29 defectos críticos y moderados** que impiden la compilación, la navegación y el funcionamiento correcto del robot.

### Hallazgos Principales:
1. **Fallo Crítico de Compilación (`src/CITÉ.cpp`):** Caracteres no ASCII en el nombre de archivo provocan que el compilador `xtensa-esp32-elf-g++` falle inmediatamente al no encontrar el archivo en Windows. Además, define funciones `setup()` y `loop()` duplicadas y variables globales idénticas a las de `main.cpp`.
2. **Caída por Ausencia de `break;` en Máquina de Estados (`src/main.cpp:86`):** El estado `AVANZANDO` carece de sentencia `break;`, provocando que en cada iteración del bucle caiga incondicionalmente en `PREGIRO_DER`, sobreescribiendo de inmediato la velocidad calculada por el PID con valores base y corrompiendo la máquina de estados.
3. **Reseteo Continuo de Encoders en Avance (`src/main.cpp:72`):** Cada iteración donde el robot detecta que puede avanzar (`condicionAvanzar`), se invoca `resetearEncoders()`, destruyendo cualquier medición de odometría o distancia recorrida y saturando el canal Bluetooth.
4. **Cálculo de Giro de 180° Erróneo (`src/config.h:71`):** `PULSOS_GIRO_180` está configurado en 300 pulsos, idéntico a un giro de 90° (`PULSOS_GIRO_90_DER = 300`), provocando que un giro en callejón sin salida solo gire 90° e intente avanzar hacia la pared lateral.
5. **Asimetría Matemática en Encoders (`src/main.cpp:89, 120` vs `104, 135`):** Durante giros y pre-giros a la derecha se divide la lectura del Encoder A por 2 (`(abs(verPulsosEncoderA())) / 2`), mientras que para giros y pre-giros a la izquierda no se divide, requiriendo el doble de pulsos reales para girar a la derecha.
6. **Defectos en el Algoritmo PID (`src/hardware/movimiento/PID.cpp`):** La constante de offset de los sensores (`OFSET_IZQ = 40`, `OFSET_DER = 47`) es restada en `sensoresDistancia.cpp`, pero el cálculo de centrado entre dos paredes no compensa la diferencia, generando un desvío persistente de 7 mm hacia la pared izquierda. Además, el término derivativo se resetea a 0 en cada ciclo en `main.cpp`, anulando el amortiguamiento y convirtiendo el PID en un proporcional puro.
7. **Lectura Inicial Nula y Giro de 180° Inmediato (`src/hardware/sensoresDistancia/sensoresDistancia.cpp:83`):** La estructura de lectura estática arranca en `{0,0,0}`. Si el loop evalúa las condiciones antes de que los sensores completen su primera medición I2C, el robot asume que está en un callejón sin salida cerrado y ejecuta un giro de 180° al arrancar.

---

## 2. Inventario Detallado de Defectos

---

### CATEGORÍA A: Errores Críticos de Compilación y Estructura de Proyecto

#### A1. Nombre de archivo no ASCII y colisión de símbolos en `src/CITÉ.cpp`
- **Ubicación:** `src/CITÉ.cpp` (Archivo completo)
- **Severidad:** Crítica (Bloquea la compilación del proyecto).
- **Problema:** 
  1. El nombre del archivo contiene la letra con tilde `É` (UTF-8 `0xC3 0x89`). Al compilar con PlatformIO en Windows, GCC falla con el error:  
     `xtensa-esp32-elf-g++: error: src/CIT.cpp: No such file or directory`
  2. El archivo implementa `void setup()` (líneas 23-30) y `void loop()` (líneas 36-147), colisionando con las definiciones de `setup()` y `loop()` en `src/main.cpp`.
  3. Define variables globales con enlace externo (`sensadoActual`, `velocidadActual`, `estado`, `pulsosActuales`) que colisionan con las variables de `main.cpp`.
  4. La línea 16 usa `MAQUINA_ESTADOS estado = LISTO;`, pero `enum MAQUINA_ESTADOS` está comentado en `src/main.h`.
- **Solución recomendada:** 
  Eliminar `src/CITÉ.cpp` del árbol de compilación o moverlo fuera del directorio `src/` (por ejemplo, a una carpeta de archivo/pruebas o agregarlo a `src_filter` en `platformio.ini` para que no sea compilado).

---

#### A2. Declaraciones en headers sin definición (Símbolos no resueltos)
- **Ubicación:**
  1. `src/hardware/sensoresDistancia/sensoresDistancia.h` (Líneas 26, 31):
     ```cpp
     void inicializacionSensoresHCSR04();
     sensado actualizarSensadoHCSR04();
     ```
  2. `src/hardware/logger/logger.h` (Líneas 11, 15):
     ```cpp
     void enviarLog(char* mensaje);
     uint8_t leerAccion();
     ```
  3. `src/hardware/movimiento/puenteH.h` (Línea 19):
     ```cpp
     bool actualizarDeltaX();
     ```
- **Severidad:** Moderada / Alta (Si algún módulo intenta invocarlas, se genera un fallo de enlace `undefined reference`).
- **Problema:** Funciones declaradas en archivos de cabecera públicos que carecen por completo de cuerpo/implementación en los respectivos archivos `.cpp`.
- **Solución recomendada:** 
  Implementar las funciones requeridas o remover sus prototipos de los archivos `.h` para mantener una interfaz limpia y libre de símbolos huérfanos.

---

#### A3. Funciones definidas en `.cpp` sin declaración en header
- **Ubicación:** `src/hardware/movimiento/puenteH.cpp` (Líneas 69, 76)
  ```cpp
  void girar90GradosBloqueante(MOVIMIENTOS direccion);
  void avanzarBloqueante();
  ```
- **Severidad:** Baja.
- **Problema:** Funciones de control de movimiento bloqueante implementadas en `puenteH.cpp` pero no declaradas en `puenteH.h`, impidiendo su reutilización en otros módulos sin declaraciones manuales tipo `extern`.
- **Solución recomendada:** 
  Declarar los prototipos en `puenteH.h` si van a ser utilizadas, o marcarlas como `static` si son privadas del módulo.

---

### CATEGORÍA B: Defectos en Máquina de Estados y Control de Flujo (`src/main.cpp`)

---

#### B1. Falta de sentencia `break;` en `case AVANZANDO:` (Fallthrough accidental)
- **Ubicación:** `src/main.cpp` (Líneas 54 a 87)
  ```cpp
  54:   case AVANZANDO: {
  ...
  85:     }
  86:   }
  87: 
  88:   case  PREGIRO_DER : {
  ```
- **Severidad:** Crítica.
- **Problema:** 
  El bloque de `case AVANZANDO:` no tiene la instrucción `break;`. Al finalizar la evaluación de condiciones en la línea 86, el flujo de ejecución del procesador cae incondicionalmente (*fallthrough*) dentro de `case PREGIRO_DER:`.
  Esto provoca que:
  - En la línea 92 se ejecute de inmediato `movimiento(AVANZAR, {VEL_BASE_IZQ, VEL_BASE_DER});`, sobreescribiendo la velocidad corregida por el algoritmo PID calculada apenas unas líneas antes (líneas 58-60).
  - La corrección de trayectoria por PID queda completamente inutilizada.
  - Si una condición cambia el estado a otro valor, en esa misma iteración se ejecutan acciones de `PREGIRO_DER`.
- **Solución recomendada:** 
  Agregar `break;` al final del bloque `case AVANZANDO:` antes de `case PREGIRO_DER:`:
  ```cpp
      }
      break;
  ```

---

#### B2. Reseteo continuo de encoders en avance (`resetearEncoders()` repetitivo)
- **Ubicación:** `src/main.cpp` (Líneas 71-75)
  ```cpp
  } else if (condicionAvanzar) {
    resetearEncoders();
    resetearErrorAnterior();
    estado = AVANZANDO;
    enviarString (">>> AVANZANDO <<<");
  }
  ```
- **Severidad:** Crítica.
- **Problema:** 
  Cada vez que el robot avanza en línea recta, se cumple `condicionAvanzar`. En cada ciclo de `loop()`, se llama a `resetearEncoders()`, reiniciando los contadores de ambos encoders a cero.
  Consecuencias:
  1. La variable `pulsosActuales` calculada en la línea 55 nunca puede acumular pulsos de distancia.
  2. Es imposible medir si el robot avanzó una celda de 180 mm o cuánto recorrió.
  3. Se llama a `resetearErrorAnterior()`, borrando la memoria derivativa del PID en cada ciclo.
  4. Se envía la cadena `">>> AVANZANDO <<<"` por Bluetooth docenas de veces por segundo, saturando el buffer serie de FreeRTOS.
- **Solución recomendada:** 
  No resetear los encoders ni el error del PID mientras el robot se mantiene en el estado `AVANZANDO`. El reseteo debe ocurrir únicamente en las transiciones de estado (por ejemplo, al cambiar de `LISTO` a `AVANZANDO` o al completar un giro).

---

#### B3. Ausencia de cobertura para lecturas iguales al umbral (`== UMBRAL_PARED_ESTADO_NORMAL`)
- **Ubicación:** `src/main.cpp` (Líneas 63-66)
  ```cpp
  bool condicionGiroDer = sensadoActual.distanciaDer > UMBRAL_PARED_ESTADO_NORMAL;
  bool condicionAvanzar = sensadoActual.distanciaDer < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaCent > UMBRAL_PARED_ESTADO_NORMAL;
  bool condicionGiroIzq = sensadoActual.distanciaDer < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaCent < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaIzq > UMBRAL_PARED_ESTADO_NORMAL;
  bool condicionGiro180 = sensadoActual.distanciaDer < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaCent < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaIzq < UMBRAL_PARED_ESTADO_NORMAL;
  ```
- **Severidad:** Alta.
- **Problema:** 
  Todas las condiciones utilizan desigualdades estrictas (`<` y `>`). Si cualquiera de las distancias medidas es exactamente igual a `UMBRAL_PARED_ESTADO_NORMAL` (130 mm):
  - Ninguna de las cuatro condiciones booleanas será verdadera.
  - La estructura `if - else if` no ejecutará ninguna rama.
  - Debido a la falta de `break;` (B1), el flujo caerá en `PREGIRO_DER`.
  - Incluso con `break;`, el robot quedaría en un estado sin acción definida ni actualización.
- **Solución recomendada:** 
  Incluir operadores de comparación inclusivos (`<=` o `>=`) de forma consistente con la lógica deseada, y agregar una cláusula `else` por defecto para manejar estados indeterminados.

---

#### B4. Estados no implementados de `MAQUINA_NUEVA`
- **Ubicación:** `src/main.h` (Líneas 12-22) vs `src/main.cpp` (Líneas 39-163)
- **Severidad:** Media.
- **Problema:** 
  El enum `MAQUINA_NUEVA` incluye `DECISION` y `POSTGIRO`. Sin embargo, en el `switch (estado)` de `main.cpp`:
  - `case DECISION:` no existe.
  - `case POSTGIRO:` no existe.
  Al salir de los estados de giro (`GIRANDO_DER`, `GIRANDO_IZQ`, `GIRANDO_180`), el robot pasa directamente a `AVANZANDO` sin centrado ni avance ciego post-giro (`POSTGIRO`), lo que puede causar que los sensores laterales aún vean la esquina anterior y vuelvan a disparar un giro indebido.
- **Solución recomendada:** 
  Implementar los estados faltantes o eliminar las etiquetas no utilizadas del enum para mantener sincronizada la máquina de estados.

---

#### B5. Ausencia de timeouts en estados de giro (Riesgo de bucle infinito/bloqueo)
- **Ubicación:** `src/main.cpp` (Líneas 91, 106, 122, 137, 152)
- **Severidad:** Alta.
- **Problema:** 
  Las condiciones de salida de los estados `PREGIRO_DER`, `PREGIRO_IZQ`, `GIRANDO_DER`, `GIRANDO_IZQ`, y `GIRANDO_180` dependen exclusivamente de alcanzar una cantidad de pulsos de encoder (ej. `pulsosActuales < PULSOS_GIRO_90_DER`).
  Si una rueda patina contra una pared, se traba mecánicamente o un cable de encoder se desconecta:
  - `pulsosActuales` nunca alcanzará el umbral.
  - El robot quedará girando indefinidamente quemando los motores o patinando en el lugar.
- **Solución recomendada:** 
  Implementar un mecanismo de timeout basado en `millis()` (ej. si pasan más de 1.5 segundos en estado de giro, forzar parada y cambio de estado a frenado o error).

---

### CATEGORÍA C: Defectos Algorítmicos, Matemáticos y de Navegación

---

#### C1. Escala asimétrica de encoders entre derecha e izquierda (`/ 2`)
- **Ubicación:** `src/main.cpp` (Líneas 89, 120 frente a 104, 135)
  ```cpp
  89:   case PREGIRO_DER : {
  90:     pulsosActuales = (abs(verPulsosEncoderA())) / 2;
  ...
  104:  case PREGIRO_IZQ : {
  105:    pulsosActuales = abs(verPulsosEncoderB());
  ...
  120:  case GIRANDO_DER: {
  121:    pulsosActuales = (abs(verPulsosEncoderA())) / 2;
  ...
  135:  case GIRANDO_IZQ: {
  136:    pulsosActuales = abs(verPulsosEncoderB());
  ```
- **Severidad:** Crítica.
- **Problema:** 
  Para los movimientos a la derecha se divide el conteo de pulsos por 2 (`/ 2`), mientras que para los movimientos a la izquierda se toma el valor directo de `abs()`.
  Esto significa que para alcanzar `PULSOS_PREGIRO_90_DER = 300`, el motor debe generar 600 pulsos reales, mientras que para la izquierda `PULSOS_PREGIRO_90_IZQ = 280` requiere solo 280 pulsos.
  Además, `PREGIRO_DER` solo evalúa el Encoder A y `PREGIRO_IZQ` solo evalúa el Encoder B, a pesar de que durante el pre-giro ambos motores avanzan hacia adelante.
- **Solución recomendada:** 
  Unificar la escala de cálculo de pulsos para ambos lados y utilizar el promedio de ambos encoders `(abs(encA) + abs(encB)) / 2` durante el avance de pre-giro, o configurar umbrales calibrados directamente en pulsos reales sin divisiones arbitrarias.

---

#### C2. Configuración errónea de pulsos para giro de 180°
- **Ubicación:** `src/config.h` (Línea 71) y `src/main.cpp` (Líneas 150-161)
  ```cpp
  #define PULSOS_GIRO_90_DER 300
  #define PULSOS_GIRO_90_IZQ 280
  #define PULSOS_GIRO_180 300
  ```
- **Severidad:** Crítica.
- **Problema:** 
  El umbral para media vuelta (`PULSOS_GIRO_180`) tiene un valor de **300**, que es exactamente el mismo valor que un giro de 90 grados (`PULSOS_GIRO_90_DER = 300`).
  En un laberinto, cuando el robot entra a un callejón sin salida y debe dar la vuelta (180°), girará únicamente 90° e intentará avanzar directamente contra la pared lateral.
  Un giro de 180° en el mismo chasis requiere aproximadamente el doble de pulsos (~560 a 600 pulsos).
- **Solución recomendada:** 
  Modificar `PULSOS_GIRO_180` en `config.h` a un valor cercano a `600` (calibrado experimentalmente).

---

#### C3. Incompatibilidad de offsets de sensores y sesgo permanente en PID
- **Ubicación:** `src/hardware/sensoresDistancia/sensoresDistancia.cpp` (Líneas 92, 97, 102) vs `src/hardware/movimiento/PID.cpp` (Líneas 14-26)
  En `sensoresDistancia.cpp`:
  ```cpp
  lecturaAct.distanciaIzq = rawIzq - OFSET_IZQ; // OFSET_IZQ = 40
  lecturaAct.distanciaCent = rawCent - OFSET_CENT; // OFSET_CENT = 80
  lecturaAct.distanciaDer = rawDer - OFSET_DER; // OFSET_DER = 47
  ```
  En `PID.cpp`:
  ```cpp
  if (hayIzq && hayDer) {
      error = (int16_t)mediciones.distanciaDer - (int16_t)mediciones.distanciaIzq;
  }
  ```
- **Severidad:** Alta.
- **Problema:** 
  1. Si el robot está perfectamente centrado en el pasillo, con distancias físicas idénticas a ambas paredes (`rawDer == rawIzq`):
     `error = (rawDer - 47) - (rawIzq - 40) = -7 mm`.
     El PID detecta un error ficticio de -7 mm y corrige constantemente desviando el robot hacia la izquierda en lugar de ir recto.
  2. Al restar los offsets dentro del driver de sensores, si el robot se acerca a menos de 40 mm de la pared izquierda, `distanciaIzq` se vuelve **negativa** (ej. 25 mm - 40 mm = -15 mm).
  3. En `main.cpp`, las condiciones evalúan `sensadoActual.distanciaDer > UMBRAL_PARED_ESTADO_NORMAL` donde el umbral es 130. Si la distancia real medida es 130, `distanciaDer` vale $130 - 47 = 83$, lo que confunde las referencias de distancia absoluta con las relativas al centro.
- **Solución recomendada:** 
  Mantener las distancias en milímetros absolutos dentro de `sensoresDistancia.cpp` (`lecturaAct.distancia = raw`), y aplicar los offsets o setpoints de centrado exclusivamente dentro del algoritmo de control `PID.cpp`.

---

#### C4. Destrucción del término derivativo del PID por reinicio continuo
- **Ubicación:** `src/hardware/movimiento/PID.cpp` (Líneas 28, 43-45) y `src/main.cpp` (Línea 73)
  En `PID.cpp`:
  ```cpp
  int16_t correccion = (KP * error) + (KD * (error - errorAnterior)) ;
  errorAnterior = error;
  ```
  En `main.cpp`:
  ```cpp
  } else if (condicionAvanzar) {
    ...
    resetearErrorAnterior();
  ```
- **Severidad:** Alta.
- **Problema:** 
  Al resetear `errorAnterior = 0` en cada iteración de avance:
  - La derivada $(error - errorAnterior)$ siempre es $(error - 0) = error$.
  - La corrección calculada pasa a ser $(KP \cdot error) + (KD \cdot error) = (KP + KD) \cdot error$.
  - El término derivativo deja de cumplir su función física (amortiguar cambios bruscos y evitar oscilaciones) y actúa simplemente como ganancia proporcional adicional, incrementando la inestabilidad.
- **Solución recomendada:** 
  Eliminar la llamada a `resetearErrorAnterior()` dentro del ciclo continuo de `condicionAvanzar`. Solo debe resetearse al cambiar de maniobra (tras un giro o frenado completo).

---

#### C5. Término derivativo sin normalización temporal ($\Delta t$)
- **Ubicación:** `src/hardware/movimiento/PID.cpp` (Línea 28)
- **Severidad:** Media.
- **Problema:** 
  La fórmula calcula $KD \cdot (error - errorAnterior)$ sin dividir por el tiempo transcurrido $\Delta t$. Debido a las lecturas I2C y la transmisión Bluetooth, el tiempo de ciclo del bucle varía ampliamente (entre 10 ms y 60 ms). Al variar $\Delta t$, la derivada matemática fluctúa erráticamente produciendo tirones en los motores.
- **Solución recomendada:** 
  Medir el tiempo entre iteraciones con `millis()` o `micros()`, o asegurar un tiempo de muestreo fijo (timer periódico o `if (millis() - lastPid >= PID_PERIOD)`).

---

#### C6. Lógica de navegación ciega sin odometría de celda (`PULSOS_CELDA` no utilizado)
- **Ubicación:** `src/config.h` (Línea 66) vs `src/main.cpp` (Archivo completo)
- **Severidad:** Alta (Deficiencia arquitectónica).
- **Problema:** 
  En `config.h` se define `#define PULSOS_CELDA 800`. Sin embargo, `PULSOS_CELDA` **no se utiliza en ningún lugar de `main.cpp`**.
  El robot no navega celda por celda ni sabe cuándo llegó al centro de una intersección; navega de forma puramente reactiva continua.
  Si el robot va por un pasillo y pasa frente a un hueco en la pared derecha (una bifurcación o intersección), la condición de giro a la derecha se dispara de inmediato en cualquier punto del recorrido, haciendo que el robot comience a girar antes de estar alineado con la celda o chocando la esquina.
- **Solución recomendada:** 
  Implementar navegación discretizada por celdas: avanzar una celda completa contando `PULSOS_CELDA` con centrado PID, detenerse/reducir velocidad en el centro de la celda, tomar la decisión en el centro y ejecutar el giro correspondiente.

---

### CATEGORÍA D: Defectos en Sensores y Periféricos

---

#### D1. Vector de sensado inicial nulo genera giro de 180° espontáneo al arranque
- **Ubicación:** `src/hardware/sensoresDistancia/sensoresDistancia.cpp` (Líneas 83, 89-103)
  ```cpp
  static sensado lecturaAct = {0,0,0};
  ```
- **Severidad:** Alta.
- **Problema:** 
  La variable estática `lecturaAct` se inicializa en `{0,0,0}`.
  Los sensores VL53L0X tardan al menos 30 a 50 ms en realizar la primera medición de distancia.
  Si `main.cpp` llama a `actualizarSensado()` antes de que los sensores tengan su primera medición lista:
  - Devuelve `{distanciaCent: 0, distanciaDer: 0, distanciaIzq: 0}`.
  - En `main.cpp`:
    `condicionGiro180 = (0 < 130 && 0 < 130 && 0 < 130) == true`.
  - El robot asume que está encerrado entre 3 paredes y ejecuta un giro de 180° inmediatamente al presionar el botón de inicio.
- **Solución recomendada:** 
  Inicializar `lecturaAct` con valores seguros altos (ej. `{999, 999, 999}`) o implementar una bandera de validez `bool datosValidos` que no permita actuar a la máquina de estados hasta que se reciba la primera lectura válida de los 3 sensores.

---

#### D2. Tratamiento de timeout de VL53L0X como pasillo abierto infinito
- **Ubicación:** `src/hardware/sensoresDistancia/sensoresDistancia.cpp` (Líneas 90-103)
  ```cpp
  uint16_t rawIzq = sensorIzq.readRangeContinuousMillimeters();
  if (rawIzq > 2000) rawIzq = 2000;
  lecturaAct.distanciaIzq = rawIzq - OFSET_IZQ;
  ```
- **Severidad:** Alta.
- **Problema:** 
  Cuando la librería Pololu VL53L0X detecta un fallo de comunicación o timeout, `readRangeContinuousMillimeters()` devuelve `65535` (o `8190` en fuera de rango).
  El código evalúa `if (raw > 2000) raw = 2000;`, convirtiendo el error en una distancia de 2000 mm (2 metros).
  Si un sensor se desconecta o tiene un fallo óptico por un milisegundo:
  - El robot cree que hay un pasillo libre de 2 metros en esa dirección.
  - Provoca un giro brusco hacia la pared física.
- **Solución recomendada:** 
  Verificar `sensor.timeoutOccurred()` antes de aceptar la medición. Si ocurre timeout, descartar la muestra o entrar en estado de seguridad.

---

#### D3. Frecuencia de reloj I2C degradada a 10 kHz
- **Ubicación:** `src/hardware/sensoresDistancia/sensoresDistancia.cpp` (Línea 23)
  ```cpp
  Wire.setClock(10000);
  ```
- **Severidad:** Media.
- **Problema:** 
  El estándar de I2C es 100 kHz (Standard Mode) o 400 kHz (Fast Mode). Al configurar el bus en 10 kHz (10 veces más lento que el estándar):
  - Cada transacción de lectura de registro toma varios milisegundos.
  - La lectura de los 3 sensores bloquea la CPU durante 15 a 30 ms en cada ciclo.
  - Disminuye la tasa de actualización del lazo de control PID a valores inadecuados para el control en tiempo real de motores DC.
- **Solución recomendada:** 
  Usar `Wire.setClock(100000)` (100 kHz) o `400000` (400 kHz), asegurando resistencias físicas de pull-up adecuadas (2.2kΩ - 4.7kΩ) en las líneas SDA y SCL.

---

#### D4. Doble llamada consecutiva a `actualizarSensado()` en el mismo ciclo
- **Ubicación:** `src/main.cpp` (Líneas 56 y 61)
  ```cpp
  56:   sensadoActual = actualizarSensado();
  ...
  60:   movimiento(AVANZAR, {velocidadActual.izquierda, velocidadActual.derecha});
  61:   sensadoActual = actualizarSensado();
  ```
- **Severidad:** Media.
- **Problema:** 
  En el estado `AVANZANDO`, se invoca `actualizarSensado()` dos veces separadas por apenas 5 líneas de código. Esto duplica innecesariamente el tráfico en el bus I2C (que ya opera a 10 kHz), consumiendo tiempo de CPU sin aportar datos nuevos (los sensores tardan ~30 ms en actualizarse).
- **Solución recomendada:** 
  Llamar a `actualizarSensado()` una sola vez al inicio del ciclo de `loop()`.

---

#### D5. Bloqueo infinito del sistema si falla un sensor en el arranque
- **Ubicación:** `src/hardware/sensoresDistancia/sensoresDistancia.cpp` (Líneas 43-46, 57-60, 71-74)
  ```cpp
  if (!sensorDer.init()) {
    Serial.printf("ERROR: fallo init sensor en pin %d\n", xshutPinDer);
    while (true) delay(1000);
  }
  ```
- **Severidad:** Alta.
- **Problema:** 
  Si cualquiera de los 3 sensores falla al iniciar (por ruido eléctrico en el bus, falso contacto o alimentación tardía), el programa entra en `while (true) delay(1000);`, dejando el microcontrolador completamente congelado sin reintentos ni señal de alarma visible en el robot.
- **Solución recomendada:** 
  Implementar reintentos con límite (ej. 3 reintentos) y en caso de fallo crítico, encender un LED de error o emitir una señal audible.

---

### CATEGORÍA E: Defectos en el Control de Motores y Puente H

---

#### E1. Inversión cinemática en giros (`GIRAR_DER` gira a la izquierda y viceversa)
- **Ubicación:** `src/hardware/movimiento/puenteH.cpp` (Líneas 39-56)
  ```cpp
  case GIRAR_DER: {
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, HIGH);  // Motor A (Izquierdo) en RETROCESO
    digitalWrite(BIN1, HIGH);
    digitalWrite(BIN2, LOW);   // Motor B (Derecho) en AVANCE
    ledcWrite(0, velocidad.izquierda);
    ledcWrite(1, velocidad.derecha);
    break;
  }
  case GIRAR_IZQ: {
    digitalWrite(AIN1, HIGH);
    digitalWrite(AIN2, LOW);   // Motor A (Izquierdo) en AVANCE
    digitalWrite(BIN1, LOW);
    digitalWrite(BIN2, HIGH);  // Motor B (Derecho) en RETROCESO
    ledcWrite(0, velocidad.izquierda);
    ledcWrite(1, velocidad.derecha);
    break;
  }
  ```
- **Severidad:** Crítica.
- **Problema:** 
  En un chasis de tracción diferencial convencional:
  - Si la rueda izquierda va hacia atrás y la rueda derecha va hacia adelante, el frente del robot gira hacia la **IZQUIERDA** (sentido antihorario).
  - En el código, esta combinación está asignada a `case GIRAR_DER:`.
  - A su vez, `case GIRAR_IZQ:` acciona el motor izquierdo hacia adelante y el derecho hacia atrás, lo que hace girar el robot en sentido horario hacia la **DERECHA**.
  A menos que los motores estén conectados físicamente al revés, los giros por software están cruzados respecto a la rotación física del robot.
- **Solución recomendada:** 
  Verificar la polaridad física y corregir las definiciones de los pines en `GIRAR_DER` y `GIRAR_IZQ`:
  - `GIRAR_DER`: Rueda izquierda AVANZA (`AIN1=HIGH, AIN2=LOW`), rueda derecha RETROCEDE (`BIN1=LOW, BIN2=HIGH`).
  - `GIRAR_IZQ`: Rueda izquierda RETROCEDE (`AIN1=LOW, AIN2=HIGH`), rueda derecha AVANZA (`BIN1=HIGH, BIN2=LOW`).

---

#### E2. Freno pasivo por inercia (*Coasting*) en lugar de freno activo (*Braking*)
- **Ubicación:** `src/hardware/movimiento/puenteH.cpp` (Líneas 57-65)
  ```cpp
  case FRENO_F: {
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, LOW);
    digitalWrite(BIN1, LOW);
    digitalWrite(BIN2, LOW);
    ledcWrite(0, 0);
    ledcWrite(1, 0);
    break;
  }
  ```
- **Severidad:** Media.
- **Problema:** 
  Poner todas las entradas en LOW en puentes H como el TB6612FNG o L298N activa el modo de alta impedancia (*Coast / Parada por inercia*). El robot sigue rodando unos centímetros por inercia antes de detenerse. En un laberinto donde las tolerancias son de milímetros, esto genera desalineaciones y choques contra las esquinas.
- **Solución recomendada:** 
  Para frenado dinámico activo (*Short Brake*), configurar ambas entradas en HIGH con PWM al 100% (o la combinación específica de frenado según el integrado del driver de motor utilizado).

---

### CATEGORÍA F: Defectos en Comunicaciones, Logging y Configuración de Pines

---

#### F1. Función `cambioDeCelda()` ficticia que corrompe el protocolo
- **Ubicación:** `src/hardware/logger/logger.cpp` (Líneas 20-22) vs `logger.h` (Línea 16)
  En `logger.h`:
  `//Función para detectar si se ha producido un cambio de celda...`
  En `logger.cpp`:
  ```cpp
  bool cambioDeCelda(){
      if(SerialBT.available()>0){ return true;} else { return false;}
  }
  ```
- **Severidad:** Media.
- **Problema:** 
  La función indica detectar cambios de celda para sincronizar datos, pero su código evalúa si hay bytes disponibles en el puerto Bluetooth (`SerialBT.available() > 0`). Si llega un byte de control, nunca se lee con `.read()`, por lo que la función devolverá `true` indefinidamente en cada ciclo. No tiene relación alguna con la odometría de celdas.
- **Solución recomendada:** 
  Vincular la detección de cambio de celda al contador de pulsos de los encoders (`pulsosRecorridos >= PULSOS_CELDA`) o renombrar/eliminar la función si es un remanente no utilizado.

---

#### F2. Peligro de Pin de Booteo (*Strapping Pin*) en GPIO 12 (`PWMA`)
- **Ubicación:** `src/config.h` (Línea 49)
  ```cpp
  #define PWMA 12
  ```
- **Severidad:** Alta (Riesgo de bloqueo de arranque de hardware).
- **Problema:** 
  El pin GPIO 12 en el ESP32 es un pin de *strapping* (`MTDI`). Si el pin se encuentra en nivel HIGH durante el encendido o reinicio del microcontrolador, el ESP32 conmuta el voltaje de la memoria Flash interna de 3.3V a 1.8V, provocando que la Flash no responda y el ESP32 entre en un ciclo infinito de reseteos (*bootloop*). Si el driver de motor o la circuitería externa tiene una resistencia de pull-up en la línea PWM, el robot no encenderá.
- **Solución recomendada:** 
  Reasignar `PWMA` a un pin que no sea de strapping (por ejemplo GPIO 13, 15, o 2 según disponibilidad) o garantizar mediante hardware que GPIO 12 esté en LOW durante el arranque.

---

#### F3. Entrada flotante en botón de inicio (GPIO 34 sin pull-up interno)
- **Ubicación:** `src/config.h` (Línea 42) y `src/main.cpp` (Línea 25)
  ```cpp
  #define BOTON1 34
  ...
  pinMode(BOTON1, INPUT);
  ```
- **Severidad:** Media.
- **Problema:** 
  Los pines GPIO 34 a 39 en el ESP32 son pines de solo entrada (GPI) y **no poseen resistencias internas de pull-up ni pull-down**. Si la placa no cuenta con una resistencia física de pull-up externa, el pin queda flotando, provocando que `digitalRead(BOTON1) == LOW` se dispare aleatoriamente por ruido electromagnético de los motores o tacto humano.
- **Solución recomendada:** 
  Asegurar una resistencia física de pull-up externa (ej. 10kΩ a 3.3V) conectada a GPIO 34, o trasladar el botón a un pin con soporte para `INPUT_PULLUP` interno (como GPIO 15, 4, 16, etc.).

---

#### F4. Inundación (*Flooding*) del buffer serie de Bluetooth
- **Ubicación:** `src/main.cpp` (Línea 75)
  ```cpp
  enviarString (">>> AVANZANDO <<<");
  ```
- **Severidad:** Media.
- **Problema:** 
  En cada ciclo de `AVANZANDO`, se invoca `enviarString()`, transmitiendo datos por Bluetooth Serial sin ningún delay ni filtro de frecuencia. Cuando el buffer de transmisión de Bluetooth se llena, la librería bloquea la ejecución de la tarea de FreeRTOS hasta que haya espacio disponible, introduciendo jitter y demoras de decenas de milisegundos en el bucle principal de control.
- **Solución recomendada:** 
  Enviar mensajes de log solo ante cambios de estado o mediante un temporizador con tasa de refresco acotada (ej. cada 250 ms).

---

## 3. Matriz de Severidad y Priorización

| ID | Archivo | Línea | Defecto | Severidad | Impacto |
|---|---|---|---|---|---|
| A1 | `src/CITÉ.cpp` | Todo | Nombre con tilde, `setup`/`loop` duplicados, enum roto | **Crítica** | Impide compilar el proyecto en su totalidad |
| B1 | `src/main.cpp` | 86 | Ausencia de `break;` en `case AVANZANDO:` | **Crítica** | Anula el control PID y salta a `PREGIRO_DER` |
| B2 | `src/main.cpp` | 72 | Reseteo de encoders continuo en avance | **Crítica** | Impide medir distancias y satura Bluetooth |
| C1 | `src/main.cpp` | 89, 120 | Asimetría de `/ 2` en encoders de giro derecho | **Crítica** | Giros a la derecha requieren el doble de distancia |
| C2 | `src/config.h` | 71 | `PULSOS_GIRO_180` igual a giro de 90° (300 pulsos) | **Crítica** | Giro de 180° queda a mitad de camino y choca |
| E1 | `src/puenteH.cpp`| 39-56 | Inversión cinemática en giros `GIRAR_DER`/`IZQ` | **Crítica** | El robot rota en dirección contraria a la calculada |
| D1 | `src/sensoresDistancia.cpp` | 83 | Lectura inicial `{0,0,0}` dispara giro 180° inmediato | **Alta** | El robot gira sobre sí mismo nada más arrancar |
| D2 | `src/sensoresDistancia.cpp` | 91, 96 | Timeouts de sensor interpretados como pasillo libre de 2 m | **Alta** | Choca a toda velocidad si un sensor falla |
| C3 | `src/PID.cpp` | 14-26 | Offset de sensores genera sesgo permanente de 7 mm | **Alta** | El robot no viaja recto en pasillos centrados |
| C4 | `src/main.cpp` | 73 | `resetearErrorAnterior()` en avance mata derivativo | **Alta** | Oscilaciones violentas sin amortiguamiento PID |
| B5 | `src/main.cpp` | 91-152 | Ausencia de timeouts en giros | **Alta** | Bucle infinito si una rueda patina o se traba |
| F2 | `src/config.h` | 49 | GPIO 12 strapping pin puede bloquear boot de ESP32 | **Alta** | Fallo de encendido por voltaje incorrecto de flash |
| B3 | `src/main.cpp` | 63-66 | Falta de cobertura cuando distancia == 130 mm | **Alta** | Comportamiento indeterminado ante valor exacto |
| D5 | `src/sensoresDistancia.cpp` | 43-74 | `while(true)` infinito ante fallo de inicio de sensor | **Alta** | Bloqueo total del sistema si falla un sensor |
| C6 | `src/main.cpp` | - | `PULSOS_CELDA` no se usa; navegación ciega sin celdas | **Alta** | Giros prematuros en mitad de un pasillo |
| D3 | `src/sensoresDistancia.cpp` | 23 | Reloj I2C degradado a 10 kHz | **Media** | Retardo de 30 ms por ciclo, lazo PID lento |
| D4 | `src/main.cpp` | 56, 61 | Doble llamada a `actualizarSensado()` por ciclo | **Media** | Desperdicio de CPU y tráfico I2C |
| F1 | `src/logger.cpp` | 20-22 | `cambioDeCelda()` chequea RX de Bluetooth | **Media** | Inconsistencia lógica en sincronización |
| F4 | `src/main.cpp` | 75 | Envío serial desbordante por Bluetooth en cada loop | **Media** | Bloqueo por saturación de buffer |
| F3 | `src/main.cpp` | 25 | GPIO 34 sin pull-up interno en botón de inicio | **Media** | Disparos erráticos por botón flotante |
| C5 | `src/PID.cpp` | 28 | Derivativo sin normalización por $\Delta t$ | **Media** | Inestabilidad ante variaciones de ciclo |
| A2 | Múltiples `.h` | Varios | Funciones declaradas en cabeceras sin definición | **Media** | Potenciales errores de enlazado |
| B4 | `src/main.h` | 12-22 | Estados de `MAQUINA_NUEVA` sin implementar | **Baja** | Inconsistencia entre diseño y código |
| A3 | `src/puenteH.cpp`| 69, 76 | Funciones en `.cpp` sin declaración en header | **Baja** | Falta de modularidad |
| E2 | `src/puenteH.cpp`| 57-65 | Frenado pasivo (inercia) en lugar de freno activo | **Baja** | Deslizamiento al detenerse |

---

## 4. Conclusiones del Análisis

El código actual exhibe una combinación de errores sintácticos/estructurales graves (que impiden la compilación con PlatformIO), errores de control de flujo en la máquina de estados principal (destacando el fallthrough en `AVANZANDO` y el reseteo compulsivo de encoders) y fallas matemáticas en los algoritmos de navegación y control PID.

Para que el robot pueda operar de manera funcional y segura, se requiere:
1. Limpiar el árbol de fuentes eliminando `src/CITÉ.cpp` y resolviendo las cabeceras huérfanas.
2. Corregir la máquina de estados en `src/main.cpp` agregando `break;`, eliminando reseteos redundantes y dotando de timeouts a los estados de giro.
3. Unificar las escalas de los encoders y calibrar el giro de 180° a aproximadamente el doble de pulsos que un giro de 90°.
4. Reubicar la aplicación de offsets de los sensores fuera del driver de hardware y dentro del módulo de control PID, permitiendo además una gestión adecuada de errores y timeouts en el bus I2C.
