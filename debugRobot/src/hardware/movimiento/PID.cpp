#include "PID.h"
#include "hardware/sensoresDistancia/sensoresDistancia.h"
#include "config.h"


static int16_t errorAnterior = 0;
static uint32_t tiempoAnterior = 0;
int16_t calcularCorreccion(sensado mediciones){   
    bool hayIzq = mediciones.distanciaIzq < UMBRAL_PARED_ESTADO_NORMAL;
    bool hayDer = mediciones.distanciaDer < UMBRAL_PARED_ESTADO_NORMAL;
    if (hayIzq && hayDer) {
        int16_t error = (int16_t)mediciones.distanciaIzq - (int16_t)mediciones.distanciaDer;
        int16_t correccion = (KP * error) + (KD * (error - errorAnterior)) ;
        if(millis() - tiempoAnterior >= 30){
            errorAnterior = error;
            tiempoAnterior = millis();
        } 
        return constrain(correccion, -10, 10);
    } else {
        return 0;
    }
    return 0;
}  

void resetearErrorAnterior() {
    errorAnterior = 0;
}