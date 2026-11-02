# Reporte Maestro de Auditoría de Código y Diagnóstico de Firmware
## Proyecto: debugRobot — Robot Autónomo de Laberinto (MicroMouse)

**Plataforma de Hardware:** Espressif ESP32 DevKit V1 (Xtensa Dual-Core 32-bit LX6 @ 240 MHz)  
**Framework de Desarrollo:** Arduino Core for ESP32 / PlatformIO  
**Fecha de Emisión:** 2026-10-05  
**Modo de Auditoría:** 100% Read-Only (Análisis Estático, Algorítmico, Cinemático y de Hardware)  
**Estado General del Firmware:** **NO APTO PARA PRODUCCIÓN / BLOQUEO CRÍTICO DE COMPILACIÓN Y NAVEGACIÓN**

---

## 1. Resumen Ejecutivo

Durante la auditoría técnica exhaustiva del repositorio de firmware `debugRobot`, se analizaron sistemáticamente los subsistemas de compilación, control de flujo (FSM), algoritmos de odometría y centrado (PID), sensado Time-of-Flight (I2C), cinemática de motores (Puente H), gestión de memoria, telemetría Bluetooth y asignación de pines a nivel de silicio del ESP32.

### Diagnóstico Global
El proyecto presenta **34 defectos principales clasificados entre Críticos, Altos, Medios y Bajos**, acompañados de deficiencias graves de higiene de repositorio. En su estado actual:
1. **El código no compila:** Un archivo fuente en `src/` contiene caracteres no-ASCII en su nombre (`src/CITÉ.cpp`), lo que provoca la terminación abrupta del compilador `xtensa-esp32-elf-g++` en entornos Windows, sumado a colisiones de símbolos globales y tipos no declarados.
2. **La máquina de estados colapsa y omite umbrales:** En `src/main.cpp`, el estado `AVANZANDO` carece de sentencia `break;`, produciendo un *fallthrough* inmediato hacia `PREGIRO_DER:`. Asimismo, `main.cpp` ignora la macro `UMBRAL_PARED_FRENTE` (120 mm), sustituyéndola erróneamente por 130 mm.
3. **El centrado PID colisiona contra las paredes y discrepa con la FSM:** La ley de control proporcional-derivativa para el seguimiento de pared única omite por completo la consigna (*setpoint*), aplicando correcciones invertidas. Además, existe una discrepancia espacial de 49 mm entre los criterios de pared de la FSM (130 mm) y el PID (180 mm), induciendo correcciones violentas hacia aperturas y cruces.
4. **La odometría es físicamente asimétrica e inconsistente:** Los pulsos del encoder derecho se dividen arbitrariamente por 2 (`/ 2`) mientras que los del izquierdo no, duplicando la rotación real en giros a la derecha; a su vez, el giro de 180° está configurado con los mismos pulsos que un giro de 90° (`300` pulsos).
5. **Riesgos eléctricos, de arranque y saturación de memoria:** Se utiliza el GPIO 12 (pin de strapping `MTDI`) para la señal PWM de un motor (riesgo de flash a 1.8V / *bootloop*), y el pulsador está en GPIO 34 (sin pull-up interno). Los motores no se inicializan a nivel bajo ni con PWM cero en `inicializarMotores()`, y `puenteH.cpp` carece de cláusula `default:`. Además, el firmware satura la partición de flash de aplicación `app0` al 86.7% (1.13 MB de 1.25 MB) debido a BluetoothSerial, y la consola serie USB es totalmente muda al no emitir telemetría por UART.

---

## 2. Matriz General de Severidad y Clasificación

| ID | Severidad | Categoría | Ubicación Principal | Resumen del Defecto |
|---|---|---|---|---|
| **BUG-01** | **CRÍTICA** | Build / Toolchain | `src/CITÉ.cpp` (Nombre) | Carácter no-ASCII `É` aborta la compilación de GCC en Windows. |
| **BUG-02** | **CRÍTICA** | Build / Linker | `src/CITÉ.cpp:14-36` | Símbolos globales duplicados (`setup`, `loop`, `sensadoActual`, etc.). |
| **BUG-03** | **CRÍTICA** | Build / Sintaxis | `src/CITÉ.cpp:16` | Declaración con tipo enum eliminado/comentado `MAQUINA_ESTADOS`. |
| **BUG-04** | **ALTA** | Build System | `platformio.ini:11-19` | Ausencia de `build_src_filter` compila archivos temporales de `src/`. |
| **BUG-05** | **ALTA** | Arquitectura / Headers | `logger.h`, `sensoresDistancia.h`, `puenteH.h` | Declaraciones de funciones públicas huérfanas sin implementación en `.cpp`. |
| **BUG-06** | **BAJA** | Modularidad | `puenteH.cpp:69-80` | Funciones bloqueantes en `.cpp` no expuestas en `puenteH.h`. |
| **BUG-07** | **CRÍTICA** | FSM / Control de Flujo | `src/main.cpp:54-88` | Ausencia de `break;` en `case AVANZANDO:` causa *fallthrough* a `PREGIRO_DER:`. |
| **BUG-08** | **CRÍTICA** | Odometría / FSM | `src/main.cpp:71-76` | Reseteo continuo de encoders en avance destruye la medición métrica. |
| **BUG-09** | **ALTA** | PID / Memoria de Error | `src/main.cpp:73` | Reseteo repetitivo de `errorAnterior` en avance anula el término derivativo $K_d$. |
| **BUG-10** | **ALTA** | Casos Borde / FSM | `src/main.cpp:63-66` | Desigualdades estrictas (`<` y `>`) crean zona muerta a 130 mm exactos. |
| **BUG-11** | **MEDIA** | Consistencia FSM | `src/main.h:12-22` vs `main.cpp` | Estados `DECISION` y `POSTGIRO` declarados en enum pero no implementados. |
| **BUG-12** | **ALTA** | Seguridad / Bloqueo | `src/main.cpp:91, 106, 122, 137, 152` | Ausencia de timeouts en giros genera bucle infinito si una rueda patina. |
| **BUG-13** | **CRÍTICA** | Cinemática / Odometría | `src/main.cpp:89, 120` | División espuria `/ 2` en encoder A duplica la rotación a la derecha. |
| **BUG-14** | **CRÍTICA** | Cinemática / Navegación | `src/config.h:71`, `main.cpp:152` | `PULSOS_GIRO_180` igual a giro de 90° (`300`), chocando en callejones. |
| **BUG-15** | **ALTA** | Odometría de Celda | `src/config.h:66` vs `main.cpp` | `PULSOS_CELDA` no se utiliza; navegación ciega sin sincronismo espacial. |
| **BUG-16** | **CRÍTICA** | Algoritmo PID | `src/hardware/movimiento/PID.cpp:17-23` | Falta sustracción de consigna (*setpoint*) en seguimiento de pared única. |
| **BUG-17** | **ALTA** | Calibración / PID | `sensoresDistancia.cpp:92, 102`, `PID.cpp:16` | Asimetría de offsets (`40` vs `47`) induce desvío persistente de 7 mm. |
| **BUG-18** | **MEDIA** | Algoritmo PID | `src/hardware/movimiento/PID.cpp:28` | Término derivativo sin normalización temporal ($\Delta t$) sensible al jitter. |
| **BUG-19** | **CRÍTICA** | Motores / Cinemática | `src/hardware/movimiento/puenteH.cpp:39-56` | Polaridad invertida en giros: `GIRAR_DER` rota a la izquierda y viceversa. |
| **BUG-20** | **MEDIA** | Dinámica / Motores | `src/main.cpp:92, 107, 123, 138, 153` | Giros comandados a `VEL_BASE` (45) en lugar de `VEL_GIRO` (100) causan calado. |
| **BUG-21** | **MEDIA** | Control de Puente H | `src/hardware/movimiento/puenteH.cpp:57-65` | `FRENO_F` aplica modo rueda libre (*Coast*) en lugar de freno dinámico activo. |
| **BUG-22** | **MEDIA** | Seguridad de Tipos | `src/hardware/movimiento/puenteH.cpp:25, 35` | Valores negativos de `int16_t` desbordan en `ledcWrite(uint32_t)`. |
| **BUG-23** | **ALTA** | Inicialización Sensores | `sensoresDistancia.cpp:83` | Sensado inicial `{0,0,0}` dispara giro falso de 180° inmediato al arranque. |
| **BUG-24** | **ALTA** | Tolerancia a Fallos | `sensoresDistancia.cpp:45, 59, 73` | Bucle infinito bloqueante `while(true) delay(1000);` congela el microcontrolador. |
| **BUG-25** | **MEDIA** | Rendimiento I2C | `sensoresDistancia.cpp:23` | Frecuencia de reloj I2C degradada a 10 kHz impone latencias de 30 ms por ciclo. |
| **BUG-26** | **ALTA** | Integridad de Sensores | `sensoresDistancia.cpp:90-103` | Timeouts (65535) enmascarados como 1960 mm y distancias negativas por underflow. |
| **BUG-27** | **MEDIA** | Optimización I2C | `src/main.cpp:56, 61`, `sensoresDistancia.cpp:88` | Doble lectura consecutiva de sensores y rate-limiter de 20 ms comentado. |
| **BUG-28** | **ALTA** | Hardware / Strapping | `src/config.h:49` | GPIO 12 (`PWMA`) es pin strapping MTDI; riesgo de bootloop por flash a 1.8V. |
| **BUG-29** | **ALTA** | Hardware / Señales | `src/config.h:42`, `src/main.cpp:25` | GPIO 34 (`BOTON1`) sin pull-up interno en silicio flota y provoca arranques falsos. |
| **BUG-30** | **ALTA** | FSM / Navegación | `src/config.h:37` frente a `src/main.cpp:64-66` | Omisión de `UMBRAL_PARED_FRENTE` (120 mm); sustituido erróneamente por `UMBRAL_PARED_ESTADO_NORMAL` (130 mm). |
| **BUG-31** | **ALTA** | Consistencia FSM / PID | `src/main.cpp:63` frente a `src/hardware/movimiento/PID.cpp:8-10` | Discrepancia espacial de 49 mm entre FSM (130 mm) y PID (180 mm); genera correcciones erráticas en pasillos contiguos. |
| **BUG-32** | **ALTA** | Build / Recursos Flash | `platformio.ini:11-19` | Saturación de memoria flash (86.7% en `app0` por BluetoothSerial); requiere partición `huge_app.csv` para soportar FloodFill. |
| **BUG-33** | **MEDIA** | Hardware / Puente H | `src/hardware/movimiento/puenteH.cpp:6-16, 18-67` | Ausencia de `default:` en `switch (movimiento)` y omisión de puesta a cero de pines/PWM en `inicializarMotores()`. |
| **BUG-34** | **MEDIA** | Observabilidad / Telemetría | `src/main.cpp` frente a `src/hardware/logger/logger.cpp:11-18` | Inobservabilidad total de telemetría por UART en `main.cpp` a pesar de `Serial.begin(115200)`. |

