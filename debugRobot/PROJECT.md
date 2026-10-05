# Documentación Técnica del Proyecto: debugRobot (MicroMouse)

## 1. Visión General del Sistema

**debugRobot** es el firmware embebido para un robot móvil autónomo diseñado para la resolución de laberintos (categoría MicroMouse / IEEE Micromouse). El sistema navega a través de una cuadrícula ortogonal de celdas delimitadas por paredes de 18 cm x 18 cm, utilizando tres sensores de distancia ópticos Time-of-Flight (ToF) y odometría de cuadratura en dos ruedas motrices para centrado y navegación en tiempo real.

- **Microcontrolador Central:** Espressif ESP32 DevKit V1 (Xtensa LX6 32-bit Dual-Core @ 240 MHz, 320 KB SRAM, 4 MB Flash SPI).
- **Entorno de Compilación:** PlatformIO Core / Arduino Core for ESP32 (`framework-arduinoespressif32`).
- **Arquitectura de Software:** Máquina de estados finitos (FSM) no apropiativa, lazo de control proporcional-derivativo (PID) en avance, y lectura I2C secuencial multiplexada por pines `XSHUT`.

---

## 2. Mapa Completo de Pines de Hardware (Pinout)

La asignación de pines del ESP32 está centralizada en `src/config.h`:

| Periférico / Función | Etiqueta de Código | Pin ESP32 (GPIO) | Tipo de E/S | Notas Eléctricas / Restricciones |
|---|---|---|---|---|
| **Botón de Inicio 1** | `BOTON1` | GPIO 34 | Entrada Digital (GPI) | **¡ADVERTENCIA!** Entrada pura sin pull-up interno. Requiere pull-up físico externo (10 kΩ a 3.3V). |
| **Botón Auxiliar 2** | `BOTON2` | GPIO 35 | Entrada Digital (GPI) | Entrada pura sin resistencias de pull-up internas. |
| **Sensor Piso / Reflectivo**| `CNY` | GPIO 13 | Entrada/Salida | Sensor infrarrojo de detección de piso. |
| **Driver Motor A (Dir 1)** | `AIN1` | GPIO 14 | Salida Digital | Control de dirección Motor A (Izquierdo). |
| **Driver Motor A (Dir 2)** | `AIN2` | GPIO 4 | Salida Digital | Control de dirección Motor A (Izquierdo). |
| **Driver Motor B (Dir 1)** | `BIN1` | GPIO 16 | Salida Digital | Control de dirección Motor B (Derecho). |
| **Driver Motor B (Dir 2)** | `BIN2` | GPIO 17 | Salida Digital | Control de dirección Motor B (Derecho). |
| **PWM Motor A (Izquierdo)**| `PWMA` | GPIO 12 | Salida PWM (LEDC Ch 0) | **¡ADVERTENCIA CRÍTICA!** Pin de Strapping `MTDI`. Si está en HIGH al bootear, fuerza flash a 1.8V (bootloop). |
| **PWM Motor B (Derecho)**  | `PWMB` | GPIO 32 | Salida PWM (LEDC Ch 1) | Salida PWM segura de propósito general. |
| **Encoder A (Canal 1)**    | `ENC_A_1` | GPIO 33 | Entrada de Pulsos | Cuadratura Motor A (Usa periférico ESP32 PCNT). |
| **Encoder A (Canal 2)**    | `ENC_B_1` | GPIO 25 | Entrada de Pulsos | Cuadratura Motor A (Usa periférico ESP32 PCNT). |
| **Encoder B (Canal 1)**    | `ENC_A_2` | GPIO 26 | Entrada de Pulsos | Cuadratura Motor B (Usa periférico ESP32 PCNT). |
| **Encoder B (Canal 2)**    | `ENC_B_2` | GPIO 27 | Entrada de Pulsos | Cuadratura Motor B (Usa periférico ESP32 PCNT). |
| **I2C SDA (Datos)**        | `SDA` | GPIO 21 (Defecto) | Bidireccional I2C | Bus I2C compartido para los 3 sensores VL53L0X. Requiere pull-ups externos de 2.2k-4.7kΩ. |
| **I2C SCL (Reloj)**        | `SCL` | GPIO 22 (Defecto) | Salida Reloj I2C | Reloj I2C compartido. |
| **XSHUT Sensor Derecho**   | `xshutPinDer` | GPIO 18 | Salida Digital | Reset para reprogramación de dirección I2C (0x30). |
| **XSHUT Sensor Central**   | `xshutPinCent`| GPIO 19 | Salida Digital | Reset para reprogramación de dirección I2C (0x32). |
| **XSHUT Sensor Izquierdo** | `xshutPinIzq` | GPIO 23 | Salida Digital | Reset para reprogramación de dirección I2C (0x31). |

---

## 3. Arquitectura del Software y Estructura Modular

El árbol de directorios del proyecto se organiza de forma modular:

