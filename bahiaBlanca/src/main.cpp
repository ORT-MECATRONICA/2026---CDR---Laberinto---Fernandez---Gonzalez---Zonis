#include "main.h"
#include "config.h"
#include "hardware/sensoresDistancia/sensoresDistancia.h"
#include "hardware/encoders/encoders.h"
#include "hardware/logger/logger.h"
#include "hardware/movimiento/puenteH.h"
#include "maquinaEstados/leftHand.h"
#include "maquinaEstados/rightHand.h"
#include "maquinaEstados/mapeo.h"
//=================== ATENCIÓN ===================
ESTADOS estadoActual = RIGHT_HAND; //Para la etapa de la semana del 21, solo simulamos con Right hand porque no hay botones

void setup(){
    inicializacionSensoresDist();
    inicializarEncoders();
    inicializarLogger();
    inicializarMotores();
    Serial.println("Inicialización correcta");
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
               estadoActual = right_hand();
            break;
        }
        case LEFT_HAND: {
                estadoActual = left_hand();
            break;
        }
        case MAPEO: {
                estadoActual = mapeo();
            break;
        }
    }
}