---

## 3. Catálogo Detallado de Hallazgos

---

### CATEGORÍA 1: Compilación, Enlazado y Build System

#### BUG-01: Nombre de Archivo no-ASCII que Bloquea la Compilación
- **Ubicación:** `src/CITÉ.cpp` (Nombre de archivo)
- **Problema:** El carácter `É` (UTF-8 `0xC3 0x89`) en el nombre del archivo `CITÉ.cpp` no es interpretado adecuadamente por el compilador cruzado `xtensa-esp32-elf-g++` bajo Windows. PlatformIO transfiere la ruta decodificada de manera inconsistente a la línea de comandos de GCC, resultando en el error fatal:
  ```text
  xtensa-esp32-elf-g++: error: src/CIT.cpp: No such file or directory
  xtensa-esp32-elf-g++: fatal error: no input files
  compilation terminated.
  *** [.pio\build\esp32doit-devkit-v1\src\CIT.cpp.o] Error 1
  ```
  Esto bloquea por completo la construcción de cualquier binario ejecutable para el robot.
- **Solución recomendada:** 
  1. Eliminar de forma definitiva `src/CITÉ.cpp` del árbol de fuentes activas o moverlo a un directorio fuera de la compilación (por ejemplo, `test/` o `archive/`).
  2. Adoptar como norma de desarrollo el uso estricto de caracteres ASCII estándar (alfanuméricos y guión bajo) en todas las rutas y archivos de código fuente.

---

#### BUG-02: Colisión de Símbolos Globales entre Módulos de Entrada
- **Ubicación:** `src/CITÉ.cpp` (Líneas 14–36) frente a `src/main.cpp` (Líneas 15–37)
- **Problema:** `src/CITÉ.cpp` es una copia arcaica o archivo de prueba de `main.cpp` que implementa sus propias versiones globales de:
  - `void setup()`
  - `void loop()`
  - `sensado sensadoActual`
  - `VELOCIDAD velocidadActual`
  - `uint32_t pulsosActuales`
  Si se solucionara el problema de codificación del nombre (BUG-01), el enlazador GNU (`ld`) fallará de inmediato arrojando errores de enlace múltiple (`multiple definition of 'setup'`, `multiple definition of 'loop'`).
- **Solución recomendada:** 
  Retirar `src/CITÉ.cpp` del directorio `src/`. Un proyecto con framework Arduino debe contener exactamente una única función `setup()` y una única función `loop()` en todo el espacio de compilación.

---

#### BUG-03: Referencia a Tipo de Dato Inexistente (`MAQUINA_ESTADOS`)
- **Ubicación:** `src/CITÉ.cpp` (Línea 16) frente a `src/main.h` (Líneas 2–10)
- **Problema:** En `src/CITÉ.cpp`:
  ```cpp
  MAQUINA_ESTADOS estado = LISTO;
  ```
  Sin embargo, en `src/main.h`, la definición de `enum MAQUINA_ESTADOS` fue comentada por los desarrolladores y sustituida por `enum MAQUINA_NUEVA`. Esto genera un error directo de compilación de C++: `'MAQUINA_ESTADOS' was not declared in this scope`.
- **Solución recomendada:** 
  Al eliminar o excluir `src/CITÉ.cpp` de la compilación, este fallo se resuelve. En caso de mantener código legado, debe actualizarse la referencia de tipos a `MAQUINA_NUEVA`.

---

#### BUG-04: Ausencia de Filtro de Fuentes en la Configuración de PlatformIO
- **Ubicación:** `platformio.ini` (Líneas 11–19)
- **Problema:** La configuración actual de `platformio.ini` no declara la directiva `build_src_filter`. Por defecto, PlatformIO compila todos los archivos con extensiones `.c`, `.cpp` y `.S` que residan bajo la carpeta `src/`. Cualquier archivo de prueba, borrador temporal o script que un desarrollador guarde con extensión C++ en `src/` ingresa automáticamente al proceso de compilación, comprometiendo la estabilidad de la integración.
- **Solución recomendada:** 
  Configurar explícitamente `build_src_filter` en `platformio.ini` para incluir únicamente los módulos activos aprobados y excluir archivos transitorios:
  ```ini
  [env:esp32doit-devkit-v1]
  platform = espressif32
  board = esp32doit-devkit-v1
  framework = arduino
  build_src_filter = +<*> -<CIT*.cpp> -<*.txt>
  lib_deps =
      VL53L0X
      madhephaestus/ESP32Encoder @ ^0.11.7
  monitor_speed = 115200
  ```

---

### CATEGORÍA 2: Interfaces, Modularidad y Cabeceras

#### BUG-05: Declaraciones Públicas en Archivos `.h` sin Definición en `.cpp`
- **Ubicación:**
  - `src/hardware/logger/logger.h` (Líneas 11, 15): `void enviarLog(char* mensaje);`, `uint8_t leerAccion();`
  - `src/hardware/sensoresDistancia/sensoresDistancia.h` (Líneas 26, 31): `void inicializacionSensoresHCSR04();`, `sensado actualizarSensadoHCSR04();`
  - `src/hardware/movimiento/puenteH.h` (Línea 19): `bool actualizarDeltaX();`
- **Problema:** Múltiples funciones están expuestas formalmente en los contratos de interfaz pública de los módulos, pero no poseen ningún cuerpo de función implementado en sus correspondientes archivos `.cpp`. Si cualquier otro archivo del proyecto intenta invocar alguna de estas funciones (por ejemplo, para telemetría o telecontrol), el enlazador abortará con fallos de tipo `undefined reference to 'enviarLog(char*)'`.
- **Solución recomendada:** 
  1. Si las funciones corresponden a funcionalidades planificadas para sensores ultrasónicos (HCSR04) o control de posición que fueron descartadas, eliminar los prototipos de los archivos `.h`.
  2. Si se van a utilizar, implementar sus definiciones correspondientes en sus archivos `.cpp`.

---

#### BUG-06: Funciones de Control Bloqueante Implementadas sin Declaración en Header
- **Ubicación:** `src/hardware/movimiento/puenteH.cpp` (Líneas 69–80) frente a `src/hardware/movimiento/puenteH.h`
- **Problema:** Las funciones `girar90GradosBloqueante(MOVIMIENTOS direccion)` y `avanzarBloqueante()` están implementadas en `puenteH.cpp`, pero no están declaradas en `puenteH.h`. Otros módulos (o futuros algoritmos de navegación y prueba) no pueden invocarlas sin emitir advertencias de funciones implícitas o requerir declaraciones `extern` manuales.
- **Solución recomendada:** 
  Si estas funciones forman parte de la API de movimiento, agregar sus prototipos en `puenteH.h`:
  ```cpp
  void girar90GradosBloqueante(MOVIMIENTOS direccion);
  void avanzarBloqueante();
  ```
  Si son funciones de uso puramente interno para pruebas dentro de `puenteH.cpp`, declararlas como `static`.

---

### CATEGORÍA 3: Máquina de Estados y Control de Flujo (FSM)

