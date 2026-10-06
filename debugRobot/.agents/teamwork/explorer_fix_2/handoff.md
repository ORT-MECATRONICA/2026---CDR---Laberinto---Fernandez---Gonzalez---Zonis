# Reporte de Handoff — Explorer Fix 2 (Milestone M1 Iteration 2)

**Agente:** Explorer Fix 2 (`teamwork_preview_explorer`)  
**Fecha:** 2026-10-06  
**Ruta de Trabajo:** `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\.agents\teamwork\explorer_fix_2`  
**Destinatario:** Parent (`eeb17d92-d8a4-4a39-a22e-d0c4d184202b`) / Worker 2  
**Misión:** Análisis de vulnerabilidad ante picos de ruido óptico ToF en aproximación a pared frontal y diseño de mecanismo de cerrojo (latch) / histéresis preservando el watchdog de seguridad.

---

## 1. Observation

### 1.1 Código Fuente Inspeccionado
En `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\src\main.cpp`:
- **Líneas 27-34:** Variables globales de celda y odometría:
  ```cpp
  // Variables para control de celda y odometría adaptativa (R1, R2, R3, R4)
  uint16_t limitePulsosActual = PULSOS_CELDA;
  bool flancoDetectado = false;
  bool habiaParedIzq = false;
  bool habiaParedDer = false;
  uint32_t pulsosBaseCelda = 0;
  bool graciaPIDFinalizada = false;
  ```
  *Observación:* No existe variable de estado ni cerrojo de memoria para recordar que el robot ya inició la aproximación final a una pared frontal más allá del límite de encoders.

- **Líneas 39-51:** Función `iniciarAvanceCelda()`:
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

