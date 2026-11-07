#pragma once
#include <stdint.h>

//Función para inicializar los encoders
void inicializarEncoders();

//Función para ver el pulso del encoder A
int32_t verPulsosEncoderA();

//Función para ver el pulso del encoder B
int32_t verPulsosEncoderB();

//Función para resetear encoders
void resetearEncoders();

// [E2] Pulsos desde el último reset: promedio de |A| y |B| (vale para avanzar y para girar)
int32_t leerPulsos();