#### BUG-07: Caída Accidental (*Fallthrough*) por Falta de `break;` en `case AVANZANDO:`
- **Ubicación:** `src/main.cpp` (Líneas 54–88)
- **Problema:** En la estructura `switch (estado)` de `main.cpp`, el bloque `case AVANZANDO:` culmina en la línea 86 sin una sentencia `break;`. 
  ```cpp
  54:     case AVANZANDO: {
  ...
  84:         estado = GIRANDO_180;
  85:       }
  86:     }
  87: 
  88:     case  PREGIRO_DER : {
  ```
  **Consecuencias:**
  1. En **cada iteración del bucle principal** en la que el robot se encuentra avanzando, tras calcular y enviar la velocidad corregida por el PID (línea 60), el flujo de ejecución continúa inmediatamente dentro de `case PREGIRO_DER:`.
  2. En la línea 92, dentro de `PREGIRO_DER:`, el código ejecuta:
     ```cpp
     movimiento(AVANZAR, {VEL_BASE_IZQ, VEL_BASE_DER});
     ```
     sobrescribiendo de forma instantánea la velocidad con corrección PID calculada instantes antes, neutralizando el control direccional.
  3. Peor aún: en cuanto el acumulador de pulsos `pulsosActuales` supera `PULSOS_PREGIRO_90_DER` (300 pulsos), la condición `else` de `PREGIRO_DER` se ejecuta (líneas 93–99), cambiando el estado a `GIRANDO_DER`, frenando los motores y forzando al robot a girar a la derecha en plena marcha recta sin que exista un hueco real en el laberinto.
- **Solución recomendada:** 
  Incorporar `break;` al finalizar el ámbito del bloque `case AVANZANDO:`:
  ```cpp
        } else if (condicionGiro180) {
          enviarString(">>> GIRANDO 180 <<<");
          resetearEncoders();
          estado = GIRANDO_180;
        }
        break; // PREVIENE FALLTHROUGH CRÍTICO
      }

      case PREGIRO_DER: {
  ```

---

#### BUG-08: Reseteo Incondicional de Encoders en Cada Ciclo de Avance
- **Ubicación:** `src/main.cpp` (Líneas 71–76)
- **Problema:** Durante el avance en línea recta, se cumple continuamente `condicionAvanzar`. En cada paso de `loop()`, el código ejecuta:
  ```cpp
  } else if (condicionAvanzar) {
    resetearEncoders();
    resetearErrorAnterior();
    estado = AVANZANDO;
    enviarString (">>> AVANZANDO <<<");
  }
  ```
  Al invocar `resetearEncoders()` decenas de veces por segundo:
  1. La variable `pulsosActuales` (línea 55) siempre lee valores cercanos a 0 o 1 pulso, destruyendo la odometría de distancia recorrida.
  2. Es matemáticamente imposible saber cuándo el robot ha avanzado una celda completa (180 mm).
  3. Se reinicia el término de error del PID continuamente (BUG-09).
  4. Se inunda el puerto BluetoothSerial con mensajes en texto plano a máxima velocidad de ciclo (BUG-14 en telecomunicaciones).
- **Solución recomendada:** 
  No resetear encoders mientras se permanezca en el estado `AVANZANDO`. El reseteo debe producirse exclusivamente en las **transiciones de estado** (por ejemplo, al completar un giro o al salir del estado de reposo `LISTO`):
  ```cpp
  } else if (condicionAvanzar) {
    // Mantener estado sin resetear contadores ni emitir logs continuos
    estado = AVANZANDO;
  }
  ```

---

#### BUG-09: Anulación del Término Derivativo del PID por Blanqueo de `errorAnterior`
- **Ubicación:** `src/main.cpp` (Línea 73) frente a `src/hardware/movimiento/PID.cpp` (Líneas 28, 43–45)
- **Problema:** Al ejecutarse `resetearErrorAnterior()` en cada ciclo de avance continuo (asociado a `condicionAvanzar`), la variable estática `errorAnterior` en `PID.cpp` se fuerza a 0.
  En la ecuación del PID:
  $$\text{correccion} = K_p \cdot e + K_d \cdot (e - e_{\text{ant}})$$
  Si $e_{\text{ant}} = 0$, la ecuación degenera en:
  $$\text{correccion} = (K_p + K_d) \cdot e$$
  El término derivativo, cuyo propósito físico es amortiguar la oscilación y frenar el sobrepaso ante cambios bruscos de dirección, deja de calcular la derivada de error y actúa simplemente como ganancia proporcional adicional, induciendo vibraciones e inestabilidad dinámica en el chasis.
- **Solución recomendada:** 
  Eliminar la invocación a `resetearErrorAnterior()` dentro del ciclo continuo de `condicionAvanzar`. Solo debe llamarse al ingresar a un nuevo estado o tras una detención física completa.

---

#### BUG-10: Zona Muerta en la Lógica de Decisión para Distancias Exactas al Umbral
- **Ubicación:** `src/main.cpp` (Líneas 63–66)
- **Problema:** Las cuatro condiciones de bifurcación de la FSM utilizan desigualdades estrictas (`<` y `>`):
  ```cpp
  bool condicionGiroDer = sensadoActual.distanciaDer > UMBRAL_PARED_ESTADO_NORMAL;
  bool condicionAvanzar = sensadoActual.distanciaDer < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaCent > UMBRAL_PARED_ESTADO_NORMAL;
  bool condicionGiroIzq = sensadoActual.distanciaDer < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaCent < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaIzq > UMBRAL_PARED_ESTADO_NORMAL;
  bool condicionGiro180 = sensadoActual.distanciaDer < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaCent < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaIzq < UMBRAL_PARED_ESTADO_NORMAL;
  ```
  Si cualquiera de los sensores registra un valor exactamente igual a `UMBRAL_PARED_ESTADO_NORMAL` (130 mm):
  - Tanto `distancia < 130` como `distancia > 130` evalúan a `false`.
  - Las cuatro variables booleanas resultan `false`.
  - La cadena `if ... else if` no ejecuta ninguna rama, dejando al robot sin acción de transición. Si BUG-07 estuviera presente, esto causa la caída en `PREGIRO_DER`. Si se corrige BUG-07 con un `break;`, el robot continúa avanzando sin saber a dónde va.
- **Solución recomendada:** 
  Unificar los operadores lógicos para cubrir el espectro continuo usando `<=` y `>=`, y proveer una rama `else` por defecto:
  ```cpp
  bool hayParedDer = sensadoActual.distanciaDer <= UMBRAL_PARED_ESTADO_NORMAL;
  bool hayParedCent = sensadoActual.distanciaCent <= UMBRAL_PARED_ESTADO_NORMAL;
  bool hayParedIzq = sensadoActual.distanciaIzq <= UMBRAL_PARED_ESTADO_NORMAL;

  if (!hayParedDer) {
      // Prioridad 1: Doblar a la derecha si no hay pared
      estado = PREGIRO_DER;
  } else if (!hayParedCent) {
      // Prioridad 2: Seguir adelante si el frente está libre
      estado = AVANZANDO;
  } else if (!hayParedIzq) {
      // Prioridad 3: Doblar a la izquierda si el frente está cerrado
      estado = PREGIRO_IZQ;
  } else {
      // Prioridad 4: Callejón sin salida (3 paredes presentes)
      estado = GIRANDO_180;
  }
  ```

---

#### BUG-11: Estados Declarados en la FSM no Implementados (`DECISION`, `POSTGIRO`)
- **Ubicación:** `src/main.h` (Líneas 12–22) frente a `src/main.cpp` (Líneas 39–163)
- **Problema:** En el enum `MAQUINA_NUEVA` se definieron los estados `DECISION` y `POSTGIRO`. Sin embargo, en el `switch (estado)` de `main.cpp` no existen etiquetas `case DECISION:` ni `case POSTGIRO:`.
  Al completar cualquier giro (`GIRANDO_DER`, `GIRANDO_IZQ`, `GIRANDO_180`), el robot transiciona inmediatamente a `AVANZANDO`. Al no existir una fase `POSTGIRO` (avance ciego de unos milímetros para alinear el cuerpo del robot en la celda) ni un estado de evaluación `DECISION`, los sensores laterales aún leen la esquina recién doblada, pudiendo disparar falsos giros encadenados.
- **Solución recomendada:** 
  Implementar formalmente `POSTGIRO` en `main.cpp` para asegurar que el robot avance los 50-80 mm iniciales de la nueva celda con odometría pura antes de reactivar la lectura reactiva de paredes.

---

#### BUG-12: Ausencia de Timeouts de Seguridad en Estados de Giro
- **Ubicación:** `src/main.cpp` (Líneas 91, 106, 122, 137, 152)
- **Problema:** La condición de culminación de todas las maniobras de rotación depende exclusivamente de que los encoders alcancen un valor numérico prefijado (ej. `pulsosActuales < PULSOS_GIRO_90_DER`).
  Si una rueda queda suspendida en el aire, patina por pérdida de tracción sobre el polvo del laberinto, choca mecánicamente contra una pared inclinada o un cable de señal del encoder sufre un falso contacto:
  - `pulsosActuales` nunca alcanzará el umbral.
  - El robot quedará atrapado en un bucle infinito girando sobre sí mismo hasta agotar la batería, recalentando los bobinados de los motores y los transistores del puente H.
