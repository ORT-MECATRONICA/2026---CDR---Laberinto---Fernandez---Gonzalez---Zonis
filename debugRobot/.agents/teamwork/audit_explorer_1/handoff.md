# Informe Maestro de Auditoría de Código y Diagnóstico de Firmware

**Subagente Auditor:** `audit_explorer_1` (teamwork_preview_explorer)  
**Fecha:** 2026-10-07  
**Modo de Trabajo:** STRICTLY READ-ONLY (Sin modificaciones a archivos fuente)  
**Objetivos Auditados:**
- `src/main.cpp`
- `src/main.h`
- `src/config.h`
- Subsistemas dependientes en `src/hardware/`

---

## 1. Observation

A través de inspección estática de código, verificación de histórico git y ejecución de compilación cruzada con la cadena de herramientas PlatformIO (`C:\Users\devandroid\.platformio\penv\Scripts\pio.exe run`), se observaron de manera directa los siguientes hechos reproducibles:

### 1.1 Estado de Compilación y Memoria
- Comando ejecutado: `C:\Users\devandroid\.platformio\penv\Scripts\pio.exe run`
- Resultado: **EXIT CODE 0** (Compilación exitosa).
- Métricas reportadas por la toolchain:
  ```text
  RAM:   [=         ]  12.4% (used 40604 bytes from 327680 bytes)
  Flash: [========= ]  86.8% (used 1137453 bytes from 1310720 bytes)
  ```
  La partición `app0` por defecto (1,310,720 bytes) tiene únicamente 173,267 bytes disponibles (86.8% ocupada) debido a `BluetoothSerial`.

### 1.2 Hallazgos de Código en `src/main.cpp`
1. **Líneas 66-98 (`case AVANZANDO:`):**
   ```cpp
   pulsosActuales = (abs(verPulsosEncoderA()) + abs(verPulsosEncoderB())) / 2;
   sensadoActual = actualizarSensado();
   int16_t correccion = calcularCorreccion(sensadoActual);
   velocidadActual.izquierda = constrain(VEL_BASE_IZQ + correccion, 0, 255);
   velocidadActual.derecha = constrain(VEL_BASE_DER - correccion, 0, 255);
   movimiento(AVANZAR, {velocidadActual.izquierda, velocidadActual.derecha});
   
   bool condicionGiroDer = sensadoActual.distanciaDer >= UMBRAL_PARED_ESTADO_NORMAL;
   bool condicionAvanzar = sensadoActual.distanciaDer < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaCent >= UMBRAL_PARED_ESTADO_NORMAL;
   bool condicionGiroIzq = sensadoActual.distanciaDer < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaCent < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaIzq >= UMBRAL_PARED_ESTADO_NORMAL;
   bool condicionGiro180 = sensadoActual.distanciaDer < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaCent < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaIzq < UMBRAL_PARED_ESTADO_NORMAL;
   ```
   - `pulsosActuales` se calcula en la línea 67 pero **no se utiliza en ninguna condición o cálculo** dentro de `case AVANZANDO:`.
   - `PULSOS_CELDA` (800) y `PULSOS_CELDA_MEDIA` (400) no aparecen en ninguna línea de `src/main.cpp`.
   - No existe detección de flanco lateral ni lógica para avanzar media celda (400 pulsos) tras apertura.
   - No existe control de parada por sensor central a 50 mm (`DISTANCIA_PARADA_FRENTE`).
   - No existe período de gracia de 100 pulsos (`PULSOS_GRACIA_PID`). La corrección PID se aplica desde el pulso 0.

2. **Líneas 76, 88, 116-121 (`PREGIRO_IZQ`):**
   ```cpp
   76: bool condicionGiroIzq = sensadoActual.distanciaDer < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaCent < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaIzq >= UMBRAL_PARED_ESTADO_NORMAL;
   ...
   88: estado = PREGIRO_IZQ;
   ...
   116: case PREGIRO_IZQ : {
   117:   pulsosActuales = (abs(verPulsosEncoderB())+abs(verPulsosEncoderA()))/2;
   118:   
   119:   if (pulsosActuales < PULSOS_PREGIRO_90_IZQ) {
   120:     movimiento(AVANZAR, {VEL_BASE_IZQ, VEL_BASE_DER});
   121:   } else { ... }
   ```
   - La condición de entrada requiere que `distanciaCent < 130` (pared enfrente).
   - Al entrar a `PREGIRO_IZQ`, el robot avanza hacia adelante a ciegas durante `PULSOS_PREGIRO_90_IZQ` (280 pulsos, ~63 mm), directamente contra la pared frontal.

