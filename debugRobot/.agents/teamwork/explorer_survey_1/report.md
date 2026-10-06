# Reporte Técnico de Relevamiento del Código Base (debugRobot / MicroMouse)

**Agente:** Explorer 1 (`teamwork_preview_explorer`)  
**Fecha:** 2026-10-06  
**Ruta de Trabajo:** `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\.agents\teamwork\explorer_survey_1`  
**Directorio Raíz Analizado:** `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot`  

---

## 1. Resumen Ejecutivo

Se realizó una auditoría y relevamiento exhaustivo del firmware del robot MicroMouse (`debugRobot`). El sistema actual implementa una máquina de estados finitos (FSM) reactiva en `src/main.cpp`, gobernada por odometría de cuadratura (PCNT hardware) y tres sensores Time-of-Flight VL53L0X.

Actualmente:
- El robot avanza una celda completa fija comandado estrictamente por **800 pulsos** de odometría promedio (`PULSOS_CELDA`).
- Tras completar los 800 pulsos, ingresa al estado `FRENANDO` (pausa estabilizadora de 150 ms) y luego al estado `DECISION`, donde aplica la regla de la mano derecha con umbrales fijos de 130 mm.
- **R1 (Reseteo por flanco lateral)** NO está implementado: no existe lógica de detección de transición `< 130 mm` a `> 130 mm`, ni ajuste del objetivo a 400 pulsos.
- **R2 (Alineación con pared frontal a 50 mm)** NO está implementado: el sensor frontal no interviene en la condición de parada del estado `AVANZANDO`.
- **R3 (Gracia post-giro / Ceguera de PID durante 100 pulsos)** NO está implementado: la corrección PID actúa de inmediato desde el pulso 0.
- **R4 (Ausencia de mapeo y preservación de `FRENANDO`)** se cumple en el diseño existente: no existen matrices ni tracking cartesiano, y el estado `FRENANDO` está correctamente integrado.

A continuación se detalla la arquitectura, el mapeo de hardware y el plan analítico para implementar R1–R4 sin romper la estabilidad del sistema.

---

## 2. Configuración de Compilación y Entorno (PlatformIO)

El archivo `platformio.ini` define:
```ini
[env:esp32doit-devkit-v1]
platform = espressif32
board = esp32doit-devkit-v1
framework = arduino
lib_deps =
    VL53L0X
    madhephaestus/ESP32Encoder @ ^0.11.7
monitor_speed = 115200
```

### Estructura de Archivos en `src/`:
- `src/main.h`: Enumeración de estados activos `MAQUINA_ESTADOS`.
- `src/main.cpp`: Lazo principal `setup()` y `loop()`, máquina de estados y control de trayectoria.
- `src/config.h`: Constantes globales, calibraciones de sensores, pines GPIO y umbrales.
- `src/hardware/encoders/`: Driver de cuadratura con `ESP32Encoder` (`encoders.h`, `encoders.cpp`).
- `src/hardware/sensoresDistancia/`: Driver de 3x VL53L0X con multiplexación XSHUT (`sensoresDistancia.h`, `sensoresDistancia.cpp`).
- `src/hardware/movimiento/`:
  - `PID.h` / `PID.cpp`: Controlador proporcional-derivativo con saturación.
  - `puenteH.h` / `puenteH.cpp`: Control de puente H por PWM LEDC y pines digitales.
- `src/hardware/logger/`: Comunicación Bluetooth Serial ("Manati") y UART 115200 (`logger.h`, `logger.cpp`).

---

## 3. Mapeo Detallado de la Máquina de Estados (FSM)

### 3.1 Estados Definidos (`MAQUINA_ESTADOS` en `src/main.h`)
```cpp
enum MAQUINA_ESTADOS {
    LISTO,
    INFORMACION_RECIBIDA,
    AVANZANDO,
    DECISION,
    GIRANDO_DER,
    GIRANDO_IZQ,
    GIRANDO_180,
    FRENANDO
};
```

### 3.2 Lógica y Transiciones en `src/main.cpp`

```text
                  ┌───────────────────────┐
                  │         LISTO         │
                  └───────────┬───────────┘
                              │ Pulsador (BOTON1 == LOW)
                              ▼
            ┌─────────► ┌───────────┐
            │           │ AVANZANDO │
            │           └─────┬─────┘
            │                 │ pulsosActuales >= PULSOS_CELDA (800)
            │                 ▼
            │           ┌───────────┐
            │           │ FRENANDO  │ (Espera millis() >= 150 ms)
            │           └─────┬─────┘
            │                 │ estadoPostFreno == DECISION
            │                 ▼
            │           ┌───────────┐
            │           │ DECISION  │
            │           └─────┬─────┘
            │                 │
            │   ┌─────────────┼─────────────┬─────────────┐
            │   │ Der libre   │ Frente libre│ Izq libre   │ Callejón sin salida
            │   ▼             ▼             ▼             ▼
            │ ┌───────────┐ (Avanza)  ┌───────────┐ ┌───────────┐
            │ │GIRANDO_DER│   │       │GIRANDO_IZQ│ │GIRANDO_180│
            │ └─────┬─────┘   │       └─────┬─────┘ └─────┬─────┘
            │       │         │             │             │
            │       └─────────┼─────────────┴─────────────┘
            │                 │ Fin de giro (pulsos de giro alcanzados)
            │                 ▼
            │           ┌───────────┐
            └───────────┤ FRENANDO  │ (estadoPostFreno = AVANZANDO)
                        └───────────┘
```

#### Descripción Estado por Estado:
1. **`LISTO` (Líneas 47–64):**
   - Mantiene frenado pasivo `movimiento(FRENO_F, {0,0})`.
   - Si `hayDatosBT()` es verdadero, conmuta a `INFORMACION_RECIBIDA`.
   - Si `digitalRead(BOTON1) == LOW`, espera a soltar el botón (`while(digitalRead(BOTON1)==LOW) delay(10);`), resetea encoders y el término derivativo del PID (`resetearErrorAnterior()`), y pasa a `AVANZANDO`.

2. **`INFORMACION_RECIBIDA` (Líneas 66–70):**
   - Ejecuta `procesarTramaBT(...)` para recibir vía Bluetooth ajustes dinámicos de pulsos de giro (`PULSOS_GIRO_90_DER` y `PULSOS_GIRO_90_IZQ`).
   - Retorna a `LISTO`.

3. **`AVANZANDO` (Líneas 72–93):**
   - Calcula odometría promedio: `pulsosActuales = (abs(verPulsosEncoderA()) + abs(verPulsosEncoderB())) / 2;`.
   - **Rama activa (`pulsosActuales < PULSOS_CELDA` = 800):**
     - Muestrea sensores: `sensadoActual = actualizarSensado();`.
     - Calcula corrección PID: `int16_t correccion = calcularCorreccion(sensadoActual);`.
     - Ajusta velocidades con compensación de montaje físico invertido:
       - `velocidadActual.izquierda = constrain(VEL_BASE_DER - correccion, 0, 255);`
       - `velocidadActual.derecha = constrain(VEL_BASE_IZQ + correccion, 0, 255);`
     - Envía comando `movimiento(AVANZAR, {velocidadActual.izquierda, velocidadActual.derecha});`.
   - **Rama de finalización (`pulsosActuales >= PULSOS_CELDA`):**
     - Aplica `movimiento(FRENO_F, {0,0})`.
     - Guarda marca de tiempo: `tiempoInicioFreno = millis();`.
     - Configura `estadoPostFreno = DECISION;`.
     - Transiciona a `FRENANDO`.

4. **`FRENANDO` (Líneas 170–179):**
   - Mantiene activo el puente H en corte: `movimiento(FRENO_F, {0,0})`.
   - Condición de salida no bloqueante: `if (millis() - tiempoInicioFreno >= 150)`.
   - Al expirar los 150 ms:
     - `resetearErrorAnterior();`
     - `resetearEncoders();`
     - Transiciona a `estadoPostFreno` (`DECISION` tras avanzar, o `AVANZANDO` tras girar).

5. **`DECISION` (Líneas 95–124):**
   - Actualiza lecturas: `sensadoActual = actualizarSensado();`.
   - Evalúa prioridades (Regla de la Mano Derecha) con `UMBRAL_PARED_ESTADO_NORMAL` (130 mm):
     1. Si `distanciaDer > 130`: `resetearEncoders()`, pasa a `GIRANDO_DER`.
     2. Si `distanciaDer <= 130` y `distanciaCent > 130`: `resetearEncoders()`, `resetearErrorAnterior()`, pasa a `AVANZANDO`.
     3. Si `distanciaDer <= 130`, `distanciaCent <= 130` y `distanciaIzq > 130`: `resetearEncoders()`, pasa a `GIRANDO_IZQ`.
     4. Si las 3 paredes están cerradas (`<= 130`): `resetearEncoders()`, pasa a `GIRANDO_180`.

6. **`GIRANDO_DER` (Líneas 126–139):**
   - Mide pulsos de rueda externa: `pulsosActuales = abs(verPulsosEncoderA());`.
   - Mientras `pulsosActuales < PULSOS_GIRO_90_DER` (300): `movimiento(GIRAR_DER, {VEL_BASE_IZQ, VEL_BASE_DER});`.
   - Al finalizar: freno, `tiempoInicioFreno = millis()`, `estadoPostFreno = AVANZANDO`, pasa a `FRENANDO`.

7. **`GIRANDO_IZQ` (Líneas 141–154):**
   - Mide pulsos de rueda externa: `pulsosActuales = abs(verPulsosEncoderB());`.
   - Mientras `pulsosActuales < PULSOS_GIRO_90_IZQ` (280): `movimiento(GIRAR_IZQ, {VEL_BASE_IZQ, VEL_BASE_DER});`.
   - Al finalizar: freno, `tiempoInicioFreno = millis()`, `estadoPostFreno = AVANZANDO`, pasa a `FRENANDO`.

8. **`GIRANDO_180` (Líneas 156–169):**
   - Mide pulsos: `pulsosActuales = abs(verPulsosEncoderA());`.
   - Mientras `pulsosActuales < PULSOS_GIRO_180` (300): `movimiento(GIRAR_DER, {VEL_BASE_IZQ, VEL_BASE_DER});`.
   - Al finalizar: freno, `tiempoInicioFreno = millis()`, `estadoPostFreno = AVANZANDO`, pasa a `FRENANDO`.

---

## 4. Relevamiento de Subsistemas de Hardware y Lazo de Control

### 4.1 Encoders y Odometría
- **Driver:** `src/hardware/encoders/encoders.cpp` sobre librería `ESP32Encoder`.
- **Canales de Hardware:** Periférico PCNT del ESP32 en modo cuadratura 4x (`attachFullQuad`).
- **Pines:**
  - Encoder A: GPIO 33 (`ENC_A_1`) y GPIO 25 (`ENC_B_1`).
  - Encoder B: GPIO 26 (`ENC_A_2`) y GPIO 27 (`ENC_B_2`).
- **Comportamiento en bucle:**
  - `verPulsosEncoderA()` y `verPulsosEncoderB()` devuelven valores `int32_t`.
  - `resetearEncoders()` invoca `clearCount()` en ambas instancias.
  - No introduce latencias ni bloqueos en el lazo principal.

### 4.2 Sensores de Distancia Time-of-Flight (VL53L0X)
- **Driver:** `src/hardware/sensoresDistancia/sensoresDistancia.cpp` sobre librería `VL53L0X` de Pololu.
- **Bus I2C:** Pines SDA GPIO 21, SCL GPIO 22, con frecuencia configurada a 10 kHz (`Wire.setClock(10000)`).
- **Direccionamiento Dinámico vía XSHUT:**
  - Derecho: XSHUT en GPIO 18 $\rightarrow$ Dirección `0x30` (`adressDer`).
  - Izquierdo: XSHUT en GPIO 23 $\rightarrow$ Dirección `0x31` (`adressIzq`).
  - Central: XSHUT en GPIO 19 $\rightarrow$ Dirección `0x32` (`adressCent`).
- **Modo de Sensado:**
  - Todos configurados en `startContinuous(0)` (modo continuo).
  - En `actualizarSensado()`: verifica el registro de interrupción `RESULT_INTERRUPT_STATUS & 0x07 != 0`. Si una nueva medición está disponible, lee `readRangeContinuousMillimeters()`.
  - Filtro y offsets calibrados:
    - Lectura saturada a máximo 2000 mm.
    - `distanciaCent = rawCent - OFSET_CENT;` (`OFSET_CENT = 50`)
    - `distanciaDer = rawDer - OFSET_DER;` (`OFSET_DER = 47`)
    - `distanciaIzq = rawIzq - OFSET_IZQ;` (`OFSET_IZQ = 40`)
  - **Estructura estática `lecturaAct`:** Si un sensor aún no completó su conversión en un ciclo del loop, se preserva el último valor válido leído, evitando retornar ceros espurios.

### 4.3 Motores y Tracción (Puente H + LEDC PWM)
- **Driver:** `src/hardware/movimiento/puenteH.cpp`.
- **Pines y PWM:**
  - Motor A (Izquierdo lógico / Físico derecho): Dirección en GPIO 14 (`AIN1`) y GPIO 4 (`AIN2`). PWM en GPIO 12 (`PWMA`) asignado a LEDC Canal 0.
  - Motor B (Derecho lógico / Físico izquierdo): Dirección en GPIO 16 (`BIN1`) y GPIO 17 (`BIN2`). PWM en GPIO 32 (`PWMB`) asignado a LEDC Canal 1.
  - Frecuencia PWM: 20 kHz, resolución 8 bits (valores 0 a 255).
- **Primitivas de Movimiento:**
  - `AVANZAR`: AIN1=HIGH, AIN2=LOW, BIN1=HIGH, BIN2=LOW.
  - `FRENO_F`: Entradas en LOW, PWM en 0 (frenado pasivo/disipativo).
  - `GIRAR_DER`, `GIRAR_IZQ`, `RETROCEDER`.
- **Alineación de Velocidades:**
  - En `config.h`: `VEL_BASE_IZQ = 45`, `VEL_BASE_DER = 45`.

### 4.4 Controlador PID de Centrado
- **Archivo:** `src/hardware/movimiento/PID.cpp`.
- **Parámetros:** `KP = 0.5`, `KD = 0.3`, `KI = 0`.
- **Lógica de Error:**
  - Detección de paredes con ventana de 180 mm: `distancia < UMBRAL_PARED_ESTADO_NORMAL + 50`.
  - Ambas paredes presentes: `error = distanciaDer - distanciaIzq;` (centrado diferencial).
  - Solo pared izquierda: `error = - (int16_t)mediciones.distanciaIzq;`.
  - Solo pared derecha: `error = (int16_t)mediciones.distanciaDer;`.
  - Ninguna pared: `error = 0;`.
- **Ley de Control:**
  $$\text{correccion} = K_p \cdot \text{error} + K_d \cdot (\text{error} - \text{errorAnterior})$$
  - Saturación: `constrain(correccion, -25, 25)`.
- **Aplicación en `main.cpp`:**
  - Motor izquierdo = `constrain(VEL_BASE_DER - correccion, 0, 255)`.
  - Motor derecho = `constrain(VEL_BASE_IZQ + correccion, 0, 255)`.

---

## 5. Mapeo y Análisis de Requerimientos (R1, R2, R3, R4)

| Requerimiento | Estado en Código Actual | Ubicación de Impacto | Diagnóstico y Brecha Técnica |
|---|---|---|---|
| **R1. Reseteo por flanco lateral** | ❌ Ausente | `src/main.cpp` (`case AVANZANDO:`) | Actualmente el robot avanza siempre 800 pulsos fijos. No existe memoria de pared lateral previa ni detección del flanco de apertura (`<130` a `>130`). Falta resetear odometría y fijar meta a 400 pulsos al detectar el flanco. |
| **R2. Alineación con pared frontal** | ❌ Ausente | `src/main.cpp` (`case AVANZANDO:`) | El sensor frontal (`distanciaCent`) solo se evalúa al llegar a `DECISION`. En `AVANZANDO`, el freno por pared frontal a $\le 50\text{ mm}$ no existe y debe sobreescribir la condición de odometría. |
| **R3. Gracia post-giro (Ceguera PID)** | ❌ Ausente | `src/main.cpp` (`case AVANZANDO:`) | La corrección PID se calcula y aplica desde el pulso 0. Debe forzarse `correccion = 0` durante los primeros 100 pulsos al entrar a `AVANZANDO`. |
| **R4. Ausencia estricta de mapeo** | ✅ Cumple | Arquitectura general | El sistema no posee mapas ni matrices. La FSM conserva el estado `FRENANDO` de 150 ms para absorber inercia mecánica. |

---

## 6. Análisis Detallado de Implementación de Cada Requerimiento

### 6.1 Requerimiento R1: Reseteo por Flanco Lateral
- **Principio Físico:** En MicroMouse, el robot entra a una celda y detecta una pared lateral (distancia $< 130\text{ mm}$). Cuando la pared termina (acceso a pasillo perpendicular), la lectura salta a $> 130\text{ mm}$ (flanco de caída de presencia de pared / subida de distancia). Este punto geométrico corresponde al borde de la pared (poste/esquina). Desde esa esquina, el centro de la celda actual se encuentra exactamente a media celda ($9\text{ cm} \approx 400\text{ pulsos}$).
- **Comportamiento Requerido:**
  1. Durante `AVANZANDO`, registrar si había pared lateral (`der < 130` o `izq < 130`).
  2. Si una pared que estaba presente pasa a $> 130$, se dispara el flanco.
  3. Al disparar:
     - Se resetea el conteo de odometría de referencia (o se resetean los encoders).
     - La condición de avance restante pasa a ser exactamente **400 pulsos** (`PULSOS_CELDA / 2`).
     - Debe asegurarse que este reseteo se produzca **una sola vez por celda**.
  4. Si desde el inicio del avance no se detectó ninguna pared lateral (ej. ambos lados abiertos), el robot mantiene su avance estándar de **800 pulsos** (`PULSOS_CELDA`).

### 6.2 Requerimiento R2: Alineación con Pared Frontal
- **Principio Físico:** Cuando el robot avanza hacia una pared ciega frontal, la odometría pura puede acumular error por deslizamiento o patinamiento de ruedas, provocando que frene demasiado lejos o choque contra la pared.
- **Comportamiento Requerido:**
  1. Si hay una pared enfrente, el robot no debe utilizar el límite de pulsos (ni 800 ni 400) para detenerse.
  2. Debe continuar avanzando hasta que el sensor central reporte `distanciaCent <= 50` mm (verificando que sea una lectura válida $> 0$ para evitar falsos positivos por lecturas nulas o ruido de arranque).
  3. Al alcanzar $\le 50\text{ mm}$, conmuta inmediatamente a `FRENANDO` $\rightarrow$ `DECISION`.
  4. Si no hay pared enfrente (`distanciaCent > 130` o despejado), la condición de freno se gobierna exclusivamente por los encoders (R1 flanco 400 pulsos o fallback 800 pulsos).

### 6.3 Requerimiento R3: Gracia Post-Giro (Ceguera del PID)
- **Principio Físico:** Al salir de un giro de 90° o 180°, el chasis del robot puede encontrarse con cierta desalineación angular respecto al eje del pasillo, o uno de los sensores puede leer el borde saliente de una pared. Si el PID actúa inmediatamente, genera un volantazo brusco contra la pared contigua.
- **Comportamiento Requerido:**
  1. Al iniciar el avance (especialmente al salir de `FRENANDO` tras un giro, donde los encoders se ponen a cero), durante los primeros **100 pulsos**:
     - `correccion = 0;`
     - Los motores avanzan estrictamente a velocidad base simétrica (`VEL_BASE_IZQ`, `VEL_BASE_DER`).
  2. Una vez superados los 100 pulsos, el PID se activa normalmente.
  3. **Cuidado con la componente derivativa:** Durante la gracia de 100 pulsos, el error anterior del PID debe mantenerse en 0 (`resetearErrorAnterior()`) para que en el pulso 101 no se produzca un salto derivativo brusco ($K_d \cdot \Delta e$).
  4. **Interacción R3 y R1:** Si R1 resetea los encoders físicamente en la mitad de la celda (al detectar el flanco), NO debe reiniciar la ceguera del PID, ya que el robot ya está centrado en el pasillo y necesita corrección PID continua. Por ende, la variable de gracia de PID debe contar desde el ingreso a `AVANZANDO` o ser independiente del reseteo de flanco lateral.

### 6.4 Requerimiento R4: Ausencia Estricta de Mapeo y Conservación de `FRENANDO`
- **Principio:** No incorporar arreglos bidimensionales, laberintos virtuales, FloodFill ni historial cartesiano.
- **Conservación Arquitectónica:** Mantener la FSM no apropiativa, conservando el estado `FRENANDO` con su temporización estabilizadora de 150 ms entre `AVANZANDO` y `DECISION`, y entre los giros y `AVANZANDO`.

---

## 7. Puntos Críticos y Recomendaciones Técnicas para la Implementación

1. **Gestión de Odometría sin Bloqueos:**
   - Para no acoplar el reseteo de encoders de R1 con el contador de gracia de R3, se recomienda manejar una variable de estado en `AVANZANDO`:
     - `metaPulsos` (inicializada en 800 al comenzar la celda).
     - Si se detecta el flanco de caída lateral, `metaPulsos = pulsosActuales + 400` (o blanqueo relativo de odometría para el remanente de celda), permitiendo que el contador acumulado desde el inicio de la celda continúe para R3.
2. **Robustez de Sensado Frontal (R2):**
   - El sensor central descuenta `OFSET_CENT` (50 mm). Un valor de lectura nulo o un timeout podría reportar `<= 0`. La guarda debe exigir `sensadoActual.distanciaCent > 0 && sensadoActual.distanciaCent <= 50` para evitar frenados espurios en pasillos despejados.
3. **Mantenimiento del Lazo Libre de Delays Bloqueantes:**
   - Todo el control debe mantenerse reactivo dentro del `switch(estado)` de `main.cpp`.
