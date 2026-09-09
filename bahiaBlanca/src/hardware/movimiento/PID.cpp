#include "PID.h"
#include "hardware/sensoresDistancia/sensoresDistancia.h"
#include "config.h"

static int16_t errorAnterior = 0;

int16_t calcularCorreccion(sensado mediciones){
    // Evaluamos si las paredes están presentes (menor al umbral normal + un margen)
    bool hayIzq = mediciones.distanciaIzq < (UMBRAL_PARED_ESTADO_NORMAL + 50);
    bool hayDer = mediciones.distanciaDer < (UMBRAL_PARED_ESTADO_NORMAL + 50);
    
    int16_t error = 0;
    
    if (hayIzq && hayDer) {
        // Ambas paredes presentes: centrarse entre ellas
        error = (int16_t)mediciones.distanciaIzq - (int16_t)mediciones.distanciaDer;
    } else if (hayIzq) {
        // Solo pared izquierda: mantenerse a la distancia ideal (OFSET_IZQ representa nuestro objetivo ideal)
        error = (int16_t)mediciones.distanciaIzq - OFSET_IZQ;
    } else if (hayDer) {
        // Solo pared derecha
        error = OFSET_DER - (int16_t)mediciones.distanciaDer;
    } else {
        // Ninguna pared presente: avanzar recto
        error = 0;
    }

    int16_t correccion = (KP * error) + (KD * (error - errorAnterior)) ;
    
    errorAnterior = error; 
    
    return correccion;
}

int16_t calcularCorreccionRightHand(int16_t error){
    int16_t correccion = (KP * error) + (KD * (error - errorAnterior)) ;
    
    errorAnterior = error; 
    
    return constrain(correccion, -50, 50);
}

void resetearErrorAnterior() {
    errorAnterior = 0;
}
