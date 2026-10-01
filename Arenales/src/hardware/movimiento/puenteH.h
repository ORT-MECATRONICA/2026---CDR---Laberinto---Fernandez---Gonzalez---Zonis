#pragma once
#include <Arduino.h>

enum MOVIMIENTOS{
    AVANZAR,
    RETROCEDER,
    GIRAR_DER,
    GIRAR_IZQ,
    FRENO_F,
};

struct VELOCIDAD {
    int16_t izquierda;
    int16_t derecha;
};

void movimiento(MOVIMIENTOS movimiento, VELOCIDAD velocidad);
void inicializarMotores();
bool actualizarDeltaX();