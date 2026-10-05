# Reporte de Seguridad, Seguridad de Memoria y Casos Borde — debugRobot (ESP32)

**Fecha**: 2026-10-05  
**Auditor**: Teamwork Preview Explorer 3 (Security, Memory Safety & Edge Cases)  
**Proyecto**: `debugRobot` (Micro-Mouse / Robot Autónomo de Laberinto)  
**Plataforma**: ESP32 DevKit V1 (Espressif ESP32, Framework Arduino, PlatformIO)  
**Restricción de Auditoría**: 100% Read-Only en código fuente de la aplicación.

---

## 1. Resumen Ejecutivo

Se realizó una auditoría estática exhaustiva de seguridad, robustez de memoria, riesgos de hardware/silicio, temporización y casos límite sobre todo el código fuente del proyecto `debugRobot`.

### Hallazgo Crítico Inmediato:
El proyecto **no compila en su estado actual** debido a incompatibilidades de nombres de archivo no-ASCII (`src/CITÉ.cpp`), símbolos duplicados globales y tipos eliminados (`MAQUINA_ESTADOS`).
Adicionalmente, se detectaron **múltiples fallos críticos de seguridad física y control**:
1. **Fallthrough implícito en la máquina de estados principal** (`src/main.cpp:84`), que anula por completo el control PID y causa transiciones de giro prematuras.
2. **Ley de control PID invertida e incompleta para seguimiento de pared única** (`src/hardware/movimiento/PID.cpp:17-23`), que carece de setpoint y comanda al robot a colisionar directamente contra la pared.
3. **Peligro de bloqueo de arranque por pin de strapping MTDI** (GPIO 12 / `PWMA`), que puede forzar la memoria flash a 1.8V y generar bucles de bootloop.
4. **Pines de entrada flotantes sin pull-up en silicio** (GPIO 34 / `BOTON1`), provocando arranques autónomos no deseados.
5. **Underflow en cálculo de distancias y lectura de sensores ToF**, donde timeouts de hardware son interpretados erróneamente como pasillos abiertos de 1960 mm.

---

## 2. Matriz de Clasificación de Hallazgos

