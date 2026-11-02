#pragma once
#include <stdint.h>
#include "hardware/sensoresDistancia/sensoresDistancia.h"

// [PID] velocidadBase = velocidad del momento (sin la corrección): el tope es configRobot.topePid % de ella
int16_t calcularCorreccion(sensado mediciones, int16_t velocidadBase);

// [NO UTILIZADA ACTUALMENTE - MANTENIDA POR REQUISITO]
int16_t calcularCorreccionRightHand(int16_t error);

void resetearErrorAnterior();
