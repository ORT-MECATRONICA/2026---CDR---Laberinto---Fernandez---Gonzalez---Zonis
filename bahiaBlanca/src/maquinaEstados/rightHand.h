#pragma once

#include "config.h"
#include "hardware/sensoresDistancia/sensoresDistancia.h"
#include "hardware/encoders/encoders.h"
#include "hardware/logger/logger.h"
#include "hardware/movimiento/puenteH.h"
#include "hardware/movimiento/PID.h"

bool right_hand();

enum ESTADOS {
    AVANZANDO,
    PREPARANDOME_PARA_GIRAR_DER,
    GIRANDO_DER,
    PREPARANDOME_PARA_GIRAR_IZQ,
    GIRANDO_IZQ,
    FIN,
};

struct FILTRO_SENSORES {
    uint8_t counterIzq;
    uint8_t counterDer;
    uint8_t counterCent;
};

