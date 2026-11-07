# comodoroTres — main "INVIERNO"

Copia de **comodoroDos (OTOÑO)** con los arreglos de los choques. La máquina de estados es la
misma (mismos estados, mismo orden de prioridades, misma DECISION quieta solo con pared al
frente). Cada cambio está marcado en el código con su etiqueta **[C1] … [C12]** para que lo
puedas buscar, entender y sacar por separado. OTOÑO sigue intacto en `comodoroDos` y en
`src/VERSIONES/OTOÑO.txt`.

Valores de la Flash con los que razoné (los que me pasaste): KP 0.2, KD 1, PRED 150, PREI 200,
POST 300, G90D 220, G90I 260, G180D 480, G180I 550, VD = VI = 120, VG 50, V180 50, VS 70,
VP 45, ACEL 2, DESAC 5, TOPE 18 %, UPARED 85, ULIBRE 130. Celdas de 25 cm, ruedas atrás de
todo (la trompa sobresale), sin pasar pulsos a milímetros y sin usar la diferencia entre
encoders para ir derecho (lo pediste así).

---

## 1. Por qué se choca

### 1.1 Llegando a una pared de frente

| Causa | Qué pasaba |
|---|---|
| **POSTGIRO no miraba el frente** | Iba a VS (70) hasta hacer los 300 pulsos o hasta que el frente bajara de 50. Si al salir de un giro había pared cerca, llegaba al corte a VS y frenaba desde ahí: más rápido que VP, más distancia de frenado. |
| **Atraso del sensor** | Cada VL53L0X mide ~33 ms y la lectura es el promedio de esos 33 ms. Cuando el loop ve "menos de 50", el robot ya está entre 1 y 3 cm más cerca de lo que dice la lectura (depende de la velocidad). Con las ruedas atrás, cuando el eje está en el centro de la celda la trompa ya está cerca de la pared: queda poco margen para pasarse. |
| **Bluetooth trabando el loop** | `SerialBT.println` espera hasta **1 s por paquete** si el enlace se congestiona (cola de 32 paquetes en `BluetoothSerial.cpp`; cada `println` son 2). OTOÑO mandaba mensajes **con los motores andando**: "PREGIRO…", "CANCELADO" y "FIN POSTGIRO" (este último justo antes de volver a AVANZANDO, que es el que frena contra la pared). Si el celu estaba conectado y el enlace se trababa, el robot seguía a ciegas a la última velocidad. |
| **Sensor colgado** | Si un sensor deja de contestar por I2C (ruido de motores, falso contacto), queda su última lectura para siempre y `nueva` no llega más: con el del frente colgado nunca ve la pared. Solo avisaba por USB al arrancar. |
| **Viene de un giro malo** | Si sale torcido de un giro (ver 1.2), el sensor del frente ve la pared en ángulo; con el haz inclinado el VL53L0X puede devolver una distancia más larga y frena tarde. |

### 1.2 Justo después de girar

- **POSTGIRO iba sin PID** y es el tramo ciego más largo (300 pulsos). Cualquier error de ángulo del
  giro, o un motor un poco más rápido que el otro, se acumulaba hasta la pared lateral.
- **PREGIRO también iba sin PID.** Al pasar de AVANZANDO (con corrección) a PREGIRO (las dos ruedas
  iguales), la dirección cambiaba de golpe justo antes de girar, y el giro arrancaba con el ángulo
  que tuviera el robot en ese momento: 90° sobre un ángulo malo = sale torcido.
- **El "volantazo" hacia la abertura.** El PID usaba una pared hasta 110 mm (`UMBRAL_PARED_PID`),
  o sea también lecturas *dudosas* (85–110) de una pared que se está terminando. Como esa pared
  "se aleja", el PID tira el robot hacia la abertura justo antes del pregiro: gira corrido.
- **VS y VP perdían la diferencia VD/VI.** Hoy no afecta (VD = VI = 120), pero si algún día los
  separás para compensar un motor, en pregiro y postgiro la compensación desaparecía.

### 1.3 Dos giros seguidos (rodear la punta de una pared)

```
        +-------+-------+
        |   X → | → Y   |     1) Viene subiendo por X y gira a la derecha en el centro de X.
        |   ↑   |   ↓   |     2) Ahora la derecha mira el pasillo por el que vino: LIBRE.
        |   ↑   ‖   ↓   |     3) Tiene que volver a girar a la derecha en el centro de Y.
        |   ↑   ‖   ↓   |        ‖ = la pared que está rodeando (su punta toca X|Y).
```

