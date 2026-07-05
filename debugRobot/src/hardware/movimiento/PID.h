#pragma once
#include <stdint.h>
#include "hardware/sensoresDistancia/sensoresDistancia.h"
int16_t calcularCorreccion(sensado mediciones);
int16_t calcularCorreccionRightHand(int16_t error);
