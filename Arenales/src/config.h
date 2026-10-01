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
#define KD 0.3

#define VEL_BASE_DER 65
#define VEL_BASE_IZQ 65

#define VEL_GIRO_DER 100
#define VEL_GIRO_IZQ 100

#define UMBRAL_LECTURA 100

#define OFSET_DER 47
#define OFSET_IZQ 40
#define OFSET_CENT 80

#define PULSOS_90_GRADOS 110 // (Ajustado desde 300 porque daba 1 vuelta entera)
#define PULSOS_AVANCE_PREGIRO 250 // Reducido para que no avance de más en la intersección
#define PULSOS_AVANCE_PREGIRO_IZQ 100 // Reducido para que no avance de más en la intersección
#define PULSOS_AVANZAR_BLOQUEANTE 550 // (Aprox 1 vuelta de rueda ciega despues de doblar)

//Es el tiempo de delay del Freno F. Es BLOQUEANTE
#define DELAY_TIEMPO_FRENADO_EN_F 750

//Es el umbral de distancia medido en MM en el que se encuentra la pared si el robot está centrado
#define UMBRAL_PARED_ESTADO_NORMAL 100 

//Es el umbral (MM) para que el robot gire si la pared está frente a él
#define UMBRAL_PARED_FRENTE 120
//===================
//     PINES
//===================
//TODOS LOS PINES SE CHECKEARON
#define BOTON1 34
#define BOTON2 35
//Checheados - motores
#define AIN1 14
#define AIN2 17
#define BIN1 16
#define BIN2 4
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
//===================
// CONSTANTES DE TIEMPO (ms)
// Reemplazan a los PULSOS de los encoders
//===================
#define TIEMPO_90_GRADOS 350
#define TIEMPO_AVANCE_PREGIRO 400
#define TIEMPO_AVANZAR_BLOQUEANTE 500

