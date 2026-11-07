#pragma once

#define USAR_BLUETOOTH 1

//===================
// CONST GLOBALES
//===================

// Ganancia proporcional PID (valor por defecto, ahora calibrable dinámicamente)
#define KP 0.2f
// [NO UTILIZADA ACTUALMENTE - MANTENIDA POR REQUISITO]
#define KI 0
// [PID D] Ganancia derivativa por defecto (calibrable por BT con KD=valor). Antes "SI LE AGREGO
// ALGO NO VA A FUNCIONAR!!": el D estaba mal calculado (explicado en PID.cpp). KD=0 lo apaga.
#define KD 1.0f

// [E1] OJO: velocidades y pulsos de acá son solo los valores POR DEFECTO. El robot usa lo que
// está guardado en la Flash (lo que muestra VER por BT). Estos solo se usan si se manda DEFAULT
// o si la Flash está vacía. Para andar a 100 hay que mandar por BT: VD=100 y VI=100.
// Los de giro y de pulsos son los que tenía la Flash el 2026-11-06 (antes había valores viejos).

// Velocidades de avance [E1] 100/255
#define VEL_BASE_DER 100
#define VEL_BASE_IZQ 100

// [E12] Velocidad (PWM) para las partes que tienen que ser precisas: desde la primera lectura
// libre del lado del giro, todo el PREGIRO y cuando la pared de enfrente está a menos de
// DISTANCIA_LENTA (mm, ya con offset). El resto del tiempo va a VD/VI. Con los sensores midiendo
// cada ~33 ms, llegar rápido a la pared hace que se pase del corte y la trompa barra la pared al
// girar; llegar lento hace que frene siempre en el mismo lugar.
// Tiene que ser bastante más que lo mínimo con lo que arrancan los motores (los giros andan a 50).
#define VEL_LENTA 60
#define DISTANCIA_LENTA 150

// Velocidades de giro 90°
#define VEL_BASE_DER_GIRO 50
#define VEL_BASE_IZQ_GIRO 50

// Velocidades de giro 180°
#define VEL_BASE_DER_GIRO180 50
#define VEL_BASE_IZQ_GIRO180 50

// [NO UTILIZADA ACTUALMENTE - MANTENIDA POR REQUISITO]
#define UMBRAL_LECTURA 100

// Offsets que se restan a la lectura cruda de cada sensor (sensoresDistancia.cpp)
// [OFFSET DINÁMICO] OFSET_DER y OFSET_IZQ ahora son solo el valor inicial: se recalculan al
// apretar B1 en LISTO. OFSET_CENT sigue fijo.
#define OFSET_DER 47
#define OFSET_IZQ 40
#define OFSET_CENT 50

// [OFFSET DINÁMICO] Valor que deben dar los laterales (ya con offset) con el robot centrado.
// TIENE QUE SER IGUAL A SETPOINT_PARED de PID.cpp (55), si no el PID centra en otro lugar.
#define LECTURA_CENTRADO_LATERAL 55

// Pulsos de giros (valores por defecto)
#define PULSOS_GIRO_90_DER 220
#define PULSOS_GIRO_90_IZQ 260
#define PULSOS_GIRO_180 700      // [NO UTILIZADA DIRECTAMENTE - MANTENIDA POR REQUISITO]
#define PULSOS_GIRO_180_DER 480  // [180 OPUESTO] Ahora SÍ se usa (180 hacia la derecha). Calibrar por BT con G180D=valor
#define PULSOS_GIRO_180_IZQ 550

// PRED / PREI: pulsos desde la PRIMERA lectura libre del lado del giro hasta el centro de la celda
// [E12] Ahora el PREGIRO va a VEL_LENTA y frena más corto que a 120: casi seguro hay que SUBIR
// PRED y PREI respecto de lo que tenías (el log "FIN PREGIRO ... freno P:" dice dónde frenó).
#define PULSOS_PREGIRO_90_DER 150
#define PULSOS_PREGIRO_90_IZQ 200

// [E10] POST: pulsos que avanza (con PID) después de CADA giro sin buscar aberturas. Se cuentan
// desde el centro de la celda donde giró. Tienen que alcanzar justo para que los sensores
// laterales ENTREN en la celda siguiente: más o menos (pulsos de una celda) - PRED.
//  - Si POST es CORTO: en dos giros seguidos a la derecha (rodear la punta de una pared) la
//    derecha todavía ve libre el pasillo por el que vino y el segundo giro sale ANTES del
//    centro de la celda: choca contra la punta de la pared.
//  - Si POST es LARGO: después de un giro, si la celda siguiente tiene abertura, gira tarde.
// Pulsos de una celda: es el P de ">>> PARED AL FRENTE P:..." (cuenta desde el último giro o
// desde la largada) en un pasillo recto sin aberturas, dividido por las celdas recorridas.
#define PULSOS_POSTGIRO 300

// [NO UTILIZADA DIRECTAMENTE - MANTENIDA POR REQUISITO - SETPOINT_PARED HARDCODEADO EN PID.CPP]
#define SETPOINT 60 //Es el umbral de distancia medido en MM en el que se encuentra la pared si el robot está centrado

// Umbrales de sensores VL53L0X
#define UMBRAL_PARED_ESTADO_NORMAL 200 //EN BAHIA ESTABA 175. Más que esto = libre (abertura)

// [E3] Lectura del frente (mm, ya con offset) en la que la pared de enfrente deja al robot en el
// centro de la celda: ahí frena AVANZANDO y cortan PREGIRO y POSTGIRO. Antes era un 50 escrito a
// mano en cuatro lugares. ES EL QUE MANDA EL FRENADO CONTRA LA PARED DE ENFRENTE.
#define DISTANCIA_CORTE_FRENTE 50

// [E13] Filtro del sensor de enfrente (sensoresDistancia.cpp): una lectura que cambia más que esto
// (mm) respecto de la anterior se cree recién si la siguiente la confirma. Andando, el frente
// cambia mucho menos por lectura (~33 ms), así que no atrasa el frenado; una lectura falsa suelta
// sí salta mucho más y se descarta.
#define SALTO_FRENTE 60

// [E3] Ahora solo lo usa decidirQuieto, con el robot ya frenado contra la pared: menos que esto
// = pared al frente; más = la lectura que lo hizo frenar era falsa y sigue derecho.
// Tiene que ser bastante MÁS GRANDE que DISTANCIA_CORTE_FRENTE: si fueran iguales (50 y 50), un
// robot que frenó en 52 cree que el frente está libre, arranca, vuelve a leer 49 y frena otra vez
// (en la simulación, con 50 hubo el triple de frenadas inútiles contra la pared que con 100).
// Antes valía 50 porque además decidía cuándo empezar el PREGIRO hacia la pared; eso ya no existe.
#define UMBRAL_FRENTE 100

// [PID] Umbral para que el PID use una pared lateral (mm, ya con offset). Es más bajo que
// UMBRAL_PARED_ESTADO_NORMAL a propósito: con el robot en un pasillo la lectura real nunca pasa
// de ~100, y lo que está entre 110 y 170 son lecturas a medias de una pared que empieza o termina.
// [E6] También es el límite de "pared segura" del debounce: entre este valor y
// UMBRAL_PARED_ESTADO_NORMAL la lectura es dudosa (borde de una pared) y no reinicia la cuenta.
#define UMBRAL_PARED_PID 110

// [FIX 1] Tiempo (ms) que el robot queda frenado antes de resetear encoders y pasar a la
// maniobra siguiente. Absorbe la inercia para que no se cuente como pulsos del estado siguiente.
#define TIEMPO_FRENO_MS 450

// [DEBOUNCE] Cantidad de lecturas completas seguidas (los 3 sensores) con el lado prioritario
// libre que hacen falta para entrar en PREGIRO_DER (RIGHT HAND) o PREGIRO_IZQ (LEFT HAND)
#define LECTURAS_CONFIRMACION 3

// [E7] Watchdog de sensores: si uno pasa este tiempo (ms) sin dar una lectura válida con el
// robot andando, se reinician los 3. Cada sensor mide cada ~33 ms: 250 ms son ~7 lecturas.
#define TIEMPO_SENSOR_COLGADO_MS 250
// [E7] Reinicios de sensores permitidos en una corrida; si hacen falta más, el robot se frena.

// [E8] Tiempo máximo (ms) de un PREGIRO, un giro o un POSTGIRO. Si se pasa (rueda trabada,
// encoder suelto) el robot frena y avisa por qué. Es una red de seguridad: tiene que ser bastante
// más de lo que tarda la maniobra más lenta (un PREGIRO a VEL_LENTA con un reinicio de sensores
// en el medio puede llegar a ~2-3 s).
#define TIEMPO_MAX_MANIOBRA_MS 2500

//===================
//     PINES
//===================
//TODOS LOS PINES SE CHECKEARON
#define BOTON1 34
#define BOTON2 35

//Chequeados - motores
#define AIN1 14
#define AIN2 4
#define BIN1 16
#define BIN2 17
#define PWMA 12
#define PWMB 32
#define ENC_A_1 33
#define ENC_B_1 25
#define ENC_A_2 26
#define ENC_B_2 27

//===================
// PINES VL53L0X
//===================
#define xshutPinDer 18
#define xshutPinIzq 23
#define xshutPinCent 19
#define adressDer 0x30
#define adressIzq 0x31
#define adressCent 0x32
// [NO UTILIZADA ACTUALMENTE - MANTENIDA POR REQUISITO]
#define CNY 13