- **Solución recomendada:** 
  Incorporar un mecanismo de tiempo límite (*timeout*) con `millis()` en cada estado de rotación:
  ```cpp
  static unsigned long tiempoInicioGiro = 0;
  // Al entrar al estado: tiempoInicioGiro = millis();
  if (pulsosActuales >= PULSOS_GIRO_90_DER || (millis() - tiempoInicioGiro > 1500)) {
      // Finalizar giro por pulsos o por tiempo máximo de seguridad (1.5 s)
      movimiento(FRENO_F, {0,0});
      estado = AVANZANDO;
  }
  ```

---

### CATEGORÍA 4: Cinemática, Odometría y Navegación

#### BUG-13: Conteo Asimétrico de Encoders (División `/ 2` en Giro Derecho)
- **Ubicación:** `src/main.cpp` (Líneas 89, 120 frente a Líneas 104, 135)
- **Problema:** En `main.cpp`, el cálculo de pulsos para maniobras a la derecha incluye una división entera por 2:
  ```cpp
  89:  case PREGIRO_DER : {
  90:    pulsosActuales = (abs(verPulsosEncoderA())) / 2;
  ...
  120: case GIRANDO_DER: {
  121:   pulsosActuales = (abs(verPulsosEncoderA())) / 2;
  ```
  Mientras que para la izquierda se toma el valor directo:
  ```cpp
  104: case PREGIRO_IZQ : {
  105:   pulsosActuales = abs(verPulsosEncoderB());
  ...
  135: case GIRANDO_IZQ: {
  136:   pulsosActuales = abs(verPulsosEncoderB());
  ```
  **Impacto:** Para alcanzar `PULSOS_GIRO_90_DER = 300`, el motor derecho debe entregar **600 pulsos físicos de encoder**, mientras que para girar a la izquierda con `PULSOS_GIRO_90_IZQ = 280` solo requiere 280 pulsos. Esto provoca que el robot rote aproximadamente **180° físicos** cuando cree estar rotando 90° a la derecha, destruyendo la orientación angular en el laberinto.
- **Solución recomendada:** 
  Eliminar la división `/ 2` de las líneas 89 y 120. Ambas ruedas deben evaluarse en la misma escala física y calibrarse experimentalmente en `config.h`.

---

#### BUG-14: Configuración de Pulsos para Giro de 180° Idéntica al Giro de 90°
- **Ubicación:** `src/config.h` (Línea 71) y `src/main.cpp` (Línea 152)
- **Problema:** En `config.h`:
  ```cpp
  #define PULSOS_GIRO_90_DER 300
  #define PULSOS_GIRO_180 300
  ```
  En `main.cpp:152`:
  ```cpp
  if (pulsosActuales < PULSOS_GIRO_180) { ... }
  ```
  El número de pulsos para dar media vuelta en un callejón sin salida (180°) está fijado en 300, exactamente igual al giro de 90°.
  Al detectar un callejón cerrado (`condicionGiro180`), el robot ejecuta un giro de tan solo 90°, se detiene y pasa a `AVANZANDO`, estrellándose de frente a toda velocidad contra la pared lateral del pasillo.
- **Solución recomendada:** 
  Ajustar `PULSOS_GIRO_180` en `config.h` a aproximadamente el doble de los pulsos de un giro de 90° (ej. entre 560 y 600 pulsos, a ajustar en pista según el ancho de vía del robot):
  ```cpp
  #define PULSOS_GIRO_180 580
  ```

---

#### BUG-15: Desconexión Arquitectónica de Odometría de Celda (`PULSOS_CELDA`)
- **Ubicación:** `src/config.h` (Línea 66) frente a `src/main.cpp` (Archivo completo)
- **Problema:** En `config.h` se define `#define PULSOS_CELDA 800`. Sin embargo, `PULSOS_CELDA` no se utiliza en ninguna parte de `src/main.cpp`.
  La navegación actual es 100% reactiva y ciega a la cuadrícula del laberinto. Si el robot transita por un pasillo recto y se abre una bifurcación a su derecha, la condición `distanciaDer > UMBRAL` se dispara de inmediato en cualquier posición intermedia de la celda. El robot comenzará el pre-giro desfasado respecto al centro de la celda, colisionando inevitablemente sus ruedas traseras contra el pilar de la esquina.
- **Solución recomendada:** 
  Implementar avance discretizado por celdas: una vez iniciada una celda, avanzar midiendo `pulsosActuales` hasta alcanzar `PULSOS_CELDA` (180 mm), posicionar el centro del robot en el cruce y recién allí evaluar los sensores para decidir la siguiente maniobra.

---

### CATEGORÍA 5: Algoritmo de Control y Corrección PID

#### BUG-16: Ley de Control PID de Pared Única Invertida y Carente de Setpoint
- **Ubicación:** `src/hardware/movimiento/PID.cpp` (Líneas 17–23)
- **Problema:** En `calcularCorreccion`:
  ```cpp
  } else if (hayIzq) {
      // Solo pared izquierda: mantenerse a la distancia ideal (OFSET_IZQ representa nuestro objetivo ideal)
      error = - (int16_t)mediciones.distanciaIzq;
  } else if (hayDer) {
      // Solo pared derecha
      error = (int16_t)mediciones.distanciaDer;
  }
  ```
  El comentario explícitamente reconoce que debe mantenerse una distancia objetivo. Sin embargo, en el código **no se resta ningún setpoint de referencia**.
  **Análisis Matemático y Cinemático:**
  - En `main.cpp:58-60`:
    $$\text{velIzq} = \text{constrain}(\text{VEL\_BASE\_IZQ} + \text{correccion}, 0, 255)$$
    $$\text{velDer} = \text{constrain}(\text{VEL\_BASE\_DER} - \text{correccion}, 0, 255)$$
  - En una plataforma de tracción diferencial, acelerar la rueda izquierda y desacelerar la derecha ($\text{correccion} > 0$) provoca un **giro hacia la DERECHA**.
  - Acelerar la rueda derecha y desacelerar la izquierda ($\text{correccion} < 0$) provoca un **giro hacia la IZQUIERDA**.
  - **Fallo en código actual:** Si el robot detecta solo la pared izquierda a una distancia de 50 mm, el código calcula $\text{error} = -50$. La corrección negativa reduce la rueda izquierda y acelera la derecha, haciendo que el robot vire a la izquierda contra la pared que pretendía seguir.
  - **Convención de Signos Coherente con `main.cpp`:**
    1. **Seguimiento de Pared Izquierda (`hayIzq`):**
       - Si está demasiado cerca de la pared izquierda ($\text{distanciaIzq} < \text{DISTANCIA\_OBJETIVO\_PARED}$), debe virar hacia la DERECHA para alejarse ($\text{correccion} > 0 \implies \text{error} > 0$).
       - Si está demasiado lejos de la pared izquierda ($\text{distanciaIzq} > \text{DISTANCIA\_OBJETIVO\_PARED}$), debe virar hacia la IZQUIERDA para acercarse ($\text{correccion} < 0 \implies \text{error} < 0$).
       - Por lo tanto, la ecuación correcta es:
         $$\text{error} = \text{DISTANCIA\_OBJETIVO\_PARED} - \text{mediciones.distanciaIzq}$$
    2. **Seguimiento de Pared Derecha (`hayDer`):**
       - Si está demasiado cerca de la pared derecha ($\text{distanciaDer} < \text{DISTANCIA\_OBJETIVO\_PARED}$), debe virar hacia la IZQUIERDA para alejarse ($\text{correccion} < 0 \implies \text{error} < 0$).
       - Si está demasiado lejos de la pared derecha ($\text{distanciaDer} > \text{DISTANCIA\_OBJETIVO\_PARED}$), debe virar hacia la DERECHA para acercarse ($\text{correccion} > 0 \implies \text{error} > 0$).
       - Por lo tanto, la ecuación correcta es:
         $$\text{error} = \text{mediciones.distanciaDer} - \text{DISTANCIA\_OBJETIVO\_PARED}$$