- **Líneas 126-149:** Lógica de parada en `case AVANZANDO:`:
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

  if (stopPorParedFrontal || stopPorEncoders || stopPorWatchdog) {
    enviarString(">>> INGRESO A DECISIÓN (FRENANDO) <<<");
    movimiento(FRENO_F, {0,0});
    tiempoInicioFreno = millis();
    estadoPostFreno = DECISION;
    estado = FRENANDO;
  }
  ```

En `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\src\config.h`:
- **Líneas 37, 66-71:**
  ```cpp
  #define UMBRAL_PARED_FRENTE 120
  ...
  #define PULSOS_CELDA 800
  #define PULSOS_CELDA_MEDIA 400
  #define DISTANCIA_PARADA_FRENTE 50
  #define PULSOS_GRACIA_PID 100
  #define PULSOS_MIN_DETECCION_FLANCO 150
  #define PULSOS_WATCHDOG_SEGURIDAD 1050
  ```

### 1.2 Evidencia Empírica de Falla (Challenger 1 Report & Test Harness)
En `c:\Users\marti\OneDrive\Desktop\SOFTWARE\Laberinto\debugRobot\.agents\teamwork\challenger_1\verify_odometry_adversarial.py` (Líneas 320-338):
- Test adversarial 4.3 reprodujo la vulnerabilidad:
  ```python
  # Robot at 820 pulses, distance is 70mm, but single glitch reads 135mm at pulse 820
  for p in range(0, 1500, 10):
      if p == 820:
          d_cent = 135  # Glitch
      elif p > 800:
          d_cent = 70
      else:
          d_cent = 180
      state = sim.step_loop(Sensado(cent=d_cent, der=200, izq=200), delta_pulsos=10)
  ```
- Resultado verbatim reportado por Challenger 1:
  `[FAIL/VULN] 4.3 ADVERSARIAL: Noise Spike > 120mm past 800p | VULNERABILITY CONFIRMED: Single spike to 135mm at 820p caused premature ENCODERS stop at 820p while still 70mm from wall!`

---

## 2. Logic Chain

1. **Causa Raíz del Fallo (Evaluación Instantánea Sin Memoria):**
   - Una vez que `pulsosActuales >= limitePulsosActual` (por ejemplo, 800 pulsos nominales o 400 post-flanco), la condición `pulsosActuales >= limitePulsosActual` permanece permanentemente en `true`.
   - Por ende, la expresión `bool stopPorEncoders = (pulsosActuales >= limitePulsosActual) && !paredAlFrente;` se reduce estrictamente a `stopPorEncoders = !paredAlFrente;` en cada ciclo de ejecución del bucle.
   - En sensores Time-of-Flight (VL53L0X), la dispersión óptica o reflexiones especulares producen espurias transitorias donde una muestra puntual puede medir $> 120\text{ mm}$ (por ejemplo, $135\text{ mm}$ o saturación a $2000\text{ mm}$).
   - En el instante exacto en que ocurre esa lectura espuria, `paredAlFrente` pasa a `false` durante ese único ciclo.
   - Al no tener memoria ni cerrojo, `!paredAlFrente` evalúa a `true`, disparando inmediatamente `stopPorEncoders = true`.
   - La máquina de estados transiciona de forma irreversible a `FRENANDO` y luego a `DECISION`. El robot frena a $70\text{ mm} - 90\text{ mm}$ de la pared, violando el requerimiento R2 que exige avanzar hasta $\le 50\text{ mm}$.

2. **Propiedad Física Inmutable del Entorno:**
   - En un laberinto de micromouse, las paredes físicas son estructuras fijas de madera o plástico.
   - Si el robot se encuentra en la segunda mitad de la celda ($> 700$ pulsos) y ha detectado una pared frontal a $\le 120\text{ mm}$, **la pared física no puede desaparecer súbitamente en el ciclo siguiente**.
   - Por lo tanto, una lectura aislada $> 120\text{ mm}$ durante la fase de aproximación frontal es necesariamente ruido óptico y nunca una desaparición física de la pared.

3. **Mecanismo de Cerrojo Biestable (`aproximandoParedFrontal`):**
   - Se introduce la bandera de estado `bool aproximandoParedFrontal = false;`.
   - Se limpia determinísticamente en `iniciarAvanceCelda()` al comenzar cada nueva celda.
   - **Activación del Cerrojo:**
     Cuando el robot detecta pared al frente (`paredAlFrente == true`) en la ventana de aproximación final:
     ```cpp
     if (paredAlFrente && (pulsosActuales >= limitePulsosActual || pulsosActuales >= (limitePulsosActual - 100))) {
       aproximandoParedFrontal = true;
     }
     ```
     *Justificación de la ventana anticipada `(limitePulsosActual - 100)`:* Si el pico de ruido ocurriese exactamente en el pulso 800, el robot ya habrá enclavado `aproximandoParedFrontal = true` en los pulsos 750-790 (donde la pared ya estaba a $\approx 75-90\text{ mm}$), evitando que una espuria en el pulso 800 gane la carrera contra el cerrojo.
   - **Inhibición de Parada por Encoders:**
     La condición de parada por encoders se redefine como:
     ```cpp
     bool stopPorEncoders = (pulsosActuales >= limitePulsosActual) && !paredAlFrente && !aproximandoParedFrontal;
     ```
     Una vez enclavada `aproximandoParedFrontal = true`, el término `!aproximandoParedFrontal` es `false`, suprimiendo de forma absoluta la parada por encoders para el resto del avance de la celda.

4. **Preservación Incondicional del Watchdog de Seguridad (`PULSOS_WATCHDOG_SEGURIDAD = 1050`):**
   - Si por falla de hardware catastrófica (ej. cable de sensor ToF desconectado, sensor trabado, o pared no reflectiva) el sensor nunca llega a leer $\le 50\text{ mm}$, el robot no puede quedar avanzando indefinidamente.
   - En `src/main.cpp` línea 141:
     `bool stopPorWatchdog = (pulsosTotalesCelda >= PULSOS_WATCHDOG_SEGURIDAD);`
   - `stopPorWatchdog` es completamente independiente de `paredAlFrente`, `aproximandoParedFrontal` y `sensadoActual.distanciaCent`.
   - Al alcanzar `1050` pulsos ($236.25\text{ mm}$ de avance total en la celda), `stopPorWatchdog` fuerza la transición unificada a `FRENANDO`, disipando la energía cinética y evitando la colisión mecánica.

5. **Inmunidad en Corredores Abiertos (Sin Pared Frontal):**
   - En una celda sin pared frontal, el sensor central lee continuamente $> 200\text{ mm}$ (pasillo despejado).
   - `paredAlFrente` es `false` en todo momento; por lo tanto, `aproximandoParedFrontal` permanece en `false`.
   - Al llegar a `pulsosActuales == limitePulsosActual` (800 pulsos nominales o 400 post-flanco), `stopPorEncoders` evalúa `true && true && true = true`.
   - El robot se detiene exactamente en el centro de la celda, cumpliendo R1 y el plan B de odometría nominal sin perturbaciones.

---

## 3. Caveats

- **No Caveats:** El análisis cubre todas las ramas de la máquina de estados en `AVANZANDO`, reseteos por flanco de R1, celdas abiertas sin pared frontal, picos de ruido óptico en cualquier instante ($\le 800$, $== 800$, $> 800$), y fallos completos del sensor central mitigados por el watchdog de 1050 pulsos.
- La recomendación no altera ninguna interfaz de hardware ni añade estructuras complejas, cumpliendo estrictamente con la simplicidad arquitectónica requerida.

---

## 4. Conclusion

El diagnóstico de Challenger 1 respecto al SPOF en `stopPorEncoders` es exacto. La solución óptima y minimalista para Worker 2 consiste en:
1. Agregar la variable booleana `bool aproximandoParedFrontal = false;` en `src/main.cpp`.
2. Reinicializarla en `iniciarAvanceCelda()` (`aproximandoParedFrontal = false;`).
3. Enclavar `aproximandoParedFrontal = true;` cuando se confirme presencia de pared frontal en la zona final de la celda (`pulsosActuales >= limitePulsosActual - 100 && paredAlFrente`).
4. Reemplazar la condición de `stopPorEncoders` por:
   `bool stopPorEncoders = (pulsosActuales >= limitePulsosActual) && !paredAlFrente && !aproximandoParedFrontal;`
5. Mantener inalterado el watchdog de seguridad `stopPorWatchdog = (pulsosTotalesCelda >= PULSOS_WATCHDOG_SEGURIDAD);` como salvaguarda incondicional.

### Especificación de Cambios para Worker 2 (Before -> After):

#### En `src/main.cpp`:
1. **Líneas 27-34:**
   ```cpp
   // ANTES:
   uint16_t limitePulsosActual = PULSOS_CELDA;
   bool flancoDetectado = false;
   bool habiaParedIzq = false;
   bool habiaParedDer = false;
   uint32_t pulsosBaseCelda = 0;
   bool graciaPIDFinalizada = false;

   // DESPUÉS:
   uint16_t limitePulsosActual = PULSOS_CELDA;
   bool flancoDetectado = false;
   bool habiaParedIzq = false;
   bool habiaParedDer = false;
   uint32_t pulsosBaseCelda = 0;
   bool graciaPIDFinalizada = false;
   bool aproximandoParedFrontal = false;
   ```

2. **Líneas 39-51 (`iniciarAvanceCelda`):**
   ```cpp
   // ANTES:
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

   // DESPUÉS:
   void iniciarAvanceCelda() {
     resetearEncoders();
     resetearErrorAnterior();
     limitePulsosActual = PULSOS_CELDA;
     flancoDetectado = false;
     habiaParedIzq = false;
     habiaParedDer = false;
     pulsosBaseCelda = 0;
     graciaPIDFinalizada = false;
     aproximandoParedFrontal = false;
     sensadoActual = actualizarSensado();
     enviarString(">>> AVANZANDO <<<");
     estado = AVANZANDO;
   }
   ```

3. **Líneas 126-148 (`case AVANZANDO:`):**
   ```cpp
   // ANTES:
   // 2. Detección de pared frontal en aproximación
   bool paredAlFrente = (sensadoActual.distanciaCent > 0 &&
                         sensadoActual.distanciaCent <= UMBRAL_PARED_FRENTE);

   // 3. Parada por encoders (400 pulsos si hubo flanco, o 800 fallback si no hubo flanco).
   // R2 sobreescribe la parada por encoders si hay pared al frente.
   bool stopPorEncoders = (pulsosActuales >= limitePulsosActual) && !paredAlFrente;

   // 4. Watchdog de seguridad (anti-colisión ante fallo de sensor frontal)
   bool stopPorWatchdog = (pulsosTotalesCelda >= PULSOS_WATCHDOG_SEGURIDAD);

   // DESPUÉS (con integración coordinada de Explorer Fix 1 y Fix 3 para DISTANCIA_MIN_VALIDA):
   // 2. Detección de pared frontal en aproximación
   bool paredAlFrente = (sensadoActual.distanciaCent >= DISTANCIA_MIN_VALIDA &&
                         sensadoActual.distanciaCent <= UMBRAL_PARED_FRENTE);

   // Enclavar aproximación a pared frontal: una vez confirmada cerca del límite o más allá,
   // un pico transitorio (> 120 mm) no podrá desactivarla.
   if (paredAlFrente && (pulsosActuales >= limitePulsosActual || pulsosActuales >= (limitePulsosActual - 100))) {
     aproximandoParedFrontal = true;
   }

   // 3. Parada por encoders: solo frena si NO hay pared al frente y NO se enclavó aproximación.
   bool stopPorEncoders = (pulsosActuales >= limitePulsosActual) && !paredAlFrente && !aproximandoParedFrontal;

   // 4. Watchdog de seguridad (anti-colisión incondicional ante fallo de sensor frontal)
   bool stopPorWatchdog = (pulsosTotalesCelda >= PULSOS_WATCHDOG_SEGURIDAD);
   ```

---

## 5. Verification Method

### 5.1 Verificación Lógica y de Código
1. **Inspección de `src/main.cpp`:**
   - Confirmar la existencia de `bool aproximandoParedFrontal = false;`.
   - Confirmar su reseteo en `iniciarAvanceCelda()`.
   - Confirmar el cerrojo `aproximandoParedFrontal = true;` en `AVANZANDO`.
   - Confirmar que `stopPorEncoders` incluye `!aproximandoParedFrontal`.
   - Confirmar que `stopPorWatchdog` permanece intacto e independiente evaluando `pulsosTotalesCelda >= PULSOS_WATCHDOG_SEGURIDAD`.

### 5.2 Matriz de Validación de Escenarios

| Escenario | Condición de Entrada | Comportamiento Esperado | Criterio de Aprobación |
|---|---|---|---|
| **E1: Glitch Óptico a 820p (Test 4.3)** | Pared a 70 mm, glitch de 135 mm en pulso 820 | El robot ignora el pulso 820 y avanza hasta $\le 50\text{ mm}$ | `pulsosTotalesCelda > 820`, parada final por `stopPorParedFrontal` |
| **E2: Glitch Óptico a 800p** | Pared a 75 mm en 790p, glitch de 135 mm en 800p | El robot tenía enclavado `aproximandoParedFrontal = true` desde 790p; no frena en 800p | Parada final en $\le 50\text{ mm}$ |
| **E3: Corredor Abierto (Test 1.4)** | Sin paredes laterales ni frontales | `aproximandoParedFrontal` se mantiene `false`; frena exactamente en 800p | Parada por `stopPorEncoders` en 800p |
| **E4: Sensor Frontal Desconectado/Trabado (Test 4.1)** | Sensor lee 70 mm continuo y nunca llega a 50 mm | `stopPorWatchdog` frena incondicionalmente a 1050 pulsos | Parada por `stopPorWatchdog` en 1050p |
| **E5: Transición de Celda** | Nueva celda iniciada tras `DECISION` | `aproximandoParedFrontal` se resetea a `false` en `iniciarAvanceCelda()` | Variable limpia al pulso 0 de la nueva celda |

### 5.3 Condiciones de Invalidación
Este reporte quedaría invalidado si:
1. Se demostrara que en un pasillo abierto (`distanciaCent > 200 mm`), `aproximandoParedFrontal` puede ser activada por una lectura nominal. (Imposible: la condición exige `distanciaCent <= 120 mm`).
2. Se demostrara que una falla de hardware en el sensor VL53L0X permite que el robot sobrepase los 1050 pulsos sin frenar. (Imposible: `stopPorWatchdog` evalúa exclusivamente `pulsosTotalesCelda >= 1050`, sin depender de sensores ópticos).