3. **Líneas 20, 111, 127, 137, 155 (Timeouts en giros comentados):**
   ```cpp
   20: //uint32_t tiempoInicioGiro = 0;
   111: //tiempoInicioGiro = millis();
   127: // tiempoInicioGiro = millis();
   137: //bool cortePorTiempoDer = (millis() - tiempoInicioGiro) < 2000;
   155: //bool cortePorTiempoIzq = (millis() - tiempoInicioGiro) < 2000;
   ```
   - Toda guarda temporal está comentada con `//`. Si un encoder falla o las ruedas resbalan, el estado no tiene salida.

4. **Líneas 51-55 (`case LISTO:`):**
   ```cpp
   51: if (digitalRead(BOTON1) == LOW) {
   52:   //VER SI EL ANTIRREBOTE ES ÓPTIMO
   53:   while(digitalRead(BOTON1) == LOW) { delay(10); } // Esperar a que se suelte el botón
   54:   estado = AVANZANDO;
   55: }
   ```
   - No se invocan `resetearEncoders()` ni `resetearErrorAnterior()` al pasar de `LISTO` a `AVANZANDO`.
   - `while(digitalRead(BOTON1) == LOW)` bloquea la ejecución indefinidamente mientras el pulsador esté presionado.

5. **Líneas 93-96 (`else` inalcanzable en `AVANZANDO`):**
   ```cpp
   93: } else {
   94:   enviarString(">>> ERROR DE SENSADO <<<");
   95:   movimiento(FRENO_F, {0,0});
   96: }
   ```
   - Las 4 condiciones booleanas anteriores abarcan el 100% del espacio tridimensional de lecturas discretas. El bloque `else` es código muerto inalcanzable.

6. **Línea 18, 143, 161 (Variable muerta `movimientoAnterior`):**
   ```cpp
   18: MOVIMIENTOS movimientoAnterior = AVANZAR;
   143: movimientoAnterior = GIRAR_DER;
   161: movimientoAnterior = GIRAR_IZQ;
   ```
   - La variable `movimientoAnterior` se asigna pero nunca se lee en ninguna parte del firmware.

### 1.3 Hallazgos de Código en `src/main.h`
- Líneas 14-25:
  ```cpp
  enum MAQUINA_NUEVA {
      LISTO,
      INFORMACION_RECIBIDA,
      AVANZANDO,
      DECISION,
      PREGIRO_DER,
      PREGIRO_IZQ,
      POSTGIRO,
      GIRANDO_DER,
      GIRANDO_IZQ,
      GIRANDO_180,
  };
  ```
  - El estado `DECISION` está declarado en `enum MAQUINA_NUEVA`, pero en `src/main.cpp:42-199` no existe `case DECISION:` ni cláusula `default:`.
  - El estado `FRENANDO` (requerido por R4 en `ORIGINAL_REQUEST.md`) fue eliminado del enum.

### 1.4 Hallazgos de Código en `src/config.h`
- Líneas 66-75:
  ```cpp
  #define PULSOS_CELDA 800
  #define PULSOS_CELDA_MEDIA 400
  #define DISTANCIA_PARADA_FRENTE 50
  #define DISTANCIA_MIN_VALIDA -20
  #define PULSOS_GRACIA_PID 100
  #define PULSOS_MIN_DETECCION_FLANCO 150
  #define PULSOS_WATCHDOG_SEGURIDAD 1050
  #define PULSOS_PREGIRO_90_DER 300
  #define PULSOS_PREGIRO_90_IZQ 280
  ```
  - Las constantes `PULSOS_CELDA`, `PULSOS_CELDA_MEDIA`, `DISTANCIA_PARADA_FRENTE`, `DISTANCIA_MIN_VALIDA`, `PULSOS_GRACIA_PID`, `PULSOS_MIN_DETECCION_FLANCO`, `PULSOS_WATCHDOG_SEGURIDAD` están declaradas pero no tienen ningún uso en `src/main.cpp`.
- Líneas 7-11, 20, 33, 43, 64, 81-83:
  - Macros huérfanas sin uso: `X_SIZE`, `Y_SIZE`, `X_START`, `Y_START`, `UMBRAL_LECTURA`, `DELAY_TIEMPO_FRENADO_EN_F`, `BOTON2`, `CNY`, `TIEMPO_90_GRADOS`, `TIEMPO_AVANCE_PREGIRO`, `TIEMPO_AVANZAR_BLOQUEANTE`.
- Línea 49:
  `#define PWMA 12`
  - Asignado a pin de strapping MTDI (GPIO 12).