| ID | Severidad | Categoría | Archivo(s) | Línea(s) | Resumen del Problema |
|---|---|---|---|---|---|
| **SEC-01** | **CRÍTICA** | Build / Linker | `src/CITÉ.cpp` | 1-148 | Nombre con tilde UTF-8 rompe el compilador; duplica `setup`, `loop` y globales; usa enum eliminado. |
| **SEC-02** | **CRÍTICA** | Lógica de Control | `src/main.cpp` | 84-88 | Falta sentencia `break;` en `case AVANZANDO:`; caída directa (*fallthrough*) que sobrescribe el PID y altera estados. |
| **SEC-03** | **CRÍTICA** | Algoritmo / Control | `src/hardware/movimiento/PID.cpp` | 17-26 | Ley de control PID para 1 pared carece de setpoint; calcula error absoluto contra 0 mm, forzando colisión. |
| **SEC-04** | **ALTA** | Hardware / Strapping | `src/config.h`, `puenteH.cpp` | `config.h:49`, `puenteH.cpp:14` | GPIO 12 (`PWMA`) es pin strapping MTDI (LDO Flash). Si arranca en HIGH, la flash falla a 1.8V. |
| **SEC-05** | **ALTA** | Hardware / GPIO | `src/config.h`, `src/main.cpp` | `config.h:42`, `main.cpp:25` | GPIO 34 (`BOTON1`) no posee pull-ups internos en ESP32. Flota si no hay pull-up externo, disparando arranque fantasma. |
| **SEC-06** | **ALTA** | Lógica / Navegación | `src/main.cpp` | 71-75 | En avance continuo se resetean encoders y error PID en cada ciclo, deshabilitando odometría y término derivativo $K_d$. |
| **SEC-07** | **ALTA** | Hardware / Bus I2C | `src/hardware/sensoresDistancia/sensoresDistancia.cpp` | 19-23, 88-105 | I2C opera a 10 kHz sin pull-ups externos; saturación bloqueante del bus por omisión de tasa de refresco (20 ms comentado). |
| **SEC-08** | **ALTA** | Sensores / Underflow | `src/hardware/sensoresDistancia/sensoresDistancia.cpp` | 90-103 | Timeouts de sensor VL53L0X (65535) acotados a 2000 mm se interpretan como camino libre; distancias menores a offset dan negativo. |
| **SEC-09** | **ALTA** | Configuración / Giro | `src/config.h`, `src/main.cpp` | `config.h:71`, `main.cpp:152` | `PULSOS_GIRO_180` configurado en 300 (igual que 90°); robot rota la mitad en callejones sin salida. |
| **SEC-10** | **ALTA** | Cinemática / Asimetría | `src/main.cpp` | 89, 104, 120, 135 | Pulsos de Encoder A divididos por 2 arbitrariamente; giros a derecha requieren el doble de pulsos que giros a izquierda. |
| **SEC-11** | **MEDIA** | Disponibilidad / DoS | `src/hardware/sensoresDistancia/sensoresDistancia.cpp` | 43-46, 57-60, 71-74 | Bucle infinito bloqueante `while(true) delay(1000);` si un sensor ToF falla en inicializar, sin reporte de error. |
| **SEC-12** | **MEDIA** | Tipos / Registros HW | `src/hardware/movimiento/puenteH.cpp` | 25-26, 35-36 | `int16_t` con valores negativos pasado a `ledcWrite(duty)` (espera `uint32_t`), produciendo wrap-around a $2^{32}-1$. |
| **SEC-13** | **MEDIA** | Memoria / Heap | `src/hardware/logger/logger.h`, `logger.cpp` | `logger.h:13`, `logger.cpp:11` | Paso por valor de objetos `String` produce constante fragmentación de heap en FreeRTOS; riesgo de Out-Of-Memory. |
| **SEC-14** | **MEDIA** | Buffer / Bluetooth | `src/main.cpp`, `logger.cpp` | `main.cpp:75`, `logger.cpp:11-13` | Envío ininterrumpido de strings Bluetooth en el loop satura el buffer TX de BluetoothSerial y congela la CPU. |
| **SEC-15** | **MEDIA** | Control Motores | `src/main.cpp` | 92, 107, 123, 138, 153 | Se usa `VEL_BASE` (45) en lugar de `VEL_GIRO` (100) en todos los giros; riesgo de parada por fricción estática. |
| **SEC-16** | **MEDIA** | Driver H-Bridge | `src/hardware/movimiento/puenteH.cpp` | 57-65 | `FRENO_F` implementa parada por inercia (*Coast/Hi-Z*), no frenado activo (*Short Brake*), provocando derrape e inercia. |
| **SEC-17** | **BAJA** | Símbolos Huérfanos | `src/hardware/logger/logger.h`, `sensoresDistancia.h`, `puenteH.h` | Múltiples | Declaraciones en headers (`enviarLog`, `leerAccion`, `actualizarSensadoHCSR04`, `actualizarDeltaX`) sin definición en `.cpp`. |
| **SEC-18** | **BAJA** | Edge Case / Discontinuidad | `src/main.cpp` | 63-66 | Comparaciones estrictas (`<` y `>`) sobre 130 mm generan zona muerta cuando la medición es exactamente 130 mm. |

---

## 3. Análisis Detallado de Vulnerabilidades y Fallos

### SEC-01: Falla Crítica de Compilación por Nombre no-ASCII y Archivos Duplicados
- **Ubicación**: `src/CITÉ.cpp` (Líneas 1-148)
- **Evidencia Técnica**:
  Al invocar el compilador oficial de PlatformIO (`pio run`), la herramienta aborta con:
  ```text
  xtensa-esp32-elf-g++: error: src/CIT.cpp: No such file or directory
  xtensa-esp32-elf-g++: fatal error: no input files
  compilation terminated.
  *** [.pio\build\esp32doit-devkit-v1\src\CIT.cpp.o] Error 1
  ```
  La letra acentuada `É` en el nombre de archivo genera incompatibilidad de codificación entre Windows ANSI / UTF-8 y la invocación de `xtensa-esp32-elf-g++`.
  Asimismo, el archivo contiene:
  - Declaración de tipo inválida: `MAQUINA_ESTADOS estado = LISTO;` (línea 16). En `src/main.h`, el enum `MAQUINA_ESTADOS` fue comentado y sustituido por `MAQUINA_NUEVA`.
  - Duplicación de símbolos globales: `sensadoActual`, `velocidadActual`, `estado`, `pulsosActuales`, `setup()` y `loop()`, lo que produciría errores de enlazado múltiple (*multiple definition*).
- **Remediation Recomendada**:
  Mover `src/CITÉ.cpp` fuera del árbol de compilación `src/` (por ejemplo, a una carpeta `archive/` o `test/`), o configurar un filtro en `platformio.ini`:
  ```ini
  src_filter = +<*> -<CIT*.cpp> -<prueba*>
  ```

