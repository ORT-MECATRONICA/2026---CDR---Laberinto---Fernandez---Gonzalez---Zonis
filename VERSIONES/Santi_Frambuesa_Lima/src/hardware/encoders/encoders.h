#pragma once
#include <stdint.h>

//Función para inicializar los encoders
void inicializarEncoders();

//Función para ver el pulso del encoder A
int32_t verPulsosEncoderA();

//Función para ver el pulso del encoder B
int32_t verPulsosEncoderB();

//Función para reiniciar los contadores de los encoders
void reiniciarEncoders();