- **Solución recomendada:** 
  Definir una consigna de centrado nominal (`DISTANCIA_OBJETIVO_PARED ≈ 45 mm`) y calcular el error relativo a dicha consigna con el signo físico coherente con `main.cpp`:
  ```cpp
  const int16_t DISTANCIA_OBJETIVO_PARED = 45; // mm al centro del carril

  if (hayIzq && hayDer) {
      // Centrado diferencial entre ambas paredes:
      // Si distanciaDer > distanciaIzq (más cerca de izq), error > 0 -> correccion > 0 -> vira a la derecha
      // Si distanciaDer < distanciaIzq (más cerca de der), error < 0 -> correccion < 0 -> vira a la izquierda
      error = (int16_t)mediciones.distanciaDer - (int16_t)mediciones.distanciaIzq;
  } else if (hayIzq) {
      // Pared izquierda:
      // Si distanciaIzq < OBJETIVO (cerca), error > 0 -> correccion > 0 -> vira a la DERECHA (se aleja)
      // Si distanciaIzq > OBJETIVO (lejos), error < 0 -> correccion < 0 -> vira a la IZQUIERDA (se acerca)
      error = (int16_t)DISTANCIA_OBJETIVO_PARED - (int16_t)mediciones.distanciaIzq;
  } else if (hayDer) {
      // Pared derecha:
      // Si distanciaDer < OBJETIVO (cerca), error < 0 -> correccion < 0 -> vira a la IZQUIERDA (se aleja)
      // Si distanciaDer > OBJETIVO (lejos), error > 0 -> correccion > 0 -> vira a la DERECHA (se acerca)
      error = (int16_t)mediciones.distanciaDer - (int16_t)DISTANCIA_OBJETIVO_PARED;
  } else {
      error = 0;
  }
  ```

---

#### BUG-17: Sesgo Estático Permanente de 7 mm en Centrado de Dos Paredes
- **Ubicación:** `src/hardware/sensoresDistancia/sensoresDistancia.cpp` (Líneas 92, 102) frente a `src/hardware/movimiento/PID.cpp` (Línea 16)
- **Problema:** En `sensoresDistancia.cpp`:
  ```cpp
  lecturaAct.distanciaIzq = rawIzq - OFSET_IZQ; // OFSET_IZQ = 40
  lecturaAct.distanciaDer = rawDer - OFSET_DER; // OFSET_DER = 47
  ```
  Y en `PID.cpp`:
  ```cpp
  error = (int16_t)mediciones.distanciaDer - (int16_t)mediciones.distanciaIzq;
  ```
  Si el robot se encuentra físicamente en el centro exacto del pasillo, con la misma distancia física a ambas paredes (`rawDer == rawIzq`):
  $$\text{error} = (\text{rawDer} - 47) - (\text{rawIzq} - 40) = -7\text{ mm}$$
  El controlador PID percibe un error permanente de $-7\text{ mm}$ y corrige virando constantemente hacia la izquierda, haciendo imposible que el robot avance en línea recta por el centro del corredor.
- **Solución recomendada:** 
  Separar las mediciones de los offsets. En `sensoresDistancia.cpp`, devolver las lecturas milimétricas brutas o corregidas por calibración física individual de cada sensor, y realizar la compensación de centrado de forma unificada en el algoritmo PID.

---

#### BUG-18: Falta de Normalización Temporal ($\Delta t$) en el Término Derivativo
- **Ubicación:** `src/hardware/movimiento/PID.cpp` (Línea 28)
- **Problema:**
  ```cpp
  int16_t correccion = (KP * error) + (KD * (error - errorAnterior));
  ```
  La derivada matemática requiere el cociente diferencial $\frac{\Delta e}{\Delta t}$. El código multiplica directamente $K_d$ por la diferencia de error en muestras consecutivas sin considerar el tiempo transcurrido.
  Dado que las lecturas de los sensores ToF en el bus I2C y las comunicaciones Bluetooth introducen variaciones en la duración del bucle (de 10 ms a más de 50 ms), la tasa de variación efectiva fluctúa enormemente, generando picos erráticos de torque en los motores.
- **Solución recomendada:** 
  Ejecutar el cálculo del PID a una tasa de muestreo fija y periódica (por ejemplo, cada 20 ms mediante un temporizador `millis()` en FreeRTOS) o normalizar dividiendo por $\Delta t$:
  ```cpp
  unsigned long ahora = millis();
  float dt = (ahora - ultimoTiempo) / 1000.0f;
  if (dt <= 0.001f) dt = 0.001f;
  float derivada = (error - errorAnterior) / dt;
  int16_t correccion = (KP * error) + (KD * derivada);
  ultimoTiempo = ahora;
  ```

---

### CATEGORÍA 6: Accionamiento de Motores y Puente H

#### BUG-19: Inversión Total de Polaridad Cinemática en Giros sobre el Eje
- **Ubicación:** `src/hardware/movimiento/puenteH.cpp` (Líneas 39–56)
- **Problema:** En `puenteH.cpp`, el control de dirección para giros en el lugar establece:
  ```cpp
  case GIRAR_DER: {
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, HIGH);  // Rueda Izquierda (Canal A) -> RETROCESO
    digitalWrite(BIN1, HIGH);
    digitalWrite(BIN2, LOW);   // Rueda Derecha (Canal B) -> AVANCE
    ...
  }
  case GIRAR_IZQ: {
    digitalWrite(AIN1, HIGH);
    digitalWrite(AIN2, LOW);   // Rueda Izquierda (Canal A) -> AVANCE
    digitalWrite(BIN1, LOW);
    digitalWrite(BIN2, HIGH);  // Rueda Derecha (Canal B) -> RETROCESO
    ...
  }
  ```
  **Cinemática Diferencial:**
  - Si la rueda izquierda retrocede y la rueda derecha avanza, el robot rota sobre su eje en sentido **antihorario (HACIA LA IZQUIERDA)**. Sin embargo, esta combinación está asignada a `GIRAR_DER`.
  - Si la rueda izquierda avanza y la derecha retrocede, el robot rota en sentido **horario (HACIA LA DERECHA)**, asignado a `GIRAR_IZQ`.
  Salvo que los motores estén conectados con polaridad física inversa respecto a la marcha hacia adelante (`AVANZAR` en líneas 20–27), el software comanda las rotaciones en el sentido opuesto al deseado.
- **Solución recomendada:** 
  Invertir las combinaciones de señales para que correspondan con la física del chasis:
  ```cpp
  case GIRAR_DER: { // Rotación horaria: Izquierda avanza, Derecha retrocede
    digitalWrite(AIN1, HIGH);
    digitalWrite(AIN2, LOW);
    digitalWrite(BIN1, LOW);
    digitalWrite(BIN2, HIGH);
    ledcWrite(0, velocidad.izquierda);
    ledcWrite(1, velocidad.derecha);
    break;
  }
  case GIRAR_IZQ: { // Rotación antihoraria: Izquierda retrocede, Derecha avanza
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, HIGH);
    digitalWrite(BIN1, HIGH);
    digitalWrite(BIN2, LOW);
    ledcWrite(0, velocidad.izquierda);
    ledcWrite(1, velocidad.derecha);
    break;
  }
  ```

---

#### BUG-20: Maniobras de Giro Comandadas a Velocidad Insuficiente (`VEL_BASE`)
- **Ubicación:** `src/main.cpp` (Líneas 92, 107, 123, 138, 153) frente a `src/config.h` (Líneas 17–22)
- **Problema:** En `config.h` se definen valores diferenciados de velocidad:
  ```cpp
  #define VEL_BASE_DER 45
  #define VEL_BASE_IZQ 45
  #define VEL_GIRO_DER 100
  #define VEL_GIRO_IZQ 100
  ```
  Sin embargo, en `main.cpp`, todos los estados de rotación ejecutan:
  ```cpp
  movimiento(GIRAR_DER, {VEL_BASE_IZQ, VEL_BASE_DER}); // PWM 45
  ```
  Un valor de PWM de 45 sobre 255 (17.6% de ciclo de trabajo) es insuficiente para vencer el rozamiento estático de los neumáticos al girar sobre su propio eje. Los motores quedan calados zumbando, el robot no rota y nunca se generan los pulsos de encoder necesarios para salir del estado.
- **Solución recomendada:** 
  Utilizar `{VEL_GIRO_IZQ, VEL_GIRO_DER}` en los estados `GIRANDO_DER`, `GIRANDO_IZQ` y `GIRANDO_180`.

---

#### BUG-21: Frenado Pasivo por Inercia (*Coast*) en Lugar de Freno Dinámico Activo
- **Ubicación:** `src/hardware/movimiento/puenteH.cpp` (Líneas 57–65)
- **Problema:** En `FRENO_F`:
  ```cpp
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, LOW);
  digitalWrite(BIN1, LOW);
  digitalWrite(BIN2, LOW);
  ledcWrite(0, 0);
  ledcWrite(1, 0);
  ```
  En la mayoría de los controladores de puente H (por ejemplo TB6612FNG o L298N), colocar ambas entradas de canal en `LOW` desactiva los transistores de potencia, dejando los terminales del motor en alta impedancia (*Modo Coast*). El robot sigue rodando libremente por inercia durante varios centímetros antes de detenerse, impidiendo paradas precisas en el centro de las celdas.
- **Solución recomendada:** 
  Para lograr frenado dinámico activo (*Short Brake*), colocar ambas entradas de dirección en `HIGH` con PWM al 100% (o la combinación específica de frenado del integrado del puente H), cortocircuitando las bobinas para que la fuerza contra-electromotriz detenga el rotor al instante:
  ```cpp
  case FRENO_F: {
    digitalWrite(AIN1, HIGH);
    digitalWrite(AIN2, HIGH);
    digitalWrite(BIN1, HIGH);
    digitalWrite(BIN2, HIGH);
    ledcWrite(0, 255);
    ledcWrite(1, 255);
    break;
  }
  ```