OTOÑO ponía los encoders en cero en la **primera lectura libre** de AVANZANDO y contaba PRED desde
ahí. Después de un giro a la derecha, esa primera lectura libre aparece apenas termina el POSTGIRO
(la derecha nunca vio pared: es el pasillo por el que vino). Entonces el segundo giro se hacía a
**POST + PRED ≈ 300 + 150 = 450 pulsos** del centro de X, cuando el centro de Y está a **una celda
entera**. Gira antes de tiempo, en la línea de la pared que estaba rodeando, y se la come (es lo que
viste). PRED está pensado para contar desde el **borde** de una pared que termina, y en este caso no
hay borde.

---

## 2. Los cambios

### [C1] Logs que no traban el loop — `logger.cpp`, `main.cpp`
- Mientras el robot anda, los mensajes se **guardan** con `guardarLog()` (hasta 8, con el `millis()`
  adelante) y se mandan con `enviarLogsGuardados()` cuando el robot **ya está quieto**: al salir de
  FRENANDO (ruedas paradas), en DECISION y en LISTO.
- Como frena en cada giro, los ves casi enseguida. Si se llena la cola, avisa cuántos se perdieron
  (nunca bloquea).
- Los mensajes con el robot quieto (DECISION, largada, errores) siguen saliendo directo.

### [C2] Sensores a 20 ms (50 Hz) — `config.h` (`PRESUPUESTO_SENSOR_US`), `sensoresDistancia.cpp`
- `setMeasurementTimingBudget(20000)` a cada sensor antes de `startContinuous()`.
- Ve aberturas y paredes ~13 ms antes y confirma las 3 lecturas en 40 ms en vez de 66 ms.
- A cambio hay un poco más de ruido. Para volver a como estaba: `PRESUPUESTO_SENSOR_US 33000`.

### [C3] Watchdog de sensores — `sensoresDistancia.cpp`, `main.cpp` (`revisarSensores()`)
- Cada sensor anota cuándo dio su última lectura válida. Si alguno pasa **150 ms** sin dar una
  (`TIEMPO_SENSOR_COLGADO_MS`) mientras el robot anda guiado por sensores (AVANZANDO, PREGIRO,
  POSTGIRO): frena, avisa por BT, apaga y prende los 3 sensores (igual que en el setup) y **sigue
  en el mismo estado**. Si después de 3 intentos no vuelven, se queda en LISTO.
- Durante los giros no corta (el giro no usa sensores): lo revisa al terminar.
- Al apretar B1 en LISTO, si algún sensor no arrancó, lo reintenta y si sigue sin andar **no larga**
  y te avisa cuál. Los errores de arranque ahora salen también por BT (antes solo USB).

### [C4] Los encoders se ponen en cero solo en el centro de una celda + PCELDA — `main.cpp`
Arregla **dos giros seguidos**.
- Los encoders se resetean solo con el robot **quieto en un centro**: al terminar un giro (antes del
  POSTGIRO), en DECISION y en la largada (`marcarCentroDeCelda()`). Así `leerPulsos()` es siempre
  "cuánto avancé desde el último centro".
- En vez de poner los encoders en cero en la primera lectura libre, se **anota** dónde fue el
  **borde**: la primera lectura libre después de una que no era libre. Se anota en cualquier
  estado, también en POSTGIRO (por si la punta de una pared pasa mientras sale del giro).
- Al confirmar la abertura (3 lecturas, igual que antes):
  - **Con borde** (desde el último centro vio pared, o algo dudoso, de ese lado): el pregiro
    termina **PRED/PREI pulsos después del borde**. Es lo mismo que hacía OTOÑO.
  - **Sin borde** (desde el último centro ese lado siempre leyó libre): es el pasillo por el que
    vino. El pregiro termina en el **centro de la celda siguiente = PCELDA pulsos desde el centro**
    (nunca antes que PRED desde la primera lectura libre). Mientras falta más que un pregiro normal
    va a VS; cerca del final, a VP.
  - Si en el camino vuelve la pared de ese lado (2 lecturas seguras, ver [C9]), cancela sin frenar:
    era el pasillo por el que vino y la celda siguiente tiene pared.
- **PCELDA** (nuevo, en pulsos, lo calibrás vos): pulsos de una celda de 25 cm. **Vale 0 hasta que
  lo calibres**, y con 0 ese caso se comporta como OTOÑO (al largar te avisa). Cómo medirlo: en la
  sección 3.

### [C5] (descartado) Compensar el atraso del sensor con la velocidad de los encoders
Lo pensé para que el pregiro no dependiera de la velocidad de llegada, pero es calcular avance con
los encoders, que pediste que no. En su lugar el atraso se achica con [C2] y la velocidad de llegada
se empareja con [C6].

