#pragma once
#include <stdint.h>
#include "hardware/sensoresDistancia/sensoresDistancia.h"

// [PID] velocidadBase = velocidad del momento (sin la corrección): el tope es configRobot.topePid % de ella
// [C8] usarIzq / usarDer: false = ese lado no se usa aunque lea pared (ej. el lado que se está abriendo)
int16_t calcularCorreccion(sensado mediciones, int16_t velocidadBase, bool usarIzq = true, bool usarDer = true);

// [NO UTILIZADA ACTUALMENTE - MANTENIDA POR REQUISITO]
int16_t calcularCorreccionRightHand(int16_t error);

void resetearErrorAnterior();