---

### SEC-02: Fallthrough Implícito en la Máquina de Estados Principal
- **Ubicación**: `src/main.cpp`, líneas 54 a 88
- **Evidencia Técnica**:
  ```cpp
  54:    case AVANZANDO: {
  ...
  84:        estado = GIRANDO_180;
  85:      }
  86:     
  87:    }
  88:
  89:    case  PREGIRO_DER : {
  ```
  Entre la línea 87 y la 88 no existe una sentencia `break;`.
  Cuando el robot se encuentra en el estado `AVANZANDO`:
  1. Calcula la corrección PID y comanda `movimiento(AVANZAR, {velocidadActual.izquierda, velocidadActual.derecha});` (línea 60).
  2. Al no existir `break;`, la ejecución prosigue de inmediato dentro de `case PREGIRO_DER:`.
  3. En `case PREGIRO_DER:`, se evalúa:
     ```cpp
     pulsosActuales = (abs(verPulsosEncoderA())) / 2;
     if (pulsosActuales < PULSOS_PREGIRO_90_DER) {
         movimiento(AVANZAR, {VEL_BASE_IZQ, VEL_BASE_DER});
     }
     ```
     El comando con velocidad corregida por PID calculado en la línea 60 es **instantáneamente sobreescrito en el mismo ciclo de reloj** por `{VEL_BASE_IZQ, VEL_BASE_DER}` sin corrección.
  4. Peor aún: cuando `pulsosActuales` alcanza 300 pulsos, el bloque del *fallthrough* cambia `estado = GIRANDO_DER;`, ejecutando un freno forzado y un giro a la derecha aun cuando el robot debía continuar recto.
- **Remediation Recomendada**:
  Insertar `break;` inmediatamente antes del siguiente caso:
  ```cpp
        } else if (condicionGiro180) {
          enviarString(">>> GIRANDO 180 <<<");
          resetearEncoders();
          estado = GIRANDO_180;
        }
        break; // <-- AGREGAR BREAK
      }

      case PREGIRO_DER: {
  ```

---

### SEC-03: Ley de Control PID de Pared Única Invertida y Carente de Setpoint
- **Ubicación**: `src/hardware/movimiento/PID.cpp`, líneas 17 a 26
- **Evidencia Técnica**:
  ```cpp
  17:    } else if (hayIzq) {
  18:        // Solo pared izquierda: mantenerse a la distancia ideal (OFSET_IZQ representa nuestro objetivo ideal)
  19:        error = - (int16_t)mediciones.distanciaIzq;
  20:    } else if (hayDer) {
  21:        // Solo pared derecha
  22:        error = (int16_t)mediciones.distanciaDer;
  23:    }
  ```
  El comentario indica que `OFSET_IZQ` representa el objetivo ideal. Sin embargo, en el código `OFSET_IZQ` no se resta ni se utiliza.
  1. Si `mediciones.distanciaIzq` es 50 mm (distancia real positiva), `error = -50`.
  2. En `main.cpp:58-59`:
     ```cpp
     velocidadActual.izquierda = constrain(VEL_BASE_IZQ + correccion, 0, 255);
     velocidadActual.derecha   = constrain(VEL_BASE_DER - correccion, 0, 255);
     ```
     Con `correccion < 0`, la rueda izquierda disminuye velocidad y la derecha aumenta: **el robot gira hacia la izquierda, contra la pared**.
  3. No importa qué tan cerca esté el robot de la pared izquierda: mientras `distanciaIzq > 0`, el error siempre será negativo y el robot virará directamente hacia el obstáculo hasta estrellarse.
  4. Al pasar de dos paredes (`error ≈ 0`) a una sola pared (`error = -60`), el término derivativo experimenta un salto brusco $\Delta e = -60$, provocando un golpe violento en los motores.
- **Remediation Recomendada**:
  Calcular el error respecto a la distancia de consigna (*setpoint*):
  ```cpp
  } else if (hayIzq) {
      // Si distanciaIzq > OFSET_IZQ, el robot está lejos: debe virar a la izquierda (corrección < 0)
      // Si distanciaIzq < OFSET_IZQ, el robot está cerca: debe alejarse virando a la derecha (corrección > 0)
      error = (int16_t)OFSET_IZQ - (int16_t)mediciones.distanciaIzq;
  } else if (hayDer) {
      // Si distanciaDer < OFSET_DER, el robot está cerca: debe alejarse virando a la izquierda (corrección < 0)
      // Si distanciaDer > OFSET_DER, el robot está lejos: debe acercarse virando a la derecha (corrección > 0)
      error = (int16_t)mediciones.distanciaDer - (int16_t)OFSET_DER;
  }
  ```

---

### SEC-04: Peligro de Bloqueo de Arranque por Pin de Strapping MTDI (GPIO 12)
- **Ubicación**: `src/config.h:49`, `src/hardware/movimiento/puenteH.cpp:14`
- **Evidencia Técnica**:
  El pin `PWMA` está asignado al GPIO 12.
  En la arquitectura ESP32, GPIO 12 es el pin de strapping `MTDI`. Durante el reset del chip:
  - Si GPIO 12 se muestrea en nivel `ALTO` (3.3V), el regulador interno LDO de la memoria Flash se configura en **1.8V**.
  - Si la placa utiliza memoria Flash externa de 3.3V (estándar en módulos ESP-WROOM-32), el microcontrolador no puede leer el bootloader de la flash, entrando en pánico continuo con el error de ROM:
    ```text
    flash read err, 1000
    rst:0x10 (RTCWDT_RTC_RESET),boot:0x12 (SPI_FAST_FLASH_BOOT)
    ```
  Muchos módulos de puente H (por ejemplo TB6612FNG o placas con optoacopladores/pull-ups) o la capacitancia residual en la línea pueden elevar el voltaje de GPIO 12 durante el reinicio.
- **Remediation Recomendada**:
  1. Reasignar `PWMA` a un pin no reservado (ej. GPIO 13, 2, o 15 con precaución; o intercambiar con pines libres como GPIO 21/22 si se reasignan).
  2. Si no es posible modificar el hardware PCB, quemar permanentemente el eFuse de voltaje de flash usando `espefuse.py`:
     ```bash
     espefuse.py --port COMx set_flash_voltage 3.3V
     ```
  3. Asegurar una resistencia pull-down externa fuerte (4.7 kΩ a GND) en el GPIO 12.

---

### SEC-05: Pin de Entrada Flotante en Entrada de Botón (GPIO 34 / BOTON1)
- **Ubicación**: `src/config.h:42-43`, `src/main.cpp:25`
- **Evidencia Técnica**:
  ```cpp
  #define BOTON1 34
  ...
  pinMode(BOTON1, INPUT);
  ```
  Los pines GPIO 34, 35, 36 y 39 en el ESP32 son pines exclusivamente de entrada (GPI) y **no cuentan con resistencias internas de pull-up ni pull-down en el silicio**.
  Si el diseño esquemático no cuenta con una resistencia física pull-up soldada en placa, el pin queda flotando en alta impedancia. La conmutación de los motores o las ráfagas de transmisión de Bluetooth inducen ruido electromagnético que provoca lecturas falsas `LOW` en `digitalRead(BOTON1)`.
  Esto provoca que el robot inicie su movimiento de avance en `main.cpp:43` en forma espontánea e imprevista al energizarse.
- **Remediation Recomendada**:
  Verificar la existencia de una resistencia externa pull-up de 10 kΩ a 3.3V conectada a GPIO 34. Si no existe, reubicar el botón a un pin con soporte de pull-up interno (GPIO 0-33) y utilizar `pinMode(pin, INPUT_PULLUP)`.

---

### SEC-06: Destrucción de Odometría y Desactivación de Término Derivativo en Avance
- **Ubicación**: `src/main.cpp`, líneas 71 a 75
- **Evidencia Técnica**:
  ```cpp
  71:      } else if (condicionAvanzar) {
  72:        resetearEncoders();
  73:        resetearErrorAnterior();
  74:        estado = AVANZANDO;
  75:        enviarString (">>> AVANZANDO <<<");
  76:      }
  ```
  Durante el avance continuo por un pasillo recto, `condicionAvanzar` se evalúa como verdadera en **cada ciclo del loop**:
  1. `resetearEncoders()` pone la cuenta en cero decenas de veces por segundo. Esto hace imposible utilizar encoders para medir la distancia recorrida en la celda o calcular velocidad real.
  2. `resetearErrorAnterior()` fuerza `errorAnterior = 0`. En la ecuación PID:
     $$\text{correccion} = K_p \cdot e + K_d \cdot (e - e_{\text{ant}})$$
     Al forzar $e_{\text{ant}} = 0$, el término se reduce a $(K_p + K_d) \cdot e$. El término derivativo deja de medir la razón de cambio del error y se transforma en una ganancia proporcional redundante.
  3. `enviarString(">>> AVANZANDO <<<")` satura el puerto serie Bluetooth a máxima tasa.
- **Remediation Recomendada**:
  Eliminar las llamadas a `resetearEncoders()`, `resetearErrorAnterior()` y `enviarString()` de la rama `condicionAvanzar`. Solo deben ejecutarse una única vez al ingresar al estado de avance tras un giro completado.

---

### SEC-07: Bus I2C Degradado a 10 kHz y Tasa de Sensado Comentada
- **Ubicación**: `src/hardware/sensoresDistancia/sensoresDistancia.cpp`, líneas 20-23, 88-105
- **Evidencia Técnica**:
  ```cpp
  gpio_set_pull_mode((gpio_num_t)SDA, GPIO_PULLUP_ONLY);
  gpio_set_pull_mode((gpio_num_t)SCL, GPIO_PULLUP_ONLY);
  Wire.setClock(10000);
  ...
  // if(millis() - ultimoSensado > 20){
  ```
  1. Las resistencias pull-up internas del ESP32 son de ~45 kΩ, extremadamente débiles para I2C. Los tiempos de subida de señal son excesivamente lentos, obligando al desarrollador a reducir la frecuencia a 10 kHz (10 veces más lenta que el estándar de 100 kHz y 40 veces más lenta que Fast Mode 400 kHz).
  2. Con el temporizador de 20 ms comentado, cada llamada a `actualizarSensado()` ejecuta transacciones I2C completas (`readReg`). En `main.cpp`, se llama dos veces por ciclo.
  3. Cada ciclo del loop se bloquea durante 15 a 30 milisegundos en operaciones de I2C, lo que genera una frecuencia de refresco de control de apenas ~30-50 Hz con latencias enormes ante cambios súbitos en la trayectoria del robot.
- **Remediation Recomendada**:
  1. Instalar pull-ups físicos de 2.2 kΩ a 3.3V en las líneas SDA y SCL.
  2. Elevar la velocidad del bus a 400 kHz (`Wire.setClock(400000)`).
  3. Descomentar y respetar el filtro temporal `if (millis() - ultimoSensado >= 20)` para desacoplar el sensado de la frecuencia del bucle principal.
  4. Configurar timeout en Wire (`Wire.setTimeOut(25)`) para prevenir congelamientos permanentes del procesador ante caídas del bus.

---

### SEC-08: Manejo Erróneo de Timeouts de Sensor y Distancias Negativas
- **Ubicación**: `src/hardware/sensoresDistancia/sensoresDistancia.cpp`, líneas 90 a 103
- **Evidencia Técnica**:
  ```cpp
  uint16_t rawIzq = sensorIzq.readRangeContinuousMillimeters();
  if (rawIzq > 2000) rawIzq = 2000;
  lecturaAct.distanciaIzq = rawIzq - OFSET_IZQ;
  ```
  1. Ante una desconexión o falla de lectura, la biblioteca Pololu VL53L0X retorna `65535` si se activa el timeout. El código acota 65535 a 2000 mm, produciendo un valor de `2000 - OFSET_IZQ = 1960 mm`. En el laberinto, 1960 mm supera el umbral de 130 mm (`UMBRAL_PARED_ESTADO_NORMAL`), por lo que una falla o pérdida momentánea del sensor es interpretada por el robot como un **pasillo despejado**, provocando que el robot gire hacia un obstáculo inexistente o una pared sólida.
  2. Si el robot se aproxima a menos de 40 mm de la pared izquierda, `rawIzq` puede ser 20 mm. La operación `20 - 40` produce `-20`. Al ser un número signado negativo, en `main.cpp`:
     `sensadoActual.distanciaCent > UMBRAL_PARED_ESTADO_NORMAL` evalúa `-20 > 130` como falso, pero en otras verificaciones de seguridad de rango puede generar comportamientos no controlados.
- **Remediation Recomendada**:
  Verificar formalmente timeouts y acotar a cero distancias mínimas:
  ```cpp
  if (sensorIzq.timeoutOccurred() || rawIzq > 2000) {
      // Manejar error o mantener lectura previa válida
  } else {
      lecturaAct.distanciaIzq = (rawIzq > OFSET_IZQ) ? (rawIzq - OFSET_IZQ) : 0;
  }
  ```