---

#### BUG-22: Riesgo de Desbordamiento de Entero Negativo a Registro PWM (`uint32_t`)
- **Ubicación:** `src/hardware/movimiento/puenteH.cpp` (Líneas 25–26, 35–36, 44–45, 53–54)
- **Problema:** El tipo de dato para las velocidades es `int16_t` signado (`struct VELOCIDAD { int16_t izquierda; int16_t derecha; };`).
  En la función `movimiento`:
  ```cpp
  ledcWrite(0, velocidad.izquierda);
  ```
  La función del core ESP32 `ledcWrite(uint8_t channel, uint32_t duty)` espera un entero sin signo de 32 bits. Si por algún cálculo erróneo la velocidad resulta negativa (por ejemplo `-1`), el casting implícito lo convierte a `4294967295`. En un canal LEDC configurado a 8 bits (0–255), esto puede corromper el registro del periférico o generar ciclos de trabajo anómalos.
- **Solución recomendada:** 
  Asegurar la restricción de rango (*constrain*) antes de escribir en el hardware:
  ```cpp
  uint32_t dutyIzq = constrain((int)velocidad.izquierda, 0, 255);
  uint32_t dutyDer = constrain((int)velocidad.derecha, 0, 255);
  ledcWrite(0, dutyIzq);
  ledcWrite(1, dutyDer);
  ```

---

### CATEGORÍA 7: Sensores de Distancia y Bus I2C

#### BUG-23: Estructura de Sensado Inicial en Cero Provoca Giro Falso al Arranque
- **Ubicación:** `src/hardware/sensoresDistancia/sensoresDistancia.cpp` (Línea 83)
- **Problema:** En `sensoresDistancia.cpp`:
  ```cpp
  static sensado lecturaAct = {0,0,0};
  ```
  Los sensores ToF VL53L0X requieren al menos 30 a 50 ms para completar su primera medición física.
  Si el bucle principal evalúa las condiciones antes de que los sensores tengan su primer dato listo:
  - `distanciaCent = 0`, `distanciaDer = 0`, `distanciaIzq = 0`.
  - La condición `condicionGiro180` en `main.cpp:66` evalúa:
    `(0 < 130 && 0 < 130 && 0 < 130) == true`.
  - El robot asume que está atrapado en un callejón sin salida cerrado y ejecuta de inmediato una maniobra de giro de 180° apenas se presiona el botón de arranque.
- **Solución recomendada:** 
  Inicializar `lecturaAct` con valores lejanos seguros (ej. `{999, 999, 999}`) y añadir un flag booleano que impida a la máquina de estados operar hasta que se haya completado exitosamente la primera ronda de mediciones de los tres sensores.

---

#### BUG-24: Bucle Infinito Bloqueante ante Falla de Inicialización de un Sensor
- **Ubicación:** `src/hardware/sensoresDistancia/sensoresDistancia.cpp` (Líneas 43–46, 57–60, 71–74)
- **Problema:** En la inicialización:
  ```cpp
  if (!sensorDer.init()) {
    Serial.printf("ERROR: fallo init sensor en pin %d\n", xshutPinDer);
    while (true) delay(1000);
  }
  ```
  Si cualquiera de los tres sensores falla al arrancar (por ejemplo, debido a ruido en el bus I2C, caída de tensión momentánea o desconexión física), el microcontrolador se cuelga en un bucle infinito `while(true)`. No hay señal acústica, ni código de error devuelto, ni recuperación, ni informe por Bluetooth.
- **Solución recomendada:** 
  Hacer que `inicializacionSensoresDist()` devuelva un `bool` de estado. Implementar hasta 3 reintentos de inicialización y, en caso de fallo permanente, activar un patrón de parpadeo en un LED de estado o reportarlo por telemetría.

---

#### BUG-25: Frecuencia de Reloj I2C Degradada Anormalmente a 10 kHz
- **Ubicación:** `src/hardware/sensoresDistancia/sensoresDistancia.cpp` (Línea 23)
- **Problema:**
  ```cpp
  Wire.setClock(10000); // 10 kHz
  ```
  La frecuencia estándar del bus I2C es de 100 kHz (Standard Mode) o 400 kHz (Fast Mode). Al configurar el bus a tan solo 10 kHz:
  - Cada transacción de lectura de registro toma varios milisegundos.
  - La lectura de los 3 sensores bloquea la CPU durante 20 a 30 ms en cada ciclo.
  - La frecuencia de control del lazo PID cae a menos de 30 Hz, provocando que el robot reaccione tarde a las desviaciones de trayectoria cuando se desplaza a velocidad de competencia.
- **Solución recomendada:** 
  Instalar resistencias físicas pull-up externas de 2.2 kΩ a 3.3V en las líneas SDA y SCL, y configurar el reloj I2C a 100 kHz (`Wire.setClock(100000)`) o 400 kHz (`Wire.setClock(400000)`).

---

#### BUG-26: Timeouts de Sensor Enmascarados como Pasillo Libre y Underflow Numérico
- **Ubicación:** `src/hardware/sensoresDistancia/sensoresDistancia.cpp` (Líneas 90–103)
- **Problema:** 
  1. Cuando un sensor VL53L0X pierde la comunicación o experimenta un timeout de hardware, la librería retorna `65535`. El código evalúa:
     ```cpp
     if (rawIzq > 2000) rawIzq = 2000;
     lecturaAct.distanciaIzq = rawIzq - OFSET_IZQ; // 2000 - 40 = 1960 mm
     ```
     Un fallo momentáneo de hardware es interpretado como un pasillo despejado de 1.96 metros, haciendo que el robot crea que no hay pared y vire bruscamente contra el obstáculo.
  2. Si el robot se aproxima a menos de 40 mm de la pared izquierda, `rawIzq` puede ser 25 mm. La resta `25 - 40 = -15` genera un número negativo que distorsiona las comparaciones de proximidad.
- **Solución recomendada:** 
  Verificar formalmente `timeoutOccurred()` y acotar las lecturas mínimas a 0:
  ```cpp
  if (sensorIzq.timeoutOccurred() || rawIzq > 2000) {
      // Mantener la última lectura válida o reportar pérdida de señal
  } else {
      lecturaAct.distanciaIzq = (rawIzq > OFSET_IZQ) ? (rawIzq - OFSET_IZQ) : 0;
  }
  ```

---

#### BUG-27: Doble Lectura Consecutiva de Sensores y Rate-Limiter Comentado
- **Ubicación:** `src/main.cpp` (Líneas 56 y 61) frente a `src/hardware/sensoresDistancia/sensoresDistancia.cpp` (Líneas 88, 105)
- **Problema:** En `main.cpp`, dentro de `case AVANZANDO:`, se invoca `sensadoActual = actualizarSensado()` dos veces separadas por tan solo 5 líneas de código (antes y después del cálculo de velocidad). En `sensoresDistancia.cpp`, la guarda temporal de 20 ms (`if (millis() - ultimoSensado > 20)`) está deshabilitada con comentarios. Esto genera transacciones I2C duplicadas e inútiles en cada ciclo del procesador.
- **Solución recomendada:** 
  Llamar a `actualizarSensado()` una sola vez por ciclo en `loop()` y reactivar la guarda temporal de 20 ms en el driver de sensores.

---

### CATEGORÍA 8: Hardware, Señales Eléctricas y Strapping Pins

#### BUG-28: Peligro Crítico de Pin de Strapping MTDI en GPIO 12 (`PWMA`)
- **Ubicación:** `src/config.h` (Línea 49) y `src/hardware/movimiento/puenteH.cpp` (Línea 14)
- **Problema:** En `config.h`:
  ```cpp
  #define PWMA 12
  ```
  En la arquitectura ESP32, el **GPIO 12 es el pin de strapping MTDI**. Durante el proceso de arranque/reinicio del silicio:
  - Si GPIO 12 se muestrea en estado `HIGH` (3.3V), el regulador interno LDO de voltaje de la memoria Flash conmutará a **1.8V**.
  - Los módulos estándar ESP-WROOM-32 operan con memoria Flash de **3.3V**. Al recibir solo 1.8V, la flash no responde y la ROM del ESP32 aborta el arranque entrando en un ciclo infinito de bootloop:
    ```text
    flash read err, 1000
    rst:0x10 (RTCWDT_RTC_RESET),boot:0x12 (SPI_FAST_FLASH_BOOT)
    ```
  Si el módulo de puente H posee una resistencia de pull-up interna o una fuga de polarización en su entrada PWM, el robot no podrá encender.
- **Solución recomendada:** 
  1. Reasignar `PWMA` a un GPIO seguro que no sea de bootstrapping (como GPIO 13, 21 o 22).
  2. Si el hardware PCB ya está fabricado, quemar el eFuse de voltaje de flash de forma permanente usando `espefuse.py`:
     ```bash
     espefuse.py --port COMx set_flash_voltage 3.3V
     ```
  3. Asegurar una resistencia pull-down externa fuerte (4.7 kΩ a GND) en la línea de GPIO 12.

