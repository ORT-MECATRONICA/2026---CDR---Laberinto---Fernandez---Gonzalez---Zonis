# Reporte de Handoff — Challenger 1 (Milestone M1)

**Agente:** Challenger 1 (`teamwork_preview_challenger`)  
**Fecha:** 2026-10-06  
**Ruta de Trabajo:** `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\.agents\teamwork\challenger_1`  
**Veredicto:** **REQUEST_CHANGES**

---

## 1. Observation

### 1.1 Código Fuente Inspeccionado
En `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\src\main.cpp`:
- **Líneas 104-124 (R1: Flanco Lateral):**
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
- **Líneas 126-148 (R2: Parada Frontal y Odometría):**
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
- **Líneas 150-158 (R3: Gracia PID):**
  ```cpp
  int16_t correccion = calcularCorreccion(sensadoActual);
  if (!graciaPIDFinalizada) {
    if (pulsosTotalesCelda < PULSOS_GRACIA_PID) {
      correccion = 0;
    } else {
      graciaPIDFinalizada = true;
    }
  }
  ```

En `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\src\config.h`:
- **Líneas 66-71:**
  ```cpp
  #define PULSOS_CELDA 800
  #define PULSOS_CELDA_MEDIA 400
  #define DISTANCIA_PARADA_FRENTE 50
  #define PULSOS_GRACIA_PID 100
  #define PULSOS_MIN_DETECCION_FLANCO 150
  #define PULSOS_WATCHDOG_SEGURIDAD 1050
  ```

### 1.2 Harness y Scripts de Verificación Ejecutados
Se desarrollaron dos harnesses empíricos en el directorio de trabajo del Challenger:
1. `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\.agents\teamwork\challenger_1\verify_odometry_adversarial.py`
2. `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\.agents\teamwork\challenger_1\test_harness_m1.cpp`

### 1.3 Resultados Empíricos por Escenario

| # | Escenario Evaluado | Resultado | Detalle Empírico / Observación |
|---|---|---|---|
| 1.1 | Flanco lateral izquierdo (<130 a >130 en 250p) | **PASS** | Flanco disparó en 260p, encoders reseteados a 0, parada en 400p post-flanco (660p totales). |
| 1.2 | Flanco lateral derecho (<130 a >130 en 300p) | **PASS** | Flanco disparó en 310p, parada exacta en 400p post-flanco (710p totales). |
| 1.3 | Ambos flancos cayendo en simultáneo (200p) | **PASS** | Cerrojo `flancoDetectado = true` ejecutó exactamente un reseteo; parada a 400p post-flanco. |
| 1.4 | Pared ausente desde el inicio (sin paredes laterales) | **PASS** | No hubo flanco (`flancoDetectado == false`); parada fallback exacta a 800 pulsos. |
| 2.1 | Muesca transitoria en pared lateral (< 150p) | **PASS** | Muesca a 50p ignorada por filtro `pulsosTotalesCelda > 150`; flanco real disparó a 300p. |
| 3.1 | Aproximación nominal a pared frontal (> 800p) | **PASS** | A 800p `paredAlFrente == true` suprimió `stopPorEncoders`; robot avanzó hasta `distanciaCent <= 50mm` (880p). |
| 3.2 | Obstáculo frontal temprano (> 100p, ej. 400p) | **PASS** | Detención segura inmediata a 410p por `stopPorParedFrontal`. |
| 3.3 | **Obstáculo frontal muy temprano (< 100p, ej. 40mm a pulso 10)** | **FAIL / VULNERABILIDAD** | **El robot NO frena a $\le 50\text{ mm}$. Debido a la guarda `pulsosTotalesCelda > 100`, avanza a ciegas 100 pulsos (~22.5 mm) antes de frenar, provocando colisión física.** |
| 4.1 | Watchdog de seguridad (sensor trabado en 70mm) | **PASS** | Parada de emergencia disparada a 1050 pulsos por `stopPorWatchdog`. |
| 4.2 | Lectura negativa en sensor frontal (`distanciaCent = -5`) | **FAIL / VULNERABILIDAD** | Guarda `distanciaCent > 0` evalúa falso; robot ignora pared frontal y continúa avanzando hasta agotar encoders. |
| 4.3 | Pico de ruido sensor frontal (> 120mm a pulso 820) | **FAIL / VULNERABILIDAD** | Al superar 800 pulsos, un único glitch `distanciaCent = 135` desactiva `paredAlFrente` y dispara `stopPorEncoders` prematuramente a 70mm de la pared. |
| 5.1 | Gracia PID post-giro (pulsos 0 a 99) | **PASS** | Corrección PID forzada a 0 durante los primeros 100 pulsos. |
| 5.2 | Inmunidad de gracia PID tras reseteo por R1 | **PASS** | Reseteo a pulso 250 mantiene `pulsosTotalesCelda = 250` y `graciaPIDFinalizada = true`; PID no se silencia a mitad de celda. |

