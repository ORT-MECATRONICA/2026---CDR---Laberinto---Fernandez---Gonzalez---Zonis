#include "main.h"
#include "config.h"
#include "hardware/sensoresDistancia/sensoresDistancia.h"
#include "hardware/encoders/encoders.h"
#include "hardware/logger/logger.h"
#include "hardware/movimiento/puenteH.h"
#include "maquinaEstados/leftHand.h"
#include "maquinaEstados/rightHand.h"
#include "maquinaEstados/mapeo.h"

ESTADOS estadoActual = HUB;

void setup(){
    inicializacionSensoresDist();
    inicializarEncoders();
    inicializarLogger();
    inicializarMotores();
}

void loop(){
    switch(estadoActual){
        case HUB: {
                if(digitalRead(BOTON1) == LOW && digitalRead(BOTON2) == HIGH ){
                    estadoActual = RIGHT_HAND;
                } else if(digitalRead(BOTON1) == HIGH && digitalRead(BOTON2) == LOW){
                    estadoActual = LEFT_HAND;
                } else if(digitalRead(BOTON1) == LOW && digitalRead(BOTON2) == LOW){
                    estadoActual = MAPEO;
                } else {
                    estadoActual = HUB;
                }
            }
            break;
        case RIGHT_HAND: {
               right_hand();
               estadoActual = HUB;
            break;
        }
        case LEFT_HAND: {
                left_hand();
                estadoActual = HUB;
            break;
        }
        case MAPEO: {
                mapeo();
                estadoActual = HUB;
            break;
        }
    }
}