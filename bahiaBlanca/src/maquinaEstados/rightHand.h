#pragma once

#include "config.h"
#include "hardware/sensoresDistancia/sensoresDistancia.h"
#include "hardware/encoders/encoders.h"
#include "hardware/logger/logger.h"
#include "hardware/movimiento/puenteH.h"
#include "hardware/movimiento/PID.h"
#include "main.h"

ESTADOS right_hand();

enum SUBESTADOS {
    AVANZANDO,
    PREPARANDOME_PARA_GIRAR_DER,
    GIRANDO_DER,
    PREPARANDOME_PARA_GIRAR_IZQ,
    GIRANDO_IZQ,
    GIRANDO_180,
    POST_GIRO_AVANZAR,
    FIN,
};

struct FILTRO_DEBOUNCE {
    uint8_t apertDer;
    uint8_t apertIzq;
    uint8_t paredFrente;
    uint8_t callejon;
};

struct FILTRO_SENSORES {
    uint8_t counterIzq;
    uint8_t counterDer;
    uint8_t counterCent;
};

