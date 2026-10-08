#include "PID.h"
#include "hardware/sensoresDistancia/sensoresDistancia.h"
#include "config.h"

static int16_t errorAnterior = 0;
static uint32_t tiempoAnterior = 0;
static int16_t correccionAnterior = 0;
static float errorIntegral = 0; // Se agrega para la constante KI
static bool primerCiclo = true; // Bandera para evitar el Derivative Kick

int16_t calcularCorreccion(sensado mediciones){
    bool hayIzq = mediciones.distanciaIzq < UMBRAL_PARED_ESTADO_NORMAL;
    bool hayDer = mediciones.distanciaDer < UMBRAL_PARED_ESTADO_NORMAL;
    
    int16_t error = 0;
    
    // Tu lógica original (¡que era correcta!)
    if (hayIzq && hayDer) {
        error = (int16_t)mediciones.distanciaDer - (int16_t)mediciones.distanciaIzq;
    } else if (hayIzq) {
        error = - (int16_t)mediciones.distanciaIzq;
    } else if (hayDer) {
        error = (int16_t)mediciones.distanciaDer;
    } else {
        error = 0;
    }
    
    uint32_t tiempoActual = millis();
    uint32_t dt = tiempoActual - tiempoAnterior;
    
    if (dt >= 20) { // Usar dt en lugar de un valor estricto
        
        // --- Solución al Derivative Kick y Tiempo Estancado (Stale Time) ---
        if (primerCiclo) {
            errorAnterior = error; // Igualamos para que la derivada sea 0
            dt = 20;               // Forzamos dt normal para no disparar la integral con el tiempo pausado durante el giro
            primerCiclo = false;
        }

        // 1. Término Proporcional
        float P = KP * error;
        // 2. Término Integral (Suma el error en el tiempo)
        // Convertimos dt a segundos (dt / 1000.0)
        errorIntegral += error * (dt / 1000.0);
        // Anti-Windup: Limitamos la memoria integral para que no sature motores al doblar
        errorIntegral = constrain(errorIntegral, -50, 50); 
        float I = KI * errorIntegral;
        // 3. Término Derivativo (Velocidad a la que cambia el error)
        float D = KD * ((error - errorAnterior) / (dt / 1000.0));
        // 4. Suma total
        int16_t correccion = (int16_t)(P + I + D);
        
        // Actualizamos variables estáticas
        errorAnterior = error; 
        tiempoAnterior = tiempoActual;
        correccionAnterior = correccion;
        
        return constrain(correccion, -25, 25);
    } else {
        return correccionAnterior;
    }
}

void resetearErrorAnterior() {
    errorAnterior = 0;
    errorIntegral = 0;  // MUY IMPORTANTE: Limpiar la integral al girar
    primerCiclo = true; // Reiniciar bandera tras un giro para evitar el Kick
}