---

#### BUG-29: Entrada Flotante en Pulsador de Inicio (GPIO 34 sin Pull-Up Interno)
- **Ubicación:** `src/config.h` (Línea 42) y `src/main.cpp` (Línea 25)
- **Problema:** En `config.h`:
  ```cpp
  #define BOTON1 34
  ```
  Y en `main.cpp`:
  ```cpp
  pinMode(BOTON1, INPUT);
  ```
  Los pines GPIO 34 a 39 en el ESP32 son pines exclusivamente de entrada (GPI) y **carecen físicamente de resistencias pull-up o pull-down en el silicio**. La instrucción `pinMode(BOTON1, INPUT_PULLUP)` es ignorada por el hardware.
  Si el diseño esquemático no cuenta con una resistencia física pull-up soldada en la placa, el pin queda flotando en alta impedancia, captando ruido electromagnético de los motores y disparando arranques fantasmas e inesperados.
- **Solución recomendada:** 
  Verificar la presencia de una resistencia física de pull-up (10 kΩ a 3.3V) en GPIO 34. Si no se cuenta con ella, reubicar el pulsador a un GPIO con soporte de pull-up interno (GPIO 15, 2, 4, 18, 19, 23) y configurar `pinMode(pin, INPUT_PULLUP)`.

---

### CATEGORÍA 9: Defectos Críticos de Arquitectura, Control y Observabilidad Incorporados por Revisión Adversarial (BUG-30 a BUG-34)

#### BUG-30: Omisión de `UMBRAL_PARED_FRENTE` (120 mm) y Reemplazo Erróneo en FSM
- **Ubicación:** `src/config.h` (Línea 37) frente a `src/main.cpp` (Líneas 64–66)
- **Problema:** En `config.h`, línea 37, el desarrollador definió formalmente:
  ```cpp
  //Es el umbral (MM) para que el robot gire si la pared está frente a él
  #define UMBRAL_PARED_FRENTE 120
  ```
  Esta constante contempla la física del robot: el sensor frontal posee un offset mecánico sustancialmente diferente (`OFSET_CENT = 80 mm` frente a 40 mm y 47 mm de los sensores laterales).
  Sin embargo, en `src/main.cpp:64-66`, las condiciones de navegación comparan la distancia central contra el umbral genérico lateral `UMBRAL_PARED_ESTADO_NORMAL` (130 mm):
  ```cpp
  bool condicionAvanzar = sensadoActual.distanciaDer < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaCent > UMBRAL_PARED_ESTADO_NORMAL;
  bool condicionGiroIzq = sensadoActual.distanciaDer < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaCent < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaIzq > UMBRAL_PARED_ESTADO_NORMAL;
  bool condicionGiro180 = sensadoActual.distanciaDer < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaCent < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaIzq < UMBRAL_PARED_ESTADO_NORMAL;
  ```
  La macro `UMBRAL_PARED_FRENTE` fue omitida, quedando como código muerto inerte. Cuando el robot avanza hacia una pared frontal y la lectura de `distanciaCent` se sitúa entre 121 y 130 mm, el robot interpreta errónea y prematuramente que el frente está cerrado (`distanciaCent < 130`), abortando el avance lineal y forzando un giro no deseado a una celda incompleta.
- **Solución recomendada:** 
  Actualizar las transiciones en `src/main.cpp` para comparar `distanciaCent` exclusivamente contra `UMBRAL_PARED_FRENTE`:
  ```cpp
  bool condicionAvanzar = sensadoActual.distanciaDer <= UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaCent > UMBRAL_PARED_FRENTE;
  bool condicionGiroIzq = sensadoActual.distanciaDer <= UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaCent <= UMBRAL_PARED_FRENTE && sensadoActual.distanciaIzq > UMBRAL_PARED_ESTADO_NORMAL;
  bool condicionGiro180 = sensadoActual.distanciaDer <= UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaCent <= UMBRAL_PARED_FRENTE && sensadoActual.distanciaIzq <= UMBRAL_PARED_ESTADO_NORMAL;
  ```

---

#### BUG-31: Discrepancia Espacial Crítica de 49 mm entre Umbrales de FSM (130 mm) y PID (180 mm)
- **Ubicación:** `src/main.cpp` (Línea 63) frente a `src/hardware/movimiento/PID.cpp` (Líneas 8–10)
- **Problema:** En `src/main.cpp:63`:
  ```cpp
  bool condicionGiroDer = sensadoActual.distanciaDer > UMBRAL_PARED_ESTADO_NORMAL; // 130 mm
  ```
  Si `distanciaDer > 130 mm`, la FSM asume que **NO hay pared lateral** y planifica un giro a la derecha.
  En contraste directo, en `src/hardware/movimiento/PID.cpp:8-10`:
  ```cpp
  bool hayIzq = mediciones.distanciaIzq < (UMBRAL_PARED_ESTADO_NORMAL + 50); // 130 + 50 = 180 mm!
  bool hayDer = mediciones.distanciaDer < (UMBRAL_PARED_ESTADO_NORMAL + 50); // 180 mm!
  ```
  Para el controlador PID, cualquier lectura inferior a 180 mm se considera **presencia válida de pared**.
  En la brecha de 131 mm a 179 mm (un ancho de 49 mm, característico de esquinas abiertas, aperturas laterales o pasillos contiguos en celdas de 180 mm):
  1. Si el robot transita junto a una abertura lateral donde la pared del pasillo contiguo se detecta a ~150 mm, la FSM comanda avance.
  2. Sin embargo, el PID evalúa `hayDer = true` (150 < 180 mm). Si detecta además la pared izquierda a 45 mm, calcula `error = 150 - 45 = +105 mm`, acelerando fuertemente la rueda izquierda (virando a la derecha).
  3. El robot se desvía bruscamente hacia la derecha contra el pilar de la esquina abierta en lugar de mantener la línea central.
- **Solución recomendada:** 
  Unificar el criterio de presencia de pared entre la FSM y el PID en una función o constante común (`UMBRAL_PRESENCIA_PARED ≈ 95 mm`), acotando la ventana de seguimiento del PID exclusivamente a distancias físicas de la celda local:
  ```cpp
  #define UMBRAL_PRESENCIA_PARED 95 // mm (pared correspondiente al carril propio)
  bool hayIzq = mediciones.distanciaIzq <= UMBRAL_PRESENCIA_PARED;
  bool hayDer = mediciones.distanciaDer <= UMBRAL_PRESENCIA_PARED;
  ```

---

#### BUG-32: Saturación de Memoria Flash por Esquema de Partición por Defecto y BluetoothSerial
- **Ubicación:** `platformio.ini` (Líneas 11–19)
- **Problema:** El firmware incluye `BluetoothSerial.h`, que incorpora la pila de protocolos Bluedroid de Bluetooth Clásico. La compilación del firmware arroja un consumo masivo:
  ```text
  Flash: [========= ] 86.7% (used 1,135,869 bytes from 1,310,720 bytes)
  ```
  Con la configuración por defecto de PlatformIO para placas ESP32 (`esp32doit-devkit-v1`), se aplica la tabla de particiones `default.csv`, que reserva únicamente 1,310,720 bytes (1.25 MB) para la partición de la aplicación (`app0`).
  Esto deja escasamente **174 KB libres** de memoria Flash. Al intentar incorporar los algoritmos reales de MicroMouse (FloodFill, matrices 16x16 de mapeo y costos Manhattan, pila de retorno y exploración, persistencia en NVS o concurrencia FreeRTOS), el compilador abortará con fallo fatal de enlace:
  ```text
  firmware.elf section '.flash.text' will not fit in region 'app0'
  ```
- **Solución recomendada:** 
  1. Declarar explícitamente en `platformio.ini` la tabla de particiones `huge_app.csv`, que elimina particiones OTA secundarias y asigna ~3.1 MB a la partición de la aplicación:
     ```ini
     board_build.partitions = huge_app.csv
     ```
  2. A mediano plazo, reemplazar Bluetooth Clásico (`BluetoothSerial`) por BLE (Bluetooth Low Energy) o telemetría UART física/inalámbrica liviana, liberando más de 800 KB de flash para algoritmos de laberinto.

---

#### BUG-33: Ausencia de Cláusula `default:` en `puenteH.cpp` y Omisión de Estado Seguro en `inicializarMotores()`
- **Ubicación:** `src/hardware/movimiento/puenteH.cpp` (Líneas 6–16, 18–67)
- **Problema:** 
  1. En `puenteH.cpp:18-67`, la función `movimiento(MOVIMIENTOS movimiento, VELOCIDAD velocidad)` evalúa un `switch (movimiento)` que carece totalmente de la etiqueta `default:`. Si por corrupción de memoria, condición de carrera o recepción de comandos corruptos se pasa un valor fuera del rango `[0..4]`, el bloque se salta sin modificar los pines de dirección ni los canales PWM, manteniendo los motores en su estado previo en bucle abierto sin capacidad de parada de emergencia.
  2. En `puenteH.cpp:6-16`:
     ```cpp
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
     ```
     No se ejecuta `digitalWrite(pin, LOW)` en las 4 líneas de dirección ni `ledcWrite(ch, 0)` en los canales PWM. Durante el arranque del microcontrolador, transitorios eléctricos o estados residuales del registro LEDC pueden generar impulsos espurios de tracción antes de que `loop()` tome el control.
