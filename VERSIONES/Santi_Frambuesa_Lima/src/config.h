#pragma once

//===================
// CONST GLOBALES
//===================

#define KP 1
#define KI 0
#define KD 0.3

#define VEL_BASE_DER 100
#define VEL_BASE_IZQ 100

#define VEL_GIRO_DER 100
#define VEL_GIRO_IZQ 100

#define UMBRAL_LECTURA 150
#define UMBRAL_FRENO_F 150

#define OFSET_DER 0
#define OFSET_IZQ 0
#define OFSET_CENT 0
//Este es un factor que se multiplica por la distancia medida para obtener la distancia real. Se utiliza para corregir errores de medición del sensor. 
//HAY QUE AJUSTARLO EMPIRICAMENTE, ES REALMENTE INSOPORTABLE,
#define FACTOR_MILTON 0.5

#define UMBRAL_TIEMPO_DIGITALIZACION 2000
#define CANT_LECTURAS_IGNORADAS 10

//Es el tiempo de delay del Freno F. Es BLOQUEANTE
#define DELAY_TIEMPO_FRENADO_EN_F 750

//Tiempo de delay para girar 90 grados. Es BLOQUEANTE. (Ajustar empíricamente)
#define DELAY_TIEMPO_GIRO_90 2000

//Es el umbral de distancia medido en MM en el que se encuentra la pared si el robot está centrado
#define UMBRAL_PARED_ESTADO_NORMAL 150 

//Es el umbral (MM) para que el robot gire si la pared está frente a él
#define UMBRAL_PARED_FRENTE 120
//===================
//     PINES
//===================

#define BOTON 35 

//Checheados - motores
#define AIN1 25
#define AIN2 33
#define BIN1 27
#define BIN2 26
#define PWMA 32
#define PWMB 14
#define ENC_A_1 16
#define ENC_B_1 17
#define ENC_A_2 4
#define ENC_B_2 2
//===================
// PINES VL53L0X
//===================
#define xshutPinDer 23
#define xshutPinIzq 18
#define xshutPinCent 19
#define adressDer 0x30
#define adressIzq 0x31
#define adressCent 0x32