### 1.5 Hallazgos en Subsistemas Vinculados (`src/hardware/`)
1. **`src/hardware/movimiento/PID.cpp:6-7, 29-37` (Bug de pulsos intermitentes en PID):**
   ```cpp
   static int16_t errorAnterior = 0;
   static int32_t tiempoAnterior = millis();
   static int16_t correccionAnterior = 0;
   ...
   int32_t tiempoActual = millis();
   if (tiempoActual - tiempoAnterior > 50) {
       int16_t correccion = (KP * error) + (KD * (error - errorAnterior)) ;
       errorAnterior = error; 
       tiempoAnterior = tiempoActual;
       return constrain(correccion, -25,25);
   } else {
       return correccionAnterior;
   }
   ```
   - `correccionAnterior` está inicializado en 0 y **NUNCA se actualiza**.
   - En cualquier ciclo donde `tiempoActual - tiempoAnterior <= 50`, la función retorna `0`.
   - La corrección PID sólo se aplica durante 1 ciclo cada 50 ms y es 0 durante los demás ciclos, generando sacudidas bruscas y pérdida del 80% de la autoridad de control.
2. **`src/hardware/movimiento/PID.cpp:19-25` (Inversión de signo y falta de setpoint - BUG-16):**
   ```cpp
   } else if (hayIzq) {
       error = - (int16_t)mediciones.distanciaIzq;
   } else if (hayDer) {
       error = (int16_t)mediciones.distanciaDer;
   }
   ```
   - No resta distancia consigna (`DISTANCIA_OBJETIVO_PARED ≈ 45 mm`). El signo comanda giro hacia la pared detectada.
3. **`src/hardware/movimiento/puenteH.cpp:39-56` (Inversión cinemática en giros - BUG-19):**
   - `GIRAR_DER` pone `AIN1=LOW, AIN2=HIGH` (retroceso izq) y `BIN1=HIGH, BIN2=LOW` (avance der), girando antihorario (hacia la izquierda).
4. **`src/hardware/logger/logger.cpp:9-11` (Falta de salida UART - BUG-34):**
   - `enviarString` invoca `SerialBT.println(str)` pero omite `Serial.println(str)`. La consola serie USB permanece en silencio.
5. **`src/hardware/movimiento/PID.h:5` (Declaración huérfana):**
   - Prototipo `int16_t calcularCorreccionRightHand(int16_t error);` no tiene cuerpo en `PID.cpp`.

---

## 2. Logic Chain

A continuación se detalla la concatenación lógica rigurosa que conecta las observaciones con las conclusiones del diagnóstico:

```
[Observación 1.2.1: PULSOS_CELDA y PULSOS_CELDA_MEDIA no se usan en main.cpp]
    + [Observación 1.4: Definiciones presentes en config.h]
    ──> (Paso 1): Las especificaciones de odometría R1 (reseteo por flanco lateral y avance 400 pulsos)
                  y R2 (parada frontal por distancia de 50 mm) NO fueron integradas en main.cpp.
                  El firmware actual carece de la sincronización de avance por celdas requerida.

[Observación 1.2.2: condicionGiroIzq exige distanciaCent < 130]
    + [Observación 1.2.2: case PREGIRO_IZQ ejecuta movimiento(AVANZAR) durante 280 pulsos]
    ──> (Paso 2): Cuando el robot detecta un obstáculo al frente (< 130 mm), ingresa a PREGIRO_IZQ
                  y avanza hacia adelante 280 pulsos (~63 mm). Dado que la pared ya está frente a él,
                  este avance frontal ciego garantiza una colisión física frontal antes de girar.

[Observación 1.2.3: tiempoInicioGiro y cortePorTiempo comentados]
    ──> (Paso 3): La salida de los estados GIRANDO_DER, GIRANDO_IZQ, PREGIRO y POSTGIRO depende 100%
                  de que los encoders registren pulsos. Si las ruedas patinan sobre polvo o se calan,
                  el robot queda atrapado en un bucle infinito (Deadlock de FSM).

[Observación 1.3: DECISION está en enum MAQUINA_NUEVA pero no en switch]
    + [Observación 1.3: switch(estado) carece de default:]
    ──> (Paso 4): Cualquier asignación a DECISION provoca la omisión total del switch en loop(),
                  congelando los motores en su estado previo sin manejo de excepción.

[Observación 1.5.1: correccionAnterior = 0 nunca cambia]
    + [Observación 1.5.1: retorna correccionAnterior si deltaT <= 50 ms]
    ──> (Paso 5): En un loop de ~10 ms, 4 de cada 5 iteraciones retornan corrección = 0,
                  aplicando torque diferencial sólo en pulsos aislados cada 50 ms.

[Observación 1.5.2: error = -distanciaIzq sin consigna]
    ──> (Paso 6): Al acercarse a una pared izquierda, correccion negativa desacelera la rueda izquierda
                  y acelera la derecha, provocando un viraje en dirección a la pared hasta impactar.

[Observación 1.1: Flash consumida 86.8% en app0]
    ──> (Paso 7): La partición default.csv no deja espacio suficiente para albergar lógica de mapeo
                  o FloodFill, corroborando BUG-32.
```

---

## 3. Catálogo Integral de Defectos Identificados

A continuación se presenta el catálogo estructurado de defectos, clasificando tanto los problemas persistentes/resueltos de `bug_report.md` como las anomalías recién descubiertas en el código actual.

### 3.1 Defectos de Severidad Crítica (CRITICAL)

#### DEF-01: Ausencia Total de Implementación de Mecánicas de Odometría R1-R4 (Regresión Masiva)
- **Ruta:** `src/main.cpp` (Líneas 66–98) frente a `src/config.h` (Líneas 66–75)
- **Severidad:** **CRITICAL**
- **Causa Raíz:** En `src/config.h` se definieron `PULSOS_CELDA` (800), `PULSOS_CELDA_MEDIA` (400), `DISTANCIA_PARADA_FRENTE` (50), `PULSOS_GRACIA_PID` (100) y `PULSOS_MIN_DETECCION_FLANCO` (150). Sin embargo, el archivo actual `src/main.cpp` **no implementa ninguna de estas mecánicas**:
  - No detecta flancos descendentes de pared (<130 a >130) ni resetea para avanzar 400 pulsos (R1 omitido).
  - No detiene el avance por proximidad frontal a 50 mm; evalúa la transición a 130 mm (R2 omitido).
  - No aplica período de gracia de 100 pulsos con PID en 0; calcula corrección desde el pulso 0 (R3 omitido).
  - El estado `FRENANDO` fue eliminado de la FSM (R4 omitido).
- **Consecuencias Dinámicas:** El robot opera de forma puramente reactiva e imprecisa, perdiendo la sincronización con la cuadrícula de 18 cm del laberinto.

#### DEF-02: Colisión Frontal Inevitable en Maniobra de `PREGIRO_IZQ`
- **Ruta:** `src/main.cpp` (Líneas 76, 88, 116–121)
- **Severidad:** **CRITICAL**
- **Causa Raíz:** La transición hacia `PREGIRO_IZQ` se dispara cuando `distanciaCent < UMBRAL_PARED_ESTADO_NORMAL` (pared frontal a menos de 130 mm). Al ingresar a `PREGIRO_IZQ:`, el código ejecuta:
  ```cpp
  if (pulsosActuales < PULSOS_PREGIRO_90_IZQ) {
    movimiento(AVANZAR, {VEL_BASE_IZQ, VEL_BASE_DER});
  }
  ```
  Esto comanda un avance lineal ciego de 280 pulsos (~63 mm) **directamente hacia la pared frontal ya detectada**.
- **Consecuencias Dinámicas:** El robot choca violentamente de frente contra la pared antes de poder ejecutar el giro a la izquierda.

#### DEF-03: Inversión de Signo y Omisión de Setpoint en Algoritmo PID (BUG-16 Persistente)
- **Ruta:** `src/hardware/movimiento/PID.cpp` (Líneas 19–25)
- **Severidad:** **CRITICAL**
- **Causa Raíz:** En seguimiento de pared única (`hayIzq` o `hayDer`), se asigna `error = -distanciaIzq` y `error = distanciaDer` sin restar la consigna central (`DISTANCIA_OBJETIVO_PARED ≈ 45 mm`).
- **Consecuencias Dinámicas:** Si el robot detecta solo la pared izquierda a 40 mm, la rueda izquierda se frena y la derecha se acelera, virando el robot hacia la pared izquierda hasta colisionar.

#### DEF-04: Polaridad Cinemática Cruzada en Giros sobre el Eje (BUG-19 Persistente)
- **Ruta:** `src/hardware/movimiento/puenteH.cpp` (Líneas 39–56)
- **Severidad:** **CRITICAL**
- **Causa Raíz:** En `puenteH.cpp`, `GIRAR_DER` retrocede la rueda izquierda y avanza la derecha (rotación antihoraria = giro a la izquierda). `GIRAR_IZQ` avanza la izquierda y retrocede la derecha (giro a la derecha).
- **Consecuencias Dinámicas:** Las órdenes de giro emitidas por la FSM giran el robot físicamente en el sentido opuesto al decidido por los sensores.

---

### 3.2 Defectos de Severidad Alta (HIGH)

