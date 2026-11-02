#pragma once
#include <Arduino.h>

// Modos de seguidor de pared
enum MODO_SEGUIDOR {
    MODO_RIGHT_HAND,
    MODO_LEFT_HAND
};

/*
// [MAQUINA DE ESTADOS ANTERIOR - MANTENIDA POR REQUISITO]
enum MAQUINA_ESTADOS {
    LISTO,
    INFORMACION_RECIBIDA,
    AVANZANDO,
    DECISION,
    GIRANDO_DER,
    GIRANDO_IZQ,
    GIRANDO_180,
    FRENANDO
};
*/

enum MAQUINA_NUEVA {
    LISTO,
    INFORMACION_RECIBIDA,
    ESPERANDO_LARGADA,
    AVANZANDO,
    DECISION,            // [DECISION] Ahora SÍ se usa: decide quieto frente a una pared (antes decidirQuieto)
    PREGIRO_DER,
    PREGIRO_IZQ,
    APROXIMANDO_FRENTE,  // [FRENTE] Se acerca lento a la pared de enfrente hasta el centro de la celda
    POSTGIRO,
    GIRANDO_DER,
    GIRANDO_IZQ,
    GIRANDO_180,
    FRENANDO             // [NO BLOQUEANTE] Freno hasta que paran las ruedas (antes delay(TIEMPO_FRENO_MS))
};

// [VOTOS] Lecturas seguras de un sensor durante una maniobra (las dudosas no votan)
struct VOTOS {
    uint8_t pared;
    uint8_t libre;
};
