/* 
================================
    Máscaras para mapeo de bits
================================
    Autor: Martín Zonis
    Fecha: 2026 - 05 - 04
    Descripcion: Este archivo almacena las funciones que permiten cargar información al mapa.
*/
#include "funcionesMapeo.h"
#include <stdint.h>
#include <arduino.h>
#include "hardware/logger/logger.h"

static uint8_t mapa[X_SIZE][Y_SIZE]; //Mapa de celdas, cada celda es un uint8_t que almacena la información de esa celda.

void posicionInicial(uint8_t X, uint8_t Y){
    mapa[X][Y] |= 0b00100000;
}

void posicionFinal(uint8_t X, uint8_t Y){
    mapa[X][Y] |= 0b01000000;
}

//============================================

void paredNorte(uint8_t X, uint8_t Y){
    mapa[X][Y] |= 0b00010000;
}

void paredSur(uint8_t X, uint8_t Y){
    mapa[X][Y] |= 0b00001000;
   
}

void paredEste(uint8_t X, uint8_t Y){
    mapa[X][Y] |= 0b00000100;
}

void paredOeste(uint8_t X, uint8_t Y){
    mapa[X][Y] |= 0b00000010;
}

//============================================

void celdaVisitada(uint8_t X, uint8_t Y){
    mapa[X][Y] |= 0b00000001;
}

void celdaBloqueada(uint8_t X, uint8_t Y){
    mapa[X][Y] |= 0b10000000;
}

void celdaDesbloqueada(uint8_t X, uint8_t Y){
    mapa[X][Y] &= ~(0b10000000);
    //REVISAR Y PROBAR ESTE MÉTODO, ES RARI
}

//============================================

bool askParedNorte(uint8_t X, uint8_t Y){
    uint8_t celdaSolicitada = mapa[X][Y];
    if (celdaSolicitada & 0b00010000!= 0){
        return true;
    } else {
        return false;
    }
}   

bool askParedSur(uint8_t X, uint8_t Y){
    uint8_t celdaSolicitada = mapa[X][Y];
    if (celdaSolicitada & 0b00001000!= 0){
        return true;
    } else {
        return false;
    }
}   

bool askParedEste(uint8_t X, uint8_t Y){
    uint8_t celdaSolicitada = mapa[X][Y];
    if (celdaSolicitada & 0b00000100!= 0){
        return true;
    } else {
        return false;
    }
}   

bool askParedOeste(uint8_t X, uint8_t Y){
    uint8_t celdaSolicitada = mapa[X][Y];
    if (celdaSolicitada & 0b00000010!= 0){
        return true;
    } else {
        return false;
    }
}   

bool askCeldaVisitada(uint8_t X, uint8_t Y){
    uint8_t celdaSolicitada = mapa[X][Y];
    if (celdaSolicitada & 0b00000001!= 0){
        return true;
    } else {
        return false;
    }
}   

bool askPosicionInicial(uint8_t X, uint8_t Y){
    uint8_t celdaSolicitada = mapa[X][Y];
    if (celdaSolicitada & 0b00100000!= 0){
        return true;
    } else {
        return false;
    }
}   

bool askPosicionFinal(uint8_t X, uint8_t Y){
    uint8_t celdaSolicitada = mapa[X][Y];
    if (celdaSolicitada & 0b01000000!= 0){
        return true;
    } else {
        return false;
    }
}   

bool askCeldaBloqueada(uint8_t X, uint8_t Y){
    uint8_t celdaSolicitada = mapa[X][Y];
    if (celdaSolicitada & 0b10000000 != 0){
        return true;
    } else {
        return false;
    }
}   
//  return (celdaSolicitada & 00010000) ? true : false;