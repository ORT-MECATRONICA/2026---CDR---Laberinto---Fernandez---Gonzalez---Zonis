#include "PID.h"
#include "hardware/sensoresDistancia/sensoresDistancia.h"
#include "config.h"

int16_t calcularCorreccion(sensado mediciones){
    static int16_t errorAnterior = 0;
    
    int16_t error = (int16_t)mediciones.distanciaIzq - (int16_t)mediciones.distanciaDer;
    int16_t correccion = (KP * error) + (KD * (error - errorAnterior)) ;
    
    errorAnterior = error; 
    
    return correccion;
}
