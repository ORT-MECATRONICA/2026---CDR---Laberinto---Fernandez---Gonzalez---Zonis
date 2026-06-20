#include "maquinaDeEstados.h"
#include <Arduino.h>
#include "hardware/movimiento/pid.h"
#include "hardware/movimiento/puenteH.h"
#include "config.h"
#include "hardware/sensoresDistancia/sensoresDistancia.h"

void calibrar(){

}

void primerRecorrido(){
    static VELOCIDAD velocidadActual = {0,0};
    switch(subestadoActualPrimerRecorrido){
        case PARTIDA: {
            subestadoActualPrimerRecorrido = ENTRE_CELDAS;
        };

        case ENTRE_CELDAS: {
            sensadoActual = actualizarSensado();
            //Serial.println(" Distancia Derecha: " + String(sensadoActual.distanciaDer) + " | Distancia Izquierda: " + String(sensadoActual.distanciaIzq));
            int16_t calculoCorreccion = calcularCorreccion(sensadoActual);
            calculoCorreccion = constrain(calculoCorreccion, -50, 50);
            int16_t velIzq = VEL_BASE_IZQ + calculoCorreccion;
            int16_t velDer = VEL_BASE_DER - calculoCorreccion;
            velocidadActual.izquierda = constrain(velIzq, 0, 255);
            velocidadActual.derecha = constrain(velDer, 0, 255);
            
            avanzar(velocidadActual);
        };

        case MAPEO: {
            //Acciones de mapeo
            subestadoActualPrimerRecorrido = LLEGADA;
        };

        case LLEGADA: {
            //Acciones de llegada
            salirDelLoop = true;
        };
    }
}

void segundoRecorrido(){
    bool salirDelLoop = false; //Acción bloqueante
    while (salirDelLoop == false){
        
    }
}