---

## 2. Logic Chain

1. **R1 y Fallback de Odometría (Escenarios 1.1 - 1.4, 2.1):**
   - El cerrojo `flancoDetectado` y la condición `(flancoIzq || flancoDer) && pulsosTotalesCelda > PULSOS_MIN_DETECCION_FLANCO` aíslan de forma determinística las falsas caídas antes de 150 pulsos.
   - Tras el reseteo, `limitePulsosActual = PULSOS_CELDA_MEDIA` (400) asegura que el robot recorre exactamente media celda desde la arista física.
   - En ausencia de paredes laterales, `limitePulsosActual` permanece en 800 (`PULSOS_CELDA`), cumpliendo el fallback exigido.

2. **Vulnerabilidad Crítica — Guarda de 100 pulsos en Parada Frontal (Escenario 3.3):**
   - En `src/main.cpp` línea 128:
     `stopPorParedFrontal = (sensadoActual.distanciaCent > 0 && sensadoActual.distanciaCent <= DISTANCIA_PARADA_FRENTE && pulsosTotalesCelda > 100);`
   - La inclusión de `pulsosTotalesCelda > 100` proviene de una confusión con la gracia de R3 (silencio del PID en los primeros 100 pulsos).
   - En una situación donde el robot inicia `AVANZANDO` estando a 30 mm o 45 mm de una pared (o ante un obstáculo inmediato), `stopPorParedFrontal` es evaluado como `false` durante los primeros 100 pulsos.
   - Simultáneamente, como `distanciaCent <= 120`, `paredAlFrente` es `true`, por lo que `stopPorEncoders` también se inhibe.
   - El robot ejecuta `movimiento(AVANZAR, ...)` durante 100 pulsos (equivalente a $22.5\text{ mm}$ de avance), impactando frontalmente contra la pared antes de que la guarda le permita detenerse.

3. **Vulnerabilidad de Ruido en Aproximación Frontal (Escenario 4.3):**
   - En `src/main.cpp` línea 138:
     `stopPorEncoders = (pulsosActuales >= limitePulsosActual) && !paredAlFrente;`
   - Una vez que `pulsosActuales >= 800`, el avance hacia la pared frontal depende exclusivamente de que `!paredAlFrente` sea falso (`paredAlFrente == true`).
   - Si el sensor ToF tiene una sola lectura ruidosa $> 120\text{ mm}$ (por dispersión óptica o ángulo), `paredAlFrente` pasa a `false` en ese ciclo, activando de inmediato `stopPorEncoders`. El robot frena bruscamente a 70–90 mm de la pared en vez de completar la alineación a 50 mm.

