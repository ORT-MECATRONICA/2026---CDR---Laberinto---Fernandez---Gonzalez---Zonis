#include "PID.h"
#include "hardware/sensoresDistancia/sensoresDistancia.h"
#include "hardware/calibracion/calibracion.h"
#include "config.h"

// [NO UTILIZADAS ACTUALMENTE - MANTENIDAS POR REQUISITO]
// KI = 0
// KD = 0.3

// Valores fijados hardcodeados según especificación
static const int16_t TOPE = 8;             // Máx. corrección (con base 40 es 20%)
static const int16_t SETPOINT_PARED = 55;  // mm que lee un sensor cuando está centrado

int16_t calcularCorreccion(sensado mediciones) {
    bool hayIzq = mediciones.distanciaIzq > 0 && mediciones.distanciaIzq < UMBRAL_PARED_ESTADO_NORMAL;
    bool hayDer = mediciones.distanciaDer > 0 && mediciones.distanciaDer < UMBRAL_PARED_ESTADO_NORMAL;

    int16_t error;
    if (hayIzq && hayDer) {
        error = mediciones.distanciaIzq - mediciones.distanciaDer;
    } else if (hayDer) {
        error = 2 * (SETPOINT_PARED - mediciones.distanciaDer);
    } else if (hayIzq) {
        error = 2 * (mediciones.distanciaIzq - SETPOINT_PARED);
    } else {
        return 0;
    }

    // Ganancia proporcional dinámica calibrable vía Bluetooth / Preferences
    return constrain((int16_t)(configRobot.kp * error), -TOPE, TOPE);
}

void resetearErrorAnterior() {
    // Ya no hay estado; se deja para no romper las llamadas existentes
}