#pragma once
#include <Arduino.h>

enum MOVIMIENTOS{
    //A todo le falta tilde, en pretérito porque son acciones pasadas.
    AVANZO,
    RETROCEDIO,
    GIRO_DER,
    GIRO_IZQ,
    FRENO_F,
};

struct VELOCIDAD {
    int8_t izquierda;
    int8_t derecha;
};

void avanzar(VELOCIDAD velocidad);
void inicializarMotores();
bool actualizarDeltaX();