#pragma once
#include <stdint.h>
#include "hardware/sensoresDistancia/sensoresDistancia.h"

int16_t calcularCorreccion(sensado mediciones);

// [POST POST GIRO] Igual que calcularCorreccion pero con KP doble y TOPE doble, para corregir
// rápido lo que dejó el giro. La usa el estado POST_POST_GIRO.
// (Antes no estaba declarada acá, por eso main.cpp no la podía llamar.)
int16_t calcularCorreccionPOSTIGRO(sensado mediciones);

// [NO UTILIZADA ACTUALMENTE - MANTENIDA POR REQUISITO]
int16_t calcularCorreccionRightHand(int16_t error);

void resetearErrorAnterior();
