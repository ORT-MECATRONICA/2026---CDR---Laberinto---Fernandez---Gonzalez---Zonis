#include "maquinaDeEstados.h"
#include "main.h"
#include "hardware/movimiento/pid.h"

void calibrar(){
   SUBMAQUINA_CALIBRAR subestadoActual = SENSOR_IZQ;
    
}

void primerRecorrido(){
    switch(subestadoActualPrimerRecorrido){
        case PARTIDA: {
            subestadoActualPrimerRecorrido = ENTRE_CELDAS;
        };

        case ENTRE_CELDAS: {
            //Acciones de entre celdas
            sensadoActual = actualizarSensado(); //Paso 1 - actualizar sensado
            VELOCIDAD velocidadActual = calcularDiferencialPID(); //Paso 2 - calcular PID
            
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