### [C6] Frenado según la distancia a la pared de enfrente — `main.cpp` (`limitarPorFrente()`)
- Antes había escalones: VS si el frente leía menos de 250 y VP si leía 130 o menos. Ahora hay un
  tope continuo: a más de **DFRENO** mm del punto de corte no limita; desde ahí baja **en línea
  recta** hasta llegar a **VP justo en el corte**.
- Con DFRENO = 250 (una celda) y tus valores (crucero 120, VP 45):

  | Distancia al corte | 250 | 200 | 150 | 100 | 50 | 20 | 0 |
  |---|---|---|---|---|---|---|---|
  | Tope de PWM | 120 | 105 | 90 | 75 | 60 | 51 | 45 |

  Llega al corte a VP, igual que antes, pero sin ir a 45 durante los últimos 8 cm: es más rápido y
  frena parejo (sin escalones).
- Se aplica en **AVANZANDO, PREGIRO y POSTGIRO** (el POSTGIRO antes no miraba el frente).
- Calibrable por BT: `DFRENO=valor`. Más chico = frena más tarde (más rápido, más riesgo).

### [C7] Final del giro más lento — `main.cpp` (`girarHasta()`)
- En los últimos **ZGIRO** pulsos del giro (80 por defecto), la velocidad baja en línea recta
  hasta VP. Así siempre frena desde la misma velocidad y el sobregiro (lo que gira de más por
  inercia) no depende de la batería ni de VG.
- Con tu VG = 50 casi no cambia nada (baja de 50 a 45), así que G90/G180 deberían seguir sirviendo.
  La gracia es que ahora **podés subir VG** para girar más rápido sin que cambie tanto el ángulo
  final. `ZGIRO=0` lo apaga.

### [C8] PID solo con paredes seguras, y también en PREGIRO y POSTGIRO — `PID.cpp`, `main.cpp`
- El PID ahora usa una pared solo si lee **menos de UPARED (85)**: una pared segura. Antes usaba
  hasta 110 (`UMBRAL_PARED_PID`, que quedó sin uso). En un pasillo de 25 cm siempre hay al menos
  una pared segura (si un lado lee más de 85, el otro lee menos de 25).
- `calcularCorreccion()` recibe qué lados puede usar:
  - **AVANZANDO**: desde la primera lectura libre, el lado prioritario sale del PID (es el que se
    está abriendo). Esto saca el "volantazo" hacia la abertura.
  - **PREGIRO**: PID solo con la pared del lado **contrario** al giro. Llega al giro centrado y
    derecho, y no hay cambio brusco al pasar de AVANZANDO a PREGIRO.
  - **POSTGIRO**: PID con las paredes que haya. Si el giro lo dejó torcido o corrido, lo corrige
    enseguida en vez de ir derecho a la pared.
- Si no hay ninguna pared segura, sigue en lazo abierto como antes (sin usar los encoders).

### [C9] Veto del pregiro más robusto — `main.cpp` (`pasoPregiro()`)
- Antes, al final del pregiro alcanzaba con que la **última** lectura segura del lado del giro
  fuera de pared para cancelar: un solo dato malo cancelaba un giro real (y después giraba tarde).
- Ahora cancela cuando hay **2 lecturas seguras de pared seguidas** (`LECTURAS_PARED_VETO`) del lado
  del giro durante el pregiro (las dudosas no cuentan; una libre vuelve a cero). Y cancela en el
  momento en que pasa, sin esperar al final.

### [C10] VS y VP mantienen la diferencia VD/VI — `main.cpp` (`velocidadMaxima()`)
Si hay que limitar la velocidad, las dos ruedas se achican en la misma proporción. Hoy no cambia
nada (VD = VI), pero ya no se pierde la compensación si algún día las separás.

### [C11] Derivada normalizada — `PID.cpp`
Con lecturas cada 20 ms, el cambio del error **por lectura** es más chico y el mismo KD frenaría
menos. Ahora la pendiente se mide en una ventana de ~100 ms (6 lecturas de 20 ms, como antes 4 de
33 ms) y se expresa "por cada 33 ms". **Tu KD = 1 significa lo mismo que antes**, no hay que tocarlo.

### [C12] CORTE calibrable — `config.h`, `calibracion.cpp`
`DISTANCIA_CORTE_FRENTE` (el 50 que significa "eje en el centro de la celda") ahora se cambia por BT
con `CORTE=valor`. Lo usan el freno de AVANZANDO, el fin del pregiro, el fin del postgiro y [C6].

---

## 3. Qué hacer en la primera prueba (en este orden)

1. **Subir el firmware y mandar `VER`.** Tienen que aparecer las líneas nuevas: PCELDA 0, CORTE 50,
   DFRENO 250, ZGIRO 80. Lo demás queda como lo tenías en la Flash.
2. **Sensores a 20 ms.** Con el robot centrado entre dos paredes, apretá B1 y mirá el mensaje de
   OFFSETS (tiene que dar parecido a antes). Si en la corrida aparecen aberturas falsas, volvé a
   `PRESUPUESTO_SENSOR_US 33000`.
3. **CORTE.** Poné el robot en un pasillo con pared adelante y dejalo frenar solo. Si el eje no
   queda en el centro de la celda, ajustá `CORTE` (más grande = frena más lejos de la pared).
4. **PCELDA** (para dos giros seguidos). Poné el robot en el centro de una celda con pared a la
   derecha todo el tramo (para que no gire) y una pared al frente a 2 o 3 celdas. Largalo: cuando
   frene contra la pared vas a ver `PARED AL FRENTE ... P:xxx`. `PCELDA = P / celdas recorridas`.
   Repetilo 2 o 3 veces y usá el promedio. Mandalo con `PCELDA=valor`.
5. **PRED/PREI.** Siguen contando desde el borde, pero con sensores a 20 ms el borde se ve antes:
   si ahora gira un poco antes del centro, subilos de a 10.
6. **Ir más rápido** (de a un paso por vez, probando cada uno):
   - Bajá `DFRENO` de a 25 hasta que empiece a pasarse del corte, y dejalo 25–50 arriba de ahí.
   - Subí `VD`/`VI` de a 10. Mirá el `P:` y el `O:` del log de PREGIRO: si `P` llega cerca de `O`,
     está llegando demasiado rápido a las aberturas (subí `DESAC` para que baje más rápido a VP).
   - Subí `VGD`/`VGI` (y `V180D`/`V180I`) de a 10 con ZGIRO en 80; si cambia el ángulo, retocá
     G90/G180.

Claves BT nuevas: `PCELDA`, `CORTE`, `DFRENO`, `ZGIRO`.

---

## 4. Qué no hice (y por qué)

- **No pasé pulsos a milímetros** ni usé el diámetro de la rueda: todo lo nuevo de encoders está en
  pulsos que calibrás vos (PCELDA, ZGIRO), como PRED y POST.
- **No usé la diferencia entre encoders para ir derecho.** Sin paredes, sigue en lazo abierto.
- **Prueba de encoders**: dejé `TESTEOS/pruebaEncodersMotores.txt` (se copia en `main.cpp`, como
  las otras pruebas). Dice qué encoder corresponde a cada motor y para qué lado cuenta. Hoy ningún
  cambio depende de eso; sirve si más adelante querés control por rueda.
- **Giros en curva sin frenar**: es lo próximo para subir mucho la velocidad, pero con sensores a 90°
  el borde de la abertura se ve cuando ya habría que estar doblando. Haría falta un sensor en
  diagonal o calcular la posición en la celda.
- **Leer el estado de cada medición del VL53L0X** (el registro "range status") para descartar
  lecturas inválidas: posible mejora si siguen apareciendo lecturas raras.

## 5. Ojo con esto

- El frenado [C6] es **más rápido que los escalones de OTOÑO** en los últimos 10–20 cm (aunque
  llega a VP en el corte). Si en la primera prueba se pasa del corte, subí `DFRENO`.
- Hasta que calibres **PCELDA**, dos giros seguidos se siguen comportando como OTOÑO.
- No lo pude probar en el robot: compila limpio (Flash 88,5 %), pero los números de arranque
  (DFRENO 250, ZGIRO 80) son razonables, no medidos.

## Archivos tocados

| Archivo | Cambios |
|---|---|
| `src/main.cpp` | Nuevo main INVIERNO: [C1] [C3] [C4] [C6] [C7] [C8] [C9] [C10] [C12] |
| `src/config.h` | Constantes nuevas y comentarios de las que quedaron sin uso |
| `src/hardware/calibracion/*` | PCELDA, CORTE, DFRENO, ZGIRO (BT + Flash + VER) |
| `src/hardware/logger/*` | [C1] `guardarLog()` / `enviarLogsGuardados()` |
| `src/hardware/sensoresDistancia/*` | [C2] 20 ms, [C3] watchdog y reinicio |
| `src/hardware/movimiento/PID.*` | [C8] lados habilitados y paredes seguras, [C11] D normalizada |
| `TESTEOS/pruebaEncodersMotores.txt` | Prueba nueva de encoders/motores |