- **Solución recomendada:** 
  1. Añadir la etiqueta `default:` en `switch (movimiento)` garantizando detención segura:
     ```cpp
     default:
         movimiento(FRENO_F, {0, 0});
         break;
     ```
  2. Forzar estado seguro y PWM a cero al final de `inicializarMotores()`:
     ```cpp
     void inicializarMotores(){
         pinMode(AIN1, OUTPUT);
         pinMode(AIN2, OUTPUT);
         pinMode(BIN1, OUTPUT);
         pinMode(BIN2, OUTPUT);
         digitalWrite(AIN1, LOW);
         digitalWrite(AIN2, LOW);
         digitalWrite(BIN1, LOW);
         digitalWrite(BIN2, LOW);
         ledcSetup(0, 20000, 8);
         ledcSetup(1, 20000, 8);
         ledcAttachPin(PWMA, 0);
         ledcAttachPin(PWMB, 1);
         ledcWrite(0, 0);
         ledcWrite(1, 0);
     }
     ```

---

#### BUG-34: Inobservabilidad Total de Telemetría por UART en `main.cpp` pese a `Serial.begin(115200)`
- **Ubicación:** `src/main.cpp` frente a `src/hardware/logger/logger.cpp` (Líneas 11–18)
- **Problema:** En `logger.cpp:17`, la función `inicializarLogger()` inicializa la interfaz UART por hardware con `Serial.begin(115200);`, y `platformio.ini:19` define `monitor_speed = 115200`. Sin embargo, en todo `src/main.cpp`, todos los mensajes se transmiten únicamente invocando `enviarString()`:
  ```cpp
  void enviarString(String str){
      SerialBT.println(str);
  }
  ```
  No existe una sola llamada a `Serial.print()` ni `Serial.println()` en el lazo principal. Cuando un ingeniero conecta el robot mediante cable USB al Monitor Serial de PlatformIO, la consola permanece completamente muda durante toda la ejecución. Si no hay un smartphone o computadora emparejado por Bluetooth, el sistema es 100% inobservable en banco de ensayos.
- **Solución recomendada:** 
  Actualizar `enviarString()` en `logger.cpp` para replicar concurrentemente la salida tanto en Bluetooth como en UART física:
  ```cpp
  void enviarString(const String& str){
      SerialBT.println(str);
      Serial.println(str);
  }
  ```

---

### CATEGORÍA 10: Telecomunicaciones, Gestión de Memoria y Limpieza de Repositorio (Hallazgos Adicionales)

#### Defectos Adicionales Identificados:
1. **Función `cambioDeCelda()` Inoperativa (`src/hardware/logger/logger.cpp:20-22`):**
   La función verifica `if (SerialBT.available() > 0)` pero jamás lee el byte con `.read()`. Una vez recibido un caracter de prueba, devuelve `true` permanentemente durante toda la sesión del robot.
2. **Inundación del Buffer TX de Bluetooth (`src/main.cpp:75`):**
   `enviarString(">>> AVANZANDO <<<")` ejecutado en cada iteración del bucle principal colapsa el búfer circular de FreeRTOS, introduciendo latencias de bloqueo en la CPU de decenas de milisegundos.
3. **Fragmentación del Heap por Parámetros `String` por Valor (`src/hardware/logger/logger.h:13`, `logger.cpp:11`):**
   La función `void enviarString(String str)` duplica el objeto dinámico en cada llamada. Debe reemplazarse por `const String&` o `const char*` para evitar fugas y fragmentación de memoria dinámica.
4. **Archivos de Prueba Huérfanos en `src/`:**
   Existen 8 archivos de prueba en `src/` (`bluetooth.txt`, `encodersSerial.txt`, `movimiento.txt`, `pruebaEncoders`, `pruebaMotoresAislados.txt`, `sensoresSDist.txt`, `test.txt`, `testearHardware.txt`). En particular:
   - `src/pruebaEncoders` no tiene extensión y duplica `setup()` y `loop()`.
   - `src/pruebaMotoresAislados.txt` y `src/test.txt` incluyen cabeceras inexistentes (`hardware/sensorPiso/sensorPiso.h`, `memoria/funcionesMapeo.h`).
   - `src/bluetooth.txt` contiene llamadas bloqueantes `delay(5000)` con motores en marcha sin botón de parada.

---

## 4. Plan de Remediación e Implementación por Fases

### Fase 1: Desbloqueo de Compilación y Configuración de Memoria Flash (Inmediato)
1. Mover `src/CITÉ.cpp` y los 8 archivos `.txt` / sin extensión fuera de `src/` hacia `test/legacy/`.
2. Actualizar `platformio.ini` con la directiva `build_src_filter = +<*> -<CIT*.cpp>`.
3. Configurar en `platformio.ini` la tabla de particiones `board_build.partitions = huge_app.csv` para garantizar 3 MB en `app0` y evitar desbordamiento de flash por Bluetooth (BUG-32).
4. Limpiar las cabeceras `logger.h`, `sensoresDistancia.h` y `puenteH.h` eliminando los prototipos huérfanos.

### Fase 2: Corrección de la FSM, Actuadores y Cinemática de Navegación (Crítico)
1. Agregar `break;` al final de `case AVANZANDO:` en `src/main.cpp:86`.
2. Evaluar `distanciaCent` exclusivamente respecto a `UMBRAL_PARED_FRENTE` (120 mm) en lugar de `UMBRAL_PARED_ESTADO_NORMAL` (130 mm) en `src/main.cpp` (BUG-30).
3. Añadir la cláusula `default:` en `puenteH.cpp` y asegurar puesta a nivel bajo y PWM cero en `inicializarMotores()` (BUG-33).
4. Eliminar las invocaciones continuas a `resetearEncoders()` y `resetearErrorAnterior()` en la rama `condicionAvanzar`.
5. Invertir las polaridades de giro en `src/hardware/movimiento/puenteH.cpp` (`GIRAR_DER` y `GIRAR_IZQ`).
6. Eliminar la división `/ 2` en los encoders de giro derecho en `src/main.cpp`.
7. Calibrar `PULSOS_GIRO_180` en `config.h` a aproximadamente `580 - 600` pulsos y asignar `VEL_GIRO` en lugar de `VEL_BASE` en las rotaciones.

### Fase 3: Reformulación del Algoritmo PID, Coherencia Espacial y Sensores (Alto)
1. Unificar los umbrales de presencia de pared entre FSM y PID (`UMBRAL_PRESENCIA_PARED ≈ 95 mm`) para eliminar la discrepancia de 49 mm (BUG-31).
2. Reescribir `calcularCorreccion` en `PID.cpp` incorporando `DISTANCIA_OBJETIVO_PARED` con la convención de signos coherente con `main.cpp` (BUG-16).
3. Aislar los offsets de hardware en `sensoresDistancia.cpp` y compensar la asimetría de 7 mm.
4. Incorporar manejo de timeouts en VL53L0X para evitar que desconexiones se traduzcan en pasillos abiertos de 1960 mm.
5. Ajustar el reloj I2C a 100 kHz o 400 kHz y habilitar el filtro temporal de 20 ms.

### Fase 4: Seguridad Eléctrica, Observabilidad y Telecomunicaciones (Preventivo)
1. Replicar la telemetría en `logger.cpp` hacia `Serial.println(str)` para permitir observabilidad completa por el monitor serie USB a 115200 baudios (BUG-34).
2. Validar hardware o reasignar `PWMA` fuera del GPIO 12 (MTDI) y verificar pull-up externo en GPIO 34.
3. Restringir la telemetría Bluetooth a eventos de cambio de estado y pasar cadenas por referencia `const String&`.
4. Implementar frenado activo (*Short Brake*) en el driver del puente H.

---

## 5. Método de Verificación Independiente

Para validar las correcciones una vez implementadas sin alterar el código durante esta etapa de auditoría:
1. **Compilación Limpia:** Ejecutar `pio run` desde la raíz del proyecto para comprobar la ausencia de errores de compilador y enlazador.
2. **Prueba Unitaria de Control PID:** Evaluar con un arnés de pruebas que ante distancias unilaterales de 20 mm, 45 mm y 80 mm, la salida del PID empuje al robot hacia el centro de la celda y no hacia el muro.
3. **Prueba de Odometría:** Verificar en banco de pruebas que al girar manualmente las ruedas exactamente 360°, la cuenta de pulsos de ambos canales sea idéntica y no difiera por un factor de 2.
4. **Verificación de Strapping Pin:** Medir con osciloscopio la línea GPIO 12 durante la secuencia de encendido asegurando nivel lógico bajo (< 0.8V) durante el reset del chip.
