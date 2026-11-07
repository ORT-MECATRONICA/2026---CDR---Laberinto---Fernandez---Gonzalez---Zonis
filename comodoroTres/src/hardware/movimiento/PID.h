#pragma once
#include <stdint.h>
#include "hardware/sensoresDistancia/sensoresDistancia.h"

int16_t calcularCorreccion(sensado mediciones);

// [NO UTILIZADA ACTUALMENTE - MANTENIDA POR REQUISITO]
int16_t calcularCorreccionRightHand(int16_t error);

void resetearErrorAnterior();
