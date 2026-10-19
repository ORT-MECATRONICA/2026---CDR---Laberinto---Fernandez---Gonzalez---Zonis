#pragma once

//===================
// CONST GLOBALES
//===================

#define X_SIZE 10
#define Y_SIZE 10

#define X_START 5
#define Y_START 5

#define KP 0.5
#define KI 0
#define KD 0.3 //SI LE AGREGO ALGO NO VA A FUNCIONAR!!    

#define VEL_BASE_DER 38 
#define VEL_BASE_IZQ 38

#define UMBRAL_LECTURA 100

#define OFSET_DER 47
#define OFSET_IZQ 40
#define OFSET_CENT 50

#define PULSOS_GIRO_180 700
//#define PULSOS_GIRO_90_DER 280
//#define PULSOS_GIRO_90_IZQ 290
#define PULSOS_POSTGIRO 200

#define SETPOINT 60 //Es el umbral de distancia medido en MM en el que se encuentra la pared si el robot está centrado



//Es el umbral de distancia medido en MM en el que se encuentra la pared si el robot está centrado
#define UMBRAL_PARED_ESTADO_NORMAL 170 
#define UMBRAL_FRENTE 100

//===================
//     PINES
//===================
//TODOS LOS PINES SE CHECKEARON
#define BOTON1 34
#define BOTON2 35
//Checheados - motores
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
#define CNY 13

#define PULSOS_PREGIRO_90_DER 300
#define PULSOS_PREGIRO_90_IZQ 10 

