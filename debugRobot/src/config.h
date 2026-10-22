#pragma once

#define USAR_BLUETOOTH 1

//===================
// CONST GLOBALES
//===================

// Ganancia proporcional PID (valor por defecto, ahora calibrable dinámicamente)
#define KP 0.2f
// [NO UTILIZADAS ACTUALMENTE - MANTENIDAS POR REQUISITO]
#define KI 0
#define KD 0.3 //SI LE AGREGO ALGO NO VA A FUNCIONAR!!    

// Velocidades de avance
#define VEL_BASE_DER 45
#define VEL_BASE_IZQ 45

// Velocidades de giro 90°
#define VEL_BASE_DER_GIRO 45
#define VEL_BASE_IZQ_GIRO 45

// Velocidades de giro 180°
#define VEL_BASE_DER_GIRO180 55
#define VEL_BASE_IZQ_GIRO180 55

// [NO UTILIZADA ACTUALMENTE - MANTENIDA POR REQUISITO]
#define UMBRAL_LECTURA 100

// [NO UTILIZADAS ACTUALMENTE - MANTENIDAS POR REQUISITO]
#define OFSET_DER 47
#define OFSET_IZQ 40
#define OFSET_CENT 50

// Pulsos de giros (valores por defecto)
#define PULSOS_GIRO_90_DER 300
#define PULSOS_GIRO_90_IZQ 340
#define PULSOS_GIRO_180 700      // [NO UTILIZADA DIRECTAMENTE - MANTENIDA POR REQUISITO]
#define PULSOS_GIRO_180_DER 700  // [NO UTILIZADA DIRECTAMENTE - MANTENIDA POR REQUISITO]
#define PULSOS_GIRO_180_IZQ 525

#define PULSOS_PREGIRO_90_DER 400
#define PULSOS_PREGIRO_90_IZQ 200
#define PULSOS_POSTGIRO 200

// [NO UTILIZADA DIRECTAMENTE - MANTENIDA POR REQUISITO - SETPOINT_PARED HARDCODEADO EN PID.CPP]
#define SETPOINT 60 //Es el umbral de distancia medido en MM en el que se encuentra la pared si el robot está centrado

// Umbrales de sensores VL53L0X
#define UMBRAL_PARED_ESTADO_NORMAL 170 
#define UMBRAL_FRENTE 100

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
