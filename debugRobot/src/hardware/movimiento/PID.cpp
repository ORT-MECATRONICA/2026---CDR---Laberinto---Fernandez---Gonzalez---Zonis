#include "PID.h"
#include "hardware/sensoresDistancia/sensoresDistancia.h"
#include "config.h"


static int16_t errorAnterior = 0;
static uint32_t tiempoAnterior = 0;

int16_t calcularCorreccion(sensado mediciones){   
    bool hayIzq = mediciones.distanciaIzq < UMBRAL_PARED_ESTADO_NORMAL;
    bool hayDer = mediciones.distanciaDer < UMBRAL_PARED_ESTADO_NORMAL;
    uint32_t deltaT = millis() - tiempoAnterior;
    if (hayIzq && hayDer) {
        if(deltaT >= 30){
            int16_t error = (int16_t)mediciones.distanciaIzq - (int16_t)mediciones.distanciaDer;
            int16_t correccion = (KP * error) + (KD * (error - errorAnterior)) ;
            errorAnterior = error;
            tiempoAnterior = millis();
            return constrain(correccion, -10, 10);
        } 
    } else if (hayDer){
        if(deltaT >= 30){
            int16_t error = (int16_t)mediciones.distanciaDer - SETPOINT;
            int16_t correccion = (KP * error) + (KD * (error - errorAnterior)) ;
            errorAnterior = error;
            tiempoAnterior = millis();
            return constrain(correccion, -10, 10);
        }
    } else if (hayIzq) {
        if(deltaT >= 30){
            int16_t error = SETPOINT - (int16_t)mediciones.distanciaIzq;
            int16_t correccion = (KP * error) + (KD * (error - errorAnterior)) ;
            errorAnterior = error;
            tiempoAnterior = millis();
            return constrain(correccion, -10, 10);
        }
    } else {
        return 0;
    }
}  

void resetearErrorAnterior() {
    errorAnterior = 0;
}