```text
debugRobot/
├── platformio.ini              # Configuración de compilación, microcontrolador y dependencias
├── bug_report.md               # Informe maestro de auditoría y catálogo de 34 defectos
├── PROJECT.md                  # Especificación de arquitectura, hardware y módulos (este archivo)
├── src/
│   ├── main.h                  # Definición de enumeraciones de la FSM (MAQUINA_NUEVA)
│   ├── main.cpp                # Punto de entrada, lazo loop(), FSM y control de navegación
│   ├── config.h                # Constantes de configuración, calibración, PID y mapa de pines
│   ├── CITÉ.cpp                # [OBSOLETO / BLOQUEANTE] Archivo de prueba con nombre no-ASCII
│   ├── hardware/
│   │   ├── encoders/
│   │   │   ├── encoders.h      # Interfaz de lectura y reseteo de encoders
│   │   │   └── encoders.cpp    # Driver de hardware PCNT usando madhephaestus/ESP32Encoder
│   │   ├── logger/
│   │   │   ├── logger.h        # Interfaz de telemetría y logging
│   │   │   └── logger.cpp      # Driver de BluetoothSerial ("Manati") y UART 115200
│   │   ├── movimiento/
│   │   │   ├── PID.h           # Interfaz del algoritmo de corrección de trayectoria
│   │   │   ├── PID.cpp         # Implementación de la ley de control Proporcional-Derivativa
│   │   │   ├── puenteH.h       # Primitivas de movimiento y enumeración MOVIMIENTOS
│   │   │   └── puenteH.cpp     # Generación de PWM por LEDC y control digital de dirección
│   │   └── sensoresDistancia/
│   │       ├── sensoresDistancia.h   # Estructura sensado y prototipos de lectura
│   │       └── sensoresDistancia.cpp # Driver de 3x VL53L0X con gestión XSHUT sobre I2C
│   └── [scripts de prueba .txt]# Archivos de ensayo manual y diagnóstico descartables
```

### 3.1 Descripción de Módulos Principales

1. **Controlador de Supervisión (`src/main.cpp`):**
   - Implementa `setup()` y `loop()`.
   - Gestiona la Máquina de Estados Finitos (`LISTO`, `AVANZANDO`, `PREGIRO_DER`, `PREGIRO_IZQ`, `GIRANDO_DER`, `GIRANDO_IZQ`, `GIRANDO_180`).
   - Muestrea sensores, calcula el error de centrado llamando a `PID.cpp` y actualiza la velocidad de los motores.
2. **Subsistema de Percepción ToF (`src/hardware/sensoresDistancia/`):**
   - Controla 3 sensores ópticos de tiempo de vuelo VL53L0X (Pololu).
   - Reasigna dinámicamente las direcciones I2C en arranque encendiendo de a uno los pines `XSHUT`:
     - Sensor Derecho: `0x30`
     - Sensor Izquierdo: `0x31`
     - Sensor Central: `0x32`
   - Retorna la estructura `sensado { int16_t distanciaCent; int16_t distanciaDer; int16_t distanciaIzq; }`.
3. **Subsistema de Control PID (`src/hardware/movimiento/PID.cpp`):**
   - Recibe la estructura de mediciones y computa la corrección diferencial $K_p \cdot e + K_d \cdot \Delta e$.
   - Aplica saturación simétrica al valor de corrección para modular las velocidades de las ruedas (`VEL_BASE_IZQ + correccion`, `VEL_BASE_DER - correccion`).
4. **Subsistema de Tracción y Potencia (`src/hardware/movimiento/puenteH.cpp`):**
   - Configura dos canales de hardware LEDC a 20 kHz y resolución de 8 bits (0–255 duty cycle).
   - Comanda las entradas de puente H (`AIN1`, `AIN2`, `BIN1`, `BIN2`) para `AVANZAR`, `RETROCEDER`, `GIRAR_DER`, `GIRAR_IZQ` y `FRENO_F`.
5. **Subsistema de Odometría (`src/hardware/encoders/`):**
   - Utiliza la librería `ESP32Encoder` vinculada al hardware PCNT del ESP32.
   - Lee la rotación en cuadratura de 4x en ambas ruedas de forma no bloqueante y gestiona el blanqueo de cuentas (`clearCount()`).
6. **Subsistema de Telemetría (`src/hardware/logger/`):**
   - Administra el canal Bluetooth clásico SPP bajo el nombre *"Manati"* para diagnóstico en tiempo real.

---

## 4. Dinámica del Lazo de Control y FSM

```text
               ┌───────────────────────┐
               │         LISTO         │
               └──────────┬────────────┘
                          │ Pulsador Presionado (BOTON1 == LOW)
                          ▼
               ┌───────────────────────┐◄──────────────────────────────────┐
               │       AVANZANDO       │                                   │
               │ (Corrección por PID)  │                                   │
               └──────────┬────────────┘                                   │
                          │                                                │
         ┌────────────────┼────────────────┬───────────────┐               │
         │ Der libre      │ Frente cerrado │ 3 paredes     │ Camino libre  │
         │ (Giro Der)     │ (Giro Izq)     │ (Giro 180)    │               │
         ▼                ▼                ▼               ▼               │
   ┌───────────┐    ┌───────────┐    ┌───────────┐   (Seguir Avanzando)    │
   │PREGIRO_DER│    │PREGIRO_IZQ│    │GIRANDO_180│                         │
   └─────┬─────┘    └─────┬─────┘    └─────┬─────┘                         │
         │ Ticks A        │ Ticks B        │ Ticks A                       │
         ▼                ▼                │                               │
   ┌───────────┐    ┌───────────┐          │                               │
   │GIRANDO_DER│    │GIRANDO_IZQ│          │                               │
   └─────┬─────┘    └─────┬─────┘          │                               │
         │ 90° Giro       │ 90° Giro       │ 180° Giro                     │
         └────────────────┴────────────────┴───────────────────────────────┘
```

---

## 5. Resumen de Defectos Críticos Auditados

Una auditoría integral e interdisciplinaria (incluyendo revisión estática, dinámica y desafío adversarial) identificó **34 defectos esenciales** documentados exhaustivamente en `bug_report.md`.

### 5.1 Matriz de Distribución de Defectos por Subsistema y Severidad

| Subsistema / Dominio | Crítica | Alta | Media | Baja | Total |
|---|:---:|:---:|:---:|:---:|:---:|
| **Build / Toolchain & Particiones** | BUG-01, BUG-02, BUG-03 | BUG-04, BUG-32 | — | — | **5** |
| **Arquitectura de Software & Headers** | — | BUG-05 | — | BUG-06 | **2** |
| **Control de Flujo & Máquina de Estados (FSM)** | BUG-07 | BUG-10, BUG-12, BUG-30 | BUG-11 | — | **5** |
| **Cinemática & Odometría** | BUG-08, BUG-13, BUG-14 | BUG-15 | — | — | **4** |
| **Algoritmo de Control PID** | BUG-16 | BUG-09, BUG-17, BUG-31 | BUG-18 | — | **5** |
| **Accionamiento de Motores & Puente H** | BUG-19 | — | BUG-20, BUG-21, BUG-22, BUG-33 | — | **5** |
| **Percepción Time-of-Flight (I2C / VL53L0X)** | — | BUG-23, BUG-24, BUG-26 | BUG-25, BUG-27 | — | **5** |
| **Hardware, Strapping Pins & Entradas** | — | BUG-28, BUG-29 | — | — | **2** |
| **Telemetría, Memoria & Observabilidad** | — | — | BUG-34 | — | **1** |
| **TOTAL GENERAL** | **9** | **15** | **9** | **1** | **34** |

### 5.2 Fallos Arquitectónicos y Dinámicos de Mayor Impacto

1. **Bloqueo Fatal de Compilación (BUG-01, BUG-02, BUG-03):** `src/CITÉ.cpp` impide la compilación por caracteres no-ASCII y colisión de símbolos con `main.cpp`.
2. **Caída por Falta de `break;` (BUG-07):** El estado `AVANZANDO` cae incondicionalmente a `PREGIRO_DER`, sobreescribiendo el PID e induciendo giros involuntarios.
3. **Ausencia de Setpoint y Error de Signo en PID (BUG-16):** La ley de pared única carece de consigna de centrado y presentaba signos invertidos, forzando al robot a virar hacia la pared hasta impactar.
4. **Asimetría en Encoders (BUG-13) y Error de 180° (BUG-14):** Giro derecho dividido por 2 duplica la rotación real; giro de 180° configurado con pulsos idénticos a 90° (`300` pulsos).
5. **Inversión Cinemática de Motores (BUG-19):** Las polaridades de giro sobre el eje en `puenteH.cpp` están cruzadas respecto a la cinemática diferencial.
6. **Riesgos de Silicio (BUG-28, BUG-29):** GPIO 12 (`PWMA`) es pin strapping MTDI (peligro de flash a 1.8V); GPIO 34 (`BOTON1`) carece de pull-up interno.
7. **Lecturas Nulas Iniciales y Timeouts (BUG-23, BUG-26):** Sensado arranca en `{0,0,0}` disparando giros de 180° al iniciar; timeouts de VL53L0X se transforman en pasillos abiertos de 1960 mm.
8. **Omisión de Umbral Frontal y Discrepancia FSM vs PID (BUG-30, BUG-31):** `UMBRAL_PARED_FRENTE` (120 mm) queda inerte en `main.cpp` reemplazado por 130 mm; discrepancia de 49 mm entre FSM (130 mm) y PID (180 mm) induce virajes violentos en pasillos contiguos y cruces abiertos.
9. **Saturación de Memoria Flash (BUG-32):** BluetoothSerial consume el 86.7% de `app0` (1.13 MB de 1.25 MB); requiere partición `huge_app.csv` en `platformio.ini` para habilitar el espacio de algoritmos MicroMouse (FloodFill).
10. **Seguridad en Actuadores y Observabilidad UART (BUG-33, BUG-34):** Ausencia de `default:` en `puenteH.cpp` y omisión de puesta a nivel bajo / PWM cero en `inicializarMotores()`; inobservabilidad por UART en `main.cpp` manteniendo la consola USB de PlatformIO silente.

Para acceder al detalle exhaustivo de ubicación, análisis causa-raíz y parches de código recomendados para cada uno de los 34 defectos, remitirse a:  
👉 **`bug_report.md`** (en la raíz del proyecto).
