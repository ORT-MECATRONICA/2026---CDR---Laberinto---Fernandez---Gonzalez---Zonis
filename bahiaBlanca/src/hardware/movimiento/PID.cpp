#include "PID.h"
#include <Arduino.h>
#include "hardware/sensoresDistancia/sensoresDistancia.h"
#include "config.h"

static int16_t errorAnterior = 0;

int16_t calcularCorreccion(sensado mediciones) {
    bool paredIzqValida = (mediciones.distanciaIzq > 0) && (mediciones.distanciaIzq <= UMBRAL_PARED_VALIDA_PID);
    bool paredDerValida = (mediciones.distanciaDer > 0) && (mediciones.distanciaDer <= UMBRAL_PARED_VALIDA_PID);

    int16_t error = 0;

    if (paredIzqValida && paredDerValida) {
        // Ambas paredes presentes: centrado diferencial (signo positivo vira hacia la derecha)
        error = (int16_t)mediciones.distanciaDer - (int16_t)mediciones.distanciaIzq;
    } else {
        // Si alguna de las dos paredes no es válida (o sea que es una intersección o giro), NO HAY PID
        error = 0;
        errorAnterior = 0;
    }

    int16_t correccion = (int16_t)((KP * error) + (KD * (error - errorAnterior)));
    errorAnterior = error;

    return constrain(correccion, -MAX_CORRECCION_PID, MAX_CORRECCION_PID);
}

void resetearErrorAnterior() {
    errorAnterior = 0;
}
