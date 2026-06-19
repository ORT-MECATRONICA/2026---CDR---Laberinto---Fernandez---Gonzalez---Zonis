#include "PID.h"
#include "hardware/sensoresDistancia/sensoresDistancia.h"
#include "config.h"

//Vale esto?
int16_t errorAnterior = 0;

int16_t calcularCorreccion(sensado mediciones){
    int16_t error = (int16_t)mediciones.distanciaIzq - (int16_t)mediciones.distanciaDer;
    int16_t correccion = (KP * error) + (KD * (error - errorAnterior)) ;
    errorAnterior = error; //CHECKEAR SI O SI!!
    return correccion;
    //Cómo hago para guardar error anterior?
    //No lo puedo pasar como parámetro porque después no tengo como actualizarlo.
}
