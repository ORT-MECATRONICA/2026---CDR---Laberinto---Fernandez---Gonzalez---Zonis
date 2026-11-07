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

// Velocidades de avance (CRUCERO: la que usa cuando no sospecha nada)
#define VEL_BASE_DER 45
#define VEL_BASE_IZQ 45

// [SOSPECHA] Velocidad cuando algo puede cambiar (pared del lado prioritario dudosa o ausente,
// pared al frente a ~1 celda). Calibrable por BT con VS=valor
#define VEL_SOSPECHA 70
// [SOSPECHA] Velocidad cuando viene un freno: abertura del lado prioritario, PREGIRO y pared cerca
// al frente. Más baja todavía. 45 = la velocidad con la que se calibraron PRED, PREI y POST.
// Calibrable por BT con VP=valor
// Nunca es más rápida que la anterior: si VS o VP quedan más altas que la base, se usa la base.
#define VEL_PREGIRO 45

// [RAMPA] Cada PERIODO_RAMPA_MS la velocidad se acerca al objetivo de a PASO_ACELERACION (subiendo)
// o PASO_DESACELERACION (bajando). Sube despacio para que no patinen las ruedas y baja rápido
// para llegar lento al pregiro. Calibrables por BT con ACEL=valor y DESAC=valor
#define PERIODO_RAMPA_MS 5
#define PASO_ACELERACION 2
#define PASO_DESACELERACION 5

// [PID] Máxima corrección del PID en % de la velocidad del momento (antes TOPE = 8 fijo en PID.cpp,
// que con base 45 es 18%). Así sirve igual en crucero, sospecha y pregiro. Calibrable por BT con TOPE=valor
#define TOPE_PID 18

// Velocidades de giro 90°
#define VEL_BASE_DER_GIRO 45
#define VEL_BASE_IZQ_GIRO 45

// Velocidades de giro 180°
#define VEL_BASE_DER_GIRO180 55
#define VEL_BASE_IZQ_GIRO180 55

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
#define PULSOS_GIRO_90_DER 300
#define PULSOS_GIRO_90_IZQ 340
#define PULSOS_GIRO_180 700      // [NO UTILIZADA DIRECTAMENTE - MANTENIDA POR REQUISITO]
#define PULSOS_GIRO_180_DER 700  // [180 OPUESTO] Ahora SÍ se usa (180 hacia la derecha). Calibrar por BT con G180D=valor
#define PULSOS_GIRO_180_IZQ 525

#define PULSOS_PREGIRO_90_DER 400
#define PULSOS_PREGIRO_90_IZQ 200
#define PULSOS_POSTGIRO 200

// [NO UTILIZADA DIRECTAMENTE - MANTENIDA POR REQUISITO - SETPOINT_PARED HARDCODEADO EN PID.CPP]
#define SETPOINT 60 //Es el umbral de distancia medido en MM en el que se encuentra la pared si el robot está centrado

// Umbrales de sensores VL53L0X
// [NO UTILIZADA ACTUALMENTE - MANTENIDA POR REQUISITO] Reemplazadas por UMBRAL_PARED / UMBRAL_LIBRE
#define UMBRAL_PARED_ESTADO_NORMAL 200 //EN BAHIA ESTABA 175
#define UMBRAL_FRENTE 100

// [DIGITAL] Los sensores se usan como digitales (hay pared / no hay pared). Menos de UMBRAL_PARED
// = pared seguro; más de UMBRAL_LIBRE = libre seguro; en el medio la lectura es DUDOSA: no cambia
// nada y queda lo último seguro. mm, ya con offset. Calibrables por BT con UPARED=valor y ULIBRE=valor
#define UMBRAL_PARED 85
#define UMBRAL_LIBRE 130

// [NO UTILIZADA ACTUALMENTE - MANTENIDA POR REQUISITO] [C6] La reemplaza el perfil de frenado
// por frente (DISTANCIA_FRENADO_FRENTE). Además el comentario viejo suponía celdas de 18 cm.
#define UMBRAL_FRENTE_SOSPECHA 250
// [FRENTE] Pared de enfrente a esta distancia = robot en el centro de la celda. Corta el pregiro
// y el postgiro, y en AVANZANDO frena para decidir quieto (antes 50 en main)
// [C12] Ahora es solo el valor por defecto: se calibra por BT con CORTE=valor
#define DISTANCIA_CORTE_FRENTE 50