---

### SEC-09: Configuración Errónea de Pulsos para Giro de 180°
- **Ubicación**: `src/config.h:71`, `src/main.cpp:152`
- **Evidencia Técnica**:
  ```cpp
  #define PULSOS_GIRO_90_DER 300
  #define PULSOS_GIRO_180 300
  ```
  El número de pulsos para girar 180° está definido exactamente igual al de 90° (300 pulsos).
  Al toparse con un callejón sin salida (`condicionGiro180`), el robot transiciona a `GIRANDO_180`, pero solo ejecuta una rotación de 90°. Al finalizar, pasa a `AVANZANDO` apuntando directamente a la pared lateral, produciendo una colisión frontal inmediata.
- **Remediation Recomendada**:
  Configurar `PULSOS_GIRO_180` en aproximadamente el doble de los pulsos de 90° (ej. 600 pulsos, a ajustar en pista).

---

### SEC-10: Asimetría Crítica en Conteo de Pulsos de Encoders
- **Ubicación**: `src/main.cpp`, líneas 89, 104, 120, 135
- **Evidencia Técnica**:
  - `PREGIRO_DER`: `pulsosActuales = (abs(verPulsosEncoderA())) / 2;` (dividido por 2)
  - `PREGIRO_IZQ`: `pulsosActuales = abs(verPulsosEncoderB());` (sin división)
  - `GIRANDO_DER`: `pulsosActuales = (abs(verPulsosEncoderA())) / 2;` (dividido por 2)
  - `GIRANDO_IZQ`: `pulsosActuales = abs(verPulsosEncoderB());` (sin división)
  Para que `pulsosActuales` alcance 300 en giros a derecha, `verPulsosEncoderA()` debe acumular **600 pulsos físicos**.
  En contraste, para giros a izquierda, solo se requieren 280 pulsos físicos.
  El robot avanzará y rotará el doble en maniobras hacia la derecha que hacia la izquierda.
- **Remediation Recomendada**:
  Eliminar la división por 2 en el Encoder A, o unificar ambas ruedas calculando el promedio:
  `pulsosActuales = (abs(verPulsosEncoderA()) + abs(verPulsosEncoderB())) / 2;`

---

### SEC-11: Bucle Infinito Bloqueante ante Falla de Inicialización de Sensores
- **Ubicación**: `src/hardware/sensoresDistancia/sensoresDistancia.cpp`, líneas 45, 59, 73
- **Evidencia Técnica**:
  ```cpp
  if (!sensorDer.init()) {
    Serial.printf("ERROR: fallo init sensor en pin %d\n", xshutPinDer);
    while (true) delay(1000);
  }
  ```
  Si un sensor falla, el procesador se detiene en un bucle infinito en `setup()`.
  No se devuelve código de error, no se notifica vía Bluetooth ni LED, y el sistema queda congelado. En entornos con Task Watchdog activo en FreeRTOS, puede disparar reinicios periódicos.
- **Remediation Recomendada**:
  Hacer que `inicializacionSensoresDist()` retorne un booleano de éxito/falla, implementar reintentos (hasta 3) y alertar acústica o visualmente si la inicialización no es exitosa.

---

### SEC-12: Wrap-around de Entero Negativo a Registro de Hardware PWM (32-bit Unsigned)
- **Ubicación**: `src/hardware/movimiento/puenteH.cpp`, líneas 25-26, 35-36, 44-45, 53-54
- **Evidencia Técnica**:
  `struct VELOCIDAD { int16_t izquierda; int16_t derecha; };`
  En la función `movimiento`:
  `ledcWrite(0, velocidad.izquierda);`
  La API nativa de ESP32 `ledcWrite` recibe `uint32_t duty`.
  Si por error de cálculo o comando externo se recibe un valor negativo (ej. `-1`), el casting implícito lo convierte a `4294967295`. En un canal configurado a resolución de 8 bits (0-255), esto puede generar corrupción de registro o ciclo de trabajo imprevisible.
- **Remediation Recomendada**:
  Proteger la función `movimiento` restringiendo estrictamente los valores de entrada:
  ```cpp
  uint32_t pwmIzq = constrain((int)velocidad.izquierda, 0, 255);
  uint32_t pwmDer = constrain((int)velocidad.derecha, 0, 255);
  ledcWrite(0, pwmIzq);
  ledcWrite(1, pwmDer);
  ```

---

### SEC-13: Fragmentación de Memoria Heap por Paso por Valor de `String`
- **Ubicación**: `src/hardware/logger/logger.h:13`, `logger.cpp:11`
- **Evidencia Técnica**:
  `void enviarString(String str)`
  En cada invocación, se reserva memoria dinámica en el heap para duplicar el objeto `String` y luego liberarlo.
  En microcontroladores con FreeRTOS y conexión Bluetooth activa, las continuas asignaciones y liberaciones de pequeños bloques de memoria producen fragmentación del heap, elevando el riesgo de fallas por memoria insuficiente (*Heap Exhaustion*).
- **Remediation Recomendada**:
  Pasar por referencia constante o usar punteros C-string:
  ```cpp
  void enviarString(const String& str);
  // O preferentemente:
  void enviarString(const char* str);
  ```

---

### SEC-14: Saturación de Buffer de Transmisión Bluetooth y Bloqueo de CPU
- **Ubicación**: `src/main.cpp:75`, `src/hardware/logger/logger.cpp:11-13`
- **Evidencia Técnica**:
  En `main.cpp`, dentro de `case AVANZANDO:`, se invoca `enviarString(">>> AVANZANDO <<<");` en cada iteración.
  El buffer de transmisión de BluetoothSerial en ESP-IDF posee un tamaño acotado (típicamente 512 bytes). A cientos de ejecuciones por segundo, el buffer se llena en menos de 100 ms. A partir de ese momento, `SerialBT.println()` se bloquea esperando espacio libre o descarta paquetes con alta penalización de tiempo de ejecución, introduciendo *jitter* en el lazo de control de los motores.
- **Remediation Recomendada**:
  Emitir mensajes de log exclusivamente ante eventos de transición de estado o limitados mediante temporizadores no bloqueantes (`millis() - ultimoLog >= 500`).

---

### SEC-15: Velocidad de Giro Insuficiente (`VEL_BASE` en Lugar de `VEL_GIRO`)
- **Ubicación**: `src/main.cpp`, líneas 92, 107, 123, 138, 153
- **Evidencia Técnica**:
  En `config.h` se definen:
  `#define VEL_BASE_DER 45` y `#define VEL_GIRO_DER 100`.
  Sin embargo, en `main.cpp` los estados de rotación ejecutan:
  `movimiento(GIRAR_DER, {VEL_BASE_IZQ, VEL_BASE_DER});` (con valor 45).
  Un PWM de 45 sobre 255 (17.6%) suele ser incapaz de vencer el rozamiento estático de los neumáticos al girar sobre su propio eje. El robot queda atascado con los motores zumbando sin girar, esperando pulsos de encoder que nunca se generan.
- **Remediation Recomendada**:
  Utilizar `{VEL_GIRO_IZQ, VEL_GIRO_DER}` en los estados `GIRANDO_DER`, `GIRANDO_IZQ` y `GIRANDO_180`.

---

### SEC-16: Frenado Pasivo en Lugar de Frenado Activo en Puente H
- **Ubicación**: `src/hardware/movimiento/puenteH.cpp`, líneas 57-65
- **Evidencia Técnica**:
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
  En circuitos integrados de puente H estándar (como TB6612FNG), `IN1=LOW, IN2=LOW` activa el modo *Coast* (rueda libre/alta impedancia). El robot continúa desplazándose por inercia física.
  Para lograr frenado dinámico activo (*Short Brake*), ambas entradas deben ponerse en `HIGH` (`IN1=HIGH, IN2=HIGH`), cortocircuitando los bobinados del motor para inducir frenado contra-electromotriz inmediato.
- **Remediation Recomendada**:
  Implementar frenado dinámico colocando entradas en nivel alto o documentar la distinción entre parada suave y freno activo.

---

### SEC-17: Funciones Declaradas en Headers sin Definición en Código Fuente
- **Ubicación**:
  - `src/hardware/logger/logger.h:11, 15`: `void enviarLog(char* mensaje);`, `uint8_t leerAccion();`
  - `src/hardware/sensoresDistancia/sensoresDistancia.h:26, 31`: `void inicializacionSensoresHCSR04();`, `sensado actualizarSensadoHCSR04();`
  - `src/hardware/movimiento/puenteH.h:19`: `bool actualizarDeltaX();`
- **Evidencia Técnica**:
  Dichas funciones están expuestas públicamente en los archivos de cabecera pero no existen en ningún archivo de implementación `.cpp`.
  Cualquier intento de invocación por parte de módulos presentes o futuros generará un error de enlazador `undefined reference to ...`.
- **Remediation Recomendada**:
  Implementar las funciones requeridas o depurar las cabeceras eliminando prototipos obsoletos.

---

### SEC-18: Zonas Muertas en Condiciones de Detección de Paredes
- **Ubicación**: `src/main.cpp`, líneas 63 a 66
- **Evidencia Técnica**:
  ```cpp
  bool condicionGiroDer = sensadoActual.distanciaDer > UMBRAL_PARED_ESTADO_NORMAL;
  bool condicionAvanzar = sensadoActual.distanciaDer < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaCent > UMBRAL_PARED_ESTADO_NORMAL;
  bool condicionGiroIzq = sensadoActual.distanciaDer < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaCent < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaIzq > UMBRAL_PARED_ESTADO_NORMAL;
  bool condicionGiro180 = sensadoActual.distanciaDer < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaCent < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaIzq < UMBRAL_PARED_ESTADO_NORMAL;
  ```
  Si `distanciaDer == 130` (`UMBRAL_PARED_ESTADO_NORMAL`), ninguna de las 4 condiciones se cumple (tanto `> 130` como `< 130` evalúan a `false`).
  El robot permanece indefinidamente en su estado previo sin ejecutar ninguna transición ni acción correctiva.
- **Remediation Recomendada**:
  Utilizar operadores inclusivos (ej. `<=`) y agregar una rama `else` por defecto para prevenir estados indeterminados.

---

## 4. Análisis de Archivos Secundarios y Scripts de Prueba

1. `src/pruebaEncoders`:
   - Archivo C++ sin extensión ubicado en `src/`. Define duplicados de `setup`, `loop`, `sensadoActual`, `velocidadActual`.
   - No detiene los motores una vez alcanzado el objetivo de pulsos.
2. `src/pruebaMotoresAislados.txt`, `src/test.txt`, `src/testearHardware.txt`:
   - Contienen `#include "hardware/sensorPiso/sensorPiso.h"` y `#include "memoria/funcionesMapeo.h"`, módulos inexistentes en el repositorio.
   - Utilizan constantes no definidas como `BOTON` (en vez de `BOTON1`), `ESTADOS`, `PULSOS_AVANZAR_BLOQUEANTE`.
   - Dejan evidencia de parches anteriores ("evadir el bug de 50 en puenteH").
3. `src/bluetooth.txt`:
   - `delay(5000)` bloquea el microcontrolador durante 5 segundos completos con motores en marcha, impidiendo cualquier parada de emergencia.
   - `SerialBT.readStringUntil('#')` sin límite de longitud es vulnerable a agotamiento de memoria ante tramas corruptas.

---

## 5. Plan de Remediación Priorizado

### Fase 1: Desbloqueo de Compilación y Seguridad de Hardware (Inmediato)
1. Remover o archivar `src/CITÉ.cpp` y `src/pruebaEncoders` fuera de `src/`.
2. Verificar pull-up de hardware en GPIO 34 (`BOTON1`) y revisar la línea de GPIO 12 (`PWMA`) para evitar bootloops por strapping.
3. Incorporar resistencias pull-up externas en SDA/SCL (I2C) y configurar el bus a 400 kHz.

### Fase 2: Corrección de la Máquina de Estados y Control de Movimiento (Crítico)
1. Agregar `break;` en `main.cpp:87` para subsanar el *fallthrough* mortal en `AVANZANDO`.
2. Corregir la ley de control PID en `PID.cpp` incorporando consignas (*setpoints*) en seguimiento de pared única.
3. Eliminar los reseteos continuos de encoders y del error derivativo dentro de la rama de avance en `main.cpp`.
4. Asignar `PULSOS_GIRO_180 ≈ 600` en `config.h` y utilizar `VEL_GIRO` en lugar de `VEL_BASE` para las maniobras de giro.
5. Corregir la asimetría de los encoders eliminando la división espuria `/ 2` en el canal A.

### Fase 3: Robustez de Software, Memoria y Manejo de Errores (Defensivo)
1. Validar timeouts en sensores VL53L0X para evitar interpretar fallas de hardware como caminos abiertos.
2. Limitar mensajes de Bluetooth a eventos de cambio de estado para evitar la saturación del buffer TX.
3. Cambiar `enviarString` para recibir `const char*` o `const String&` para eliminar fragmentación de heap.
4. Acotar entradas de velocidad en `puenteH.cpp` para evitar wrap-around a enteros sin signo de 32 bits en registros PWM.