#### DEF-05: Chattering Severo y Pulsos Espurios por Variable `correccionAnterior` Estática en PID
- **Ruta:** `src/hardware/movimiento/PID.cpp` (Líneas 6–7, 29–37)
- **Severidad:** **HIGH**
- **Causa Raíz:** `static int16_t correccionAnterior = 0;` nunca se actualiza tras calcular `correccion`. Cuando `tiempoActual - tiempoAnterior <= 50`, retorna `0`.
- **Consecuencias Dinámicas:** La ley de control PID se anula a cero en 4 de cada 5 ciclos de ejecución, inyectando impulsos de corrección discontinuos cada 50 ms que desestabilizan el chasis.

#### DEF-06: Bloqueo Permanente (Deadlock) en Maniobras por Timeouts Comentados (BUG-12 Persistente)
- **Ruta:** `src/main.cpp` (Líneas 20, 111, 127, 137, 155)
- **Severidad:** **HIGH**
- **Causa Raíz:** Las guardas temporales de seguridad `//bool cortePorTiempoDer = ...` están comentadas. La finalización de giros y pre-giros depende exclusivamente de los pulsos del encoder.
- **Consecuencias Dinámicas:** Si una rueda pierde tracción sobre la pista, se traba mecánicamente o un encoder se desconecta, el microcontrolador entra en un bucle infinito consumiendo batería hasta recalentar los motores.

#### DEF-07: Bucle Infinito de Giros a la Derecha en Espacios Abiertos (Círculo de la Muerte)
- **Ruta:** `src/main.cpp` (Líneas 74, 78–81, 144, 177)
- **Severidad:** **HIGH**
- **Causa Raíz:** La condición `bool condicionGiroDer = sensadoActual.distanciaDer >= UMBRAL_PARED_ESTADO_NORMAL;` evalúa que ante cualquier ausencia de pared derecha se debe girar inmediatamente. Al salir de `POSTGIRO`, regresa a `AVANZANDO`; si la celda contigua sigue sin pared derecha, gira inmediatamente otra vez.
- **Consecuencias Dinámicas:** En celdas abiertas de 2x2 o al iniciar sin pared derecha, el robot entra en un bucle cerrado girando en círculos indefinidamente sin avanzar por el laberinto.

#### DEF-08: Desconexión de Odometría de Celda `PULSOS_CELDA` (BUG-15 Persistente)
- **Ruta:** `src/config.h` (Línea 66) frente a `src/main.cpp` (Líneas 66–98)
- **Severidad:** **HIGH**
- **Causa Raíz:** `PULSOS_CELDA 800` está definido en `config.h` pero `pulsosActuales` no se evalúa en `AVANZANDO`. La navegación no sincroniza con los 18 cm de celda.

#### DEF-09: Riesgo Eléctrico de Pin Strapping MTDI en GPIO 12 (`PWMA`) (BUG-28 Persistente)
- **Ruta:** `src/config.h` (Línea 49)
- **Severidad:** **HIGH**
- **Causa Raíz:** GPIO 12 es el pin MTDI del ESP32. Si se encuentra en HIGH durante el reinicio por una fuga del driver de motor, conmuta el LDO de flash a 1.8V provocando bootloop permanente.

#### DEF-10: Saturación de Memoria Flash al 86.8% por BluetoothSerial (BUG-32 Persistente)
- **Ruta:** `platformio.ini` (Líneas 11–19)
- **Severidad:** **HIGH**
- **Causa Raíz:** La partición por defecto reserva 1.25 MB, de los cuales `BluetoothSerial` consume 1.13 MB, dejando sólo 173 KB libres e impidiendo agregar lógica de mapeo o FloodFill.

#### DEF-11: Omisión de Umbral Frontal `UMBRAL_PARED_FRENTE` (BUG-30 Persistente)
- **Ruta:** `src/main.cpp` (Líneas 75–77) frente a `src/config.h` (Línea 68)
- **Severidad:** **HIGH**
- **Causa Raíz:** La detección de pared frontal compara `distanciaCent` contra 130 mm en lugar del umbral frontal real calibrado (50 mm o 120 mm), abortando avances de celda antes de tiempo.

---

### 3.3 Defectos de Severidad Media (MEDIUM)

#### DEF-12: Estado `DECISION` Declarado en FSM pero no Implementado (BUG-11 Persistente)
- **Ruta:** `src/main.h` (Línea 18) frente a `src/main.cpp` (Líneas 42–199)
- **Severidad:** **MEDIUM**
- **Causa Raíz:** `MAQUINA_NUEVA` incluye `DECISION`. El switch de `main.cpp` no tiene `case DECISION:` ni cláusula `default:`.
- **Consecuencias Dinámicas:** Si algún módulo comanda `estado = DECISION`, el loop ignora la máquina de estados y congela los motores.

#### DEF-13: Ausencia de Reseteo de Odometría al Salir del Estado `LISTO`
- **Ruta:** `src/main.cpp` (Líneas 51–55)
- **Severidad:** **MEDIUM**
- **Causa Raíz:** Al detectar `BOTON1 == LOW`, pasa directamente a `AVANZANDO` sin llamar a `resetearEncoders()` ni `resetearErrorAnterior()`.
- **Consecuencias Dinámicas:** El movimiento manual del robot al posicionarlo en la celda de largada corrompe las cuentas odométricas iniciales.

#### DEF-14: Lecturas Negativas de Sensores Interpretadas como Pared Cercana
- **Ruta:** `src/main.cpp` (Líneas 75–77) y `src/hardware/sensoresDistancia/sensoresDistancia.cpp` (Líneas 89, 94, 99)
- **Severidad:** **MEDIUM**
- **Causa Raíz:** Si `raw < OFSET`, la distancia calculada es negativa (ej. -15 mm). La condición `distancia < 130` evalúa a `true`. No se usa `DISTANCIA_MIN_VALIDA` (-20 mm en `config.h:69`).
- **Consecuencias Dinámicas:** Un fallo transitorio o descalibración negativa del sensor es interpretado falsamente como obstáculo frontal o lateral.

#### DEF-15: Velocidad de Giro Insuficiente `VEL_BASE` = 45 PWM (BUG-20 Persistente)
- **Ruta:** `src/main.cpp` (Líneas 139, 157, 189)
- **Severidad:** **MEDIUM**
- **Causa Raíz:** Todos los giros usan `{VEL_BASE_IZQ, VEL_BASE_DER}` (PWM 45 / 17.6%). Insuficiente torque para rotar sobre ruedas de goma de alto agarre, calando los motores.

#### DEF-16: Pin GPIO 34 (`BOTON1`) sin Resistencia de Pull-Up Interna (BUG-29 Persistente)
- **Ruta:** `src/config.h` (Línea 42) y `src/main.cpp` (Línea 28)
- **Severidad:** **MEDIUM**
- **Causa Raíz:** GPIO 34 es entrada analógica/digital pura sin resistencias de pull-up integradas en silicio. Si la placa carece de pull-up externo, el pin flota y genera arranques falsos.

#### DEF-17: Falta de Salida UART por Consola Serie (BUG-34 Persistente)
- **Ruta:** `src/hardware/logger/logger.cpp` (Líneas 9–11)
- **Severidad:** **MEDIUM**
- **Causa Raíz:** `enviarString` sólo emite hacia `SerialBT`. La consola serie conectada por USB permanece silente.

---

### 3.4 Defectos de Severidad Baja (LOW)

#### DEF-18: Bucle Bloqueante de Antirrebote en Botón de Inicio
- **Ruta:** `src/main.cpp` (Línea 53)
- **Severidad:** **LOW**
- **Causa Raíz:** `while(digitalRead(BOTON1) == LOW) { delay(10); }` congela el microcontrolador mientras el pulsador permanezca presionado mecánicamente.

#### DEF-19: Código Muerto en Bloque `else` de `case AVANZANDO:`
- **Ruta:** `src/main.cpp` (Líneas 93–96)
- **Severidad:** **LOW**
- **Causa Raíz:** Las 4 ramas booleanas previas cubren exhaustivamente todo el espacio numérico de distancias. El mensaje `>>> ERROR DE SENSADO <<<` nunca se ejecuta.

#### DEF-20: Variable Global Asignada pero Jamás Leída (`movimientoAnterior`)
- **Ruta:** `src/main.cpp` (Líneas 18, 143, 161)
- **Severidad:** **LOW**
- **Causa Raíz:** `movimientoAnterior` se almacena tras giros pero ningún proceso consulta su valor.

#### DEF-21: Declaración Huérfana en Cabecera `PID.h`
- **Ruta:** `src/hardware/movimiento/PID.h` (Línea 5)
- **Severidad:** **LOW**
- **Causa Raíz:** `int16_t calcularCorreccionRightHand(int16_t error);` no posee cuerpo de implementación en `PID.cpp`.

#### DEF-22: Constantes Inertes y Obsoletas en `src/config.h`
- **Ruta:** `src/config.h` (Líneas 7–11, 20, 33, 43, 64, 81–83)
- **Severidad:** **LOW**
- **Causa Raíz:** Macros como `X_SIZE`, `Y_SIZE`, `UMBRAL_LECTURA`, `DELAY_TIEMPO_FRENADO_EN_F`, `CNY`, `TIEMPO_90_GRADOS` no tienen ninguna referencia en el código activo.

---

## 4. Estado de Verificación de Defectos Previos (`bug_report.md`)

| ID Previo | Ubicación Principal | Estado Actual | Evidencia y Justificación |
|---|---|:---:|---|
| **BUG-01** | `src/CITÉ.cpp` | **RESUELTO** | Archivo no-ASCII retirado del árbol C++. Compilación PlatformIO exitosa. |
| **BUG-02** | `src/CITÉ.cpp` | **RESUELTO** | Eliminada colisión de símbolos `setup`/`loop`. |
| **BUG-03** | `src/CITÉ.cpp` | **RESUELTO** | Tipo obsoleto `MAQUINA_ESTADOS` no presente en código compilado. |
| **BUG-05** | Headers `.h` | **RESUELTO** | Prototipos obsoletos de HCSR04 y encoders fueron depurados de los headers. |
| **BUG-06** | `puenteH.cpp` | **RESUELTO** | Funciones bloqueantes huérfanas eliminadas. |
| **BUG-07** | `src/main.cpp:97` | **RESUELTO** | `break;` presente al final de `case AVANZANDO:`, evitando fallthrough a `PREGIRO_DER:`. |
| **BUG-08** | `src/main.cpp:82-84` | **RESUELTO** | Se eliminó el reseteo continuo de encoders en avance en línea recta. |
| **BUG-09** | `src/main.cpp:83` | **RESUELTO en main** | `resetearErrorAnterior()` ya no se llama en avance continuo. |
| **BUG-10** | `src/main.cpp:74-77` | **RESUELTO** | Desigualdades combinadas con `>=` y `<` eliminan la zona muerta a 130 mm. |
| **BUG-11** | `src/main.h:18`, `main.cpp` | **PARCIAL / PERSISTENTE** | `POSTGIRO` implementado; `DECISION` sigue sin `case` en `main.cpp`. |
| **BUG-12** | `src/main.cpp:137, 155` | **PERSISTENTE** | Timeouts comentados; riesgo de deadlock por bloqueo de ruedas. |
| **BUG-13** | `src/main.cpp:134, 152` | **RESUELTO** | División espuria `/ 2` eliminada de conteos individuales de giro. |
| **BUG-14** | `src/config.h:26` | **RESUELTO** | `PULSOS_GIRO_180` elevado a 700 (apropiado para 180° frente a ~318 de 90°). |
| **BUG-15** | `src/main.cpp:66-98` | **PERSISTENTE** | `PULSOS_CELDA` (800) definido pero no usado; navegación 100% reactiva. |
| **BUG-16** | `PID.cpp:19-25` | **PERSISTENTE** | Seguimiento de pared única carece de consigna e invierte dirección. |
| **BUG-17** | `sensoresDistancia.cpp` | **PERSISTENTE** | Asimetría de offsets físicos de sensores se arrastra a mediciones. |
| **BUG-18** | `PID.cpp:31` | **PERSISTENTE** | Término derivativo sin normalización por $\Delta t$ (sensible a jitter). |
| **BUG-19** | `puenteH.cpp:39-56` | **PERSISTENTE** | Giros sobre el eje mantienen polaridad invertida en pines H-bridge. |
| **BUG-20** | `main.cpp:139, 157` | **PERSISTENTE** | Giros comandados a PWM 45 (`VEL_BASE`) con riesgo de calado. |
| **BUG-21** | `puenteH.cpp:57-65` | **RESUELTO** | `FRENO_F` ahora coloca pines en HIGH con PWM 40 para frenado activo. |
| **BUG-22** | `puenteH.cpp:25, 35` | **PERSISTENTE** | Casting implícito de `int16_t` a `uint32_t` sin `constrain` previo en driver. |
| **BUG-23** | `sensoresDistancia.cpp:80` | **PERSISTENTE** | `lecturaAct` arranca en `{0,0,0}` disparando giro de 180° falso al boot. |
| **BUG-24** | `sensoresDistancia.cpp:43` | **RESUELTO** | Bucles `while(true)` de fallo de init fueron removidos (solo log de error). |
| **BUG-25** | `sensoresDistancia.cpp:23` | **PERSISTENTE** | Reloj I2C configurado a 10 kHz (latencia de 30 ms por ciclo de sensado). |
| **BUG-26** | `sensoresDistancia.cpp:89` | **PERSISTENTE** | Underflow produce distancias negativas en proximidad menor al offset. |
| **BUG-27** | `sensoresDistancia.cpp:85` | **PERSISTENTE** | Guarda temporal comentada; transacciones I2C ejecutadas en cada loop. |
| **BUG-28** | `config.h:49` | **PERSISTENTE** | GPIO 12 (`PWMA`) es pin strapping MTDI (peligro de bootloop). |
| **BUG-29** | `config.h:42`, `main.cpp:28`| **PERSISTENTE** | GPIO 34 (`BOTON1`) sin pull-up interno en silicio. |
| **BUG-30** | `main.cpp:75-77` | **PERSISTENTE** | Umbral frontal evaluado a 130 mm en lugar de distancia calibrada. |
| **BUG-31** | `PID.cpp:11-12` | **RESUELTO** | Eliminado el desfase de `+ 50` en PID; unificado a 130 mm con FSM. |
| **BUG-32** | `platformio.ini` | **PERSISTENTE** | Saturación de Flash al 86.8% en `app0` confirmada por PlatformIO. |
| **BUG-33** | `puenteH.cpp:19-66` | **PERSISTENTE** | Ausencia de `default:` en switch y falta de puesta a cero en init. |
| **BUG-34** | `logger.cpp:9-11` | **PERSISTENTE** | Silencio total en consola serie USB UART (`enviarString` solo envía por BT). |

---

## 5. Caveats

- **Limitación de Ejecución en Hardware Real:** La auditoría se condujo de forma estática y por compilación cruzada local; el comportamiento físico ante fricción de ruedas y lecturas ToF en vivo no fue medido sobre una pista física durante este turno.
- **Archivos `.txt` en `src/`:** En la carpeta `src/` residen múltiples borradores (`CITE.txt`, `movimiento.txt`, etc.). PlatformIO no los compila por su extensión `.txt`, pero representan deuda técnica severa de higiene de repositorio.
- **R1-R4 en `CITE.txt`:** Se confirmó que una implementación preliminar de R1-R4 existía en `src/CITE.txt` (proveniente del commit `bceb5e4`), pero fue removida de `src/main.cpp`. El análisis se enfoca estrictamente sobre el código en producción activo (`main.cpp`).

---

## 6. Conclusion

1. **Compilabilidad:** El firmware compila exitosamente bajo PlatformIO (`xtensa-esp32-elf-g++`), habiéndose resuelto los errores de bloqueo de compilación arcaicos (BUG-01, BUG-02, BUG-03).
2. **Defectos Resueltos:** Se confirmaron como corregidos 9 defectos históricos (`BUG-01`, `BUG-02`, `BUG-03`, `BUG-05`, `BUG-06`, `BUG-07`, `BUG-08`, `BUG-10`, `BUG-13`, `BUG-14`, `BUG-21`, `BUG-24`, `BUG-31`).
3. **Estado de Mecánicas de Odometría (R1-R4):** **NO ESTÁN IMPLEMENTADAS EN EL CÓDIGO ACTIVO (`src/main.cpp`)**. A pesar de que las constantes existen en `config.h`, `main.cpp` carece de detección de flancos, avance de media celda, parada frontal a 50 mm, período de gracia del PID y estado `FRENANDO`.
4. **Vulnerabilidades Críticas Inmediatas:** El firmware presenta un riesgo inminente de choque frontal en `PREGIRO_IZQ` (avanza 63 mm hacia la pared frontal), inversión de control en seguimiento de pared única en `PID.cpp` (conduce contra la pared), inversión de giro de motores en `puenteH.cpp` y pérdida de tracción suave por chattering en `PID.cpp`.

---

## 7. Verification Method

Para verificar independientemente todos los hallazgos documentados:

1. **Verificación de Compilación y Tamaño de Partición:**
   Ejecutar en la raíz del proyecto:
   ```powershell
   & "C:\Users\devandroid\.platformio\penv\Scripts\pio.exe" run
   ```
   *Resultado esperado:* Éxito con código 0 y reporte de Flash al 86.8% (1,137,453 / 1,310,720 bytes).

2. **Verificación de Omisión de R1-R4 en `src/main.cpp`:**
   Ejecutar búsqueda de uso de macros clave en `src/main.cpp`:
   ```powershell
   Select-String -Path "src/main.cpp" -Pattern "PULSOS_CELDA|DISTANCIA_PARADA_FRENTE|PULSOS_GRACIA_PID|PULSOS_CELDA_MEDIA"
   ```
   *Resultado esperado:* 0 ocurrencias en `src/main.cpp` (confirmación de que no están integradas).

3. **Verificación del Fallo en `PREGIRO_IZQ`:**
   Inspeccionar las líneas 76 y 119-121 de `src/main.cpp` con `view_file` para comprobar que la condición de entrada requiere pared al frente y la acción es avanzar 280 pulsos hacia ella.

4. **Verificación del Defecto de Chattering en `PID.cpp`:**
   Inspeccionar las líneas 7 y 30-37 de `src/hardware/movimiento/PID.cpp` con `view_file` para comprobar que `correccionAnterior` es siempre 0 y se retorna cuando `deltaT <= 50`.