// [C6] [FRENADO POR FRENTE] mm (de lectura del sensor del frente) antes del punto de corte en los
// que empieza a frenar. Lejos de la pared va a crucero; a DFRENO mm del corte empieza a bajar en
// línea recta hasta llegar a VP justo en el corte. Más chico = frena más tarde (más rápido, más
// riesgo). 250 = una celda. Calibrable por BT con DFRENO=valor
#define DISTANCIA_FRENADO_FRENTE 250

// [NO UTILIZADA ACTUALMENTE - MANTENIDA POR REQUISITO] [C8] El PID ahora usa UMBRAL_PARED (UPARED):
// solo paredes SEGURAS. Con 110 entraban lecturas dudosas (85..110) de paredes que terminan.
#define UMBRAL_PARED_PID 110

// [C4] [UNA CELDA] Pulsos que cuenta leerPulsos() al avanzar UNA celda (25 cm) en línea recta.
// Se usa cuando aparece una abertura del lado prioritario SIN haber visto antes la pared de ese
// lado desde el último centro de celda (ej. dos giros seguidos: rodear la punta de una pared).
// En ese caso no hay borde de pared para contar el pregiro y el giro va en el centro de la celda
// siguiente. 0 = sin calibrar: se comporta como OTOÑO (cuenta PRED desde la primera lectura libre).
// Calibrable por BT con PCELDA=valor (el log "PARED AL FRENTE" muestra los pulsos desde el centro)
#define PULSOS_CELDA 0

// [C9] [VETO] Lecturas SEGURAS de pared seguidas, del lado del giro, durante el pregiro para
// cancelarlo (antes alcanzaba con que la última lectura segura fuera de pared: un dato malo
// cancelaba un giro real)
#define LECTURAS_PARED_VETO 2

// [C7] [GIRO] Pulsos del final del giro en los que va bajando la velocidad hasta VP. Así frena
// siempre desde la misma velocidad y el ángulo final no depende de la batería ni de VG.
// 0 = sin zona (como OTOÑO). Calibrable por BT con ZGIRO=valor
#define ZONA_FRENO_GIRO 80

// [C2] [SENSORES 50 HZ] Tiempo de cada medición de los VL53L0X en microsegundos.
// 20000 = 20 ms (50 Hz): ve las aberturas y la pared de enfrente ~13 ms antes, con un poco más
// de ruido. 33000 = como OTOÑO (~30 Hz). No bajar de 20000.
#define PRESUPUESTO_SENSOR_US 20000

// [C3] [WATCHDOG SENSORES] Si un sensor no entrega una lectura válida en este tiempo, se considera
// colgado: el robot frena, reinicia los sensores y sigue. En LISTO avisa por BT.
#define TIEMPO_SENSOR_COLGADO_MS 150
#define INTENTOS_REINICIO_SENSORES 3

// [C1] [LOGS] Cuántos mensajes se guardan mientras el robot anda (se mandan cuando frena)
#define MAX_LOGS_GUARDADOS 8

// [FIX 1] [NO BLOQUEANTE] Tiempo MÁXIMO (ms) que el robot queda en FRENANDO antes de resetear
// encoders y pasar a la maniobra siguiente (antes era un delay fijo de 450). Ahora sale apenas las
// ruedas paran (encoders quietos), pero nunca antes de TIEMPO_FRENO_MIN_MS.
#define TIEMPO_FRENO_MS 300
#define TIEMPO_FRENO_MIN_MS 30
#define PERIODO_CHEQUEO_FRENO_MS 10

// [DEBOUNCE] Cantidad de lecturas SEGURAS de libre (más de UMBRAL_LIBRE) del lado prioritario
// para entrar en PREGIRO. Las dudosas no cuentan; una de pared vuelve a cero.
#define LECTURAS_CONFIRMACION 3

// [WATCHDOG] Si una maniobra (pregiro, giro, postgiro) no termina en este tiempo
// (rueda trabada, VP demasiado baja), se da por terminada para que el robot no quede colgado
#define TIEMPO_MAX_MANIOBRA_MS 3000

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
