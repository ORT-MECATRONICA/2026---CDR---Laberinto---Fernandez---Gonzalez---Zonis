#include "PID.h"
#include "hardware/sensoresDistancia/sensoresDistancia.h"
#include "config.h"

//Vale esto?
uint8_t errorAnterior = 0;

uint8_t calcularCorreccion(sensado mediciones){
    uint8_t error = mediciones.distanciaIzq - mediciones.distanciaDer;
    uint8_t correccion = (KP * error) + (KD * (error - errorAnterior)) ;
    errorAnterior = error; //CHECKEAR SI O SI!!
    return correccion;
    //Cómo hago para guardar error anterior?
    //No lo puedo pasar como parámetro porque después no tengo como actualizarlo.
    
}