4. **Vulnerabilidad con Lecturas Negativas (Escenario 4.2):**
   - En `src/hardware/sensoresDistancia/sensoresDistancia.cpp` línea 97:
     `lecturaAct.distanciaCent = rawCent - OFSET_CENT;`
   - Si por tolerancia o calibración un obstáculo toca el paragolpes antes de 50 mm, `rawCent < 50` genera `distanciaCent < 0`.
   - La guarda estricta `distanciaCent > 0` trata una distancia negativa como lectura inválida / ausencia de pared, deshabilitando tanto `stopPorParedFrontal` como `paredAlFrente`.

---

## 3. Caveats

- Las pruebas empíricas se ejecutaron mediante simulación determinística y unit tests independientes reproduciendo fielmente los tipos primitivos C++, estructuras de datos y lógica del bucle de `src/main.cpp` y `src/config.h`. No se realizaron descargas directas a hardware físico ESP32 debido a la ausencia de microcontrolador físico conectado a la máquina de ejecución.
- No hay caveats adicionales.

---

## 4. Conclusion

**Veredicto:** **REQUEST_CHANGES**

La mecánica base de R1 (flanco lateral y fallback a 800 pulsos) y R3 (ceguera PID y bumpless transfer) es conceptualmente robusta y pasa las pruebas. Sin embargo, **R2 contiene una vulnerabilidad de seguridad crítica (violación de frenado seguro ante pared temprana $\le 50\text{ mm}$)** y dos vulnerabilidades de robustez que deben ser corregidas antes de aprobar Milestone M1:

### Acciones Requeridas para Worker:
1. **Eliminar la guarda de 100 pulsos en `stopPorParedFrontal`:**
   En `src/main.cpp` línea 128, cambiar:
   ```cpp
   // ANTES (INSEGURO):
   bool stopPorParedFrontal = (sensadoActual.distanciaCent > 0 &&
                               sensadoActual.distanciaCent <= DISTANCIA_PARADA_FRENTE &&
                               pulsosTotalesCelda > 100);
   // PROPUESTO (SEGURO):
   bool stopPorParedFrontal = (sensadoActual.distanciaCent > 0 &&
                               sensadoActual.distanciaCent <= DISTANCIA_PARADA_FRENTE);
   ```
   *(Justificación: Si hay pared a $\le 50\text{ mm}$, la detención debe ser inmediata e incondicional. El sensado ya está inicializado por `iniciarAvanceCelda()` y la guarda `distanciaCent > 0` previene lecturas en 0).*

2. **Robustecer `stopPorEncoders` contra glitches una vez superado el límite de encoders:**
   Si el robot ya pasó los 800 pulsos en aproximación a pared frontal, no debe permitir que un único ciclo ruidoso $> 120\text{ mm}$ desactive `paredAlFrente` y frene por encoders. Se recomienda enclavar la bandera de aproximación frontal (`aproximandoParedFrontal = true`) o validar que `distanciaCent` permanezca fuera del umbral durante múltiples muestras consecutivas.

3. **Tolerancia a lecturas de contacto o calibración cero/negativa:**
   Asegurar que distancias $\le 0$ pero $> -20$ (rango de contacto físico) sean consideradas obstáculo inmediato y no ignoradas como ausencia de pared.

---

## 5. Verification Method

Para verificar independientemente las pruebas y las vulnerabilidades detectadas:

1. **Ejecutar el harness de prueba Python:**
   Inspeccionar y ejecutar `verify_odometry_adversarial.py` en `.agents/teamwork/challenger_1/`.
2. **Inspeccionar `src/main.cpp` línea 128-130:**
   Verificar que en la línea 130 se encuentra `pulsosTotalesCelda > 100`, confirmando que un objeto a $\le 50\text{ mm}$ al inicio del estado `AVANZANDO` no dispara la detención.
3. **Condición de Invalidación de este Reporte:**
   Si se demostrara que una pared frontal a 30 mm al inicio de `AVANZANDO` puede frenar al robot sin superar los 100 pulsos, este reporte quedaría invalidado. Como el código actual en `src/main.cpp:130` exige estrictamente `pulsosTotalesCelda > 100`, la condición de falla queda matemáticamente y empíricamente comprobada.
