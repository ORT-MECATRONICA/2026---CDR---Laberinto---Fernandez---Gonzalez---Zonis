#include "main.h"
#include <Arduino.h>
#include "hardware/logger/logger.h"
#include "hardware/sensoresDistancia/sensoresDistancia.h"
#include "hardware/sensorPiso/sensorPiso.h"
#include "memoria/funcionesMapeo.h"
#include "hardware/movimiento/PID.h"
#include "maquinaDeEstados/maquinaDeEstados.h"

//==============================================================
//                CREACIÓN DE VARIABLES GLOBALES
//==============================================================

sensado sensadoActual = {0,0,0};
POSICION posicionActual = {X_START,Y_START, NORTE};
ESTADOS estadoActual = ESPERA;
uint32_t tiempoAnterior = 0;
uint8_t contador = 0;
bool mapaEnMemoria = false;
bool salirPrimerRecorrido = false;
bool salirSegundoRecorrido= false;
//==============================================================
//                     VOID SETUP
//==============================================================

void setup (){
  inicializarLogger();
  inicializacionSensoresDist();
}

//==============================================================
//    ANULAMOS VOID LOOP DE ARDUINO PARA NO CONSUMIR RECURSOS
//==============================================================

void loop(){
  switch(estadoActual){
      uint8_t botonPressed = digitalRead(BOTON);  
      case ESPERA: {
        //POR AHORA SIMPLEMENTE ESPERA, NO HACE NADA
        if(botonPressed == LOW){
          estadoActual = SWITCHEAR_ESTADO;
          tiempoAnterior = millis();
        }
      };

      case SWITCHEAR_ESTADO: {
        if(botonPressed == LOW){
          contador = contador + 1;
        }
        if((millis() - tiempoAnterior) == 5000){
          if(contador == 1){
            estadoActual = CALIBRAR;
          } else if (contador > 3 && mapaEnMemoria == true ){
            estadoActual = SEGUNDO_RECORRIDO;
          } else if (contador == 0){
            estadoActual = ESPERA;
          } else {
            estadoActual = PRIMER_RECORRIDO;
          }
        }
      };

      case CALIBRAR: {
        calibrar();
        estadoActual = ESPERA;
      };
      case PRIMER_RECORRIDO: {
        primerRecorrido();
        if(salirPrimerRecorrido == true){
          estadoActual = ESPERA;
        } else {
          estadoActual = PRIMER_RECORRIDO;
        }
      };
      case SEGUNDO_RECORRIDO: {
        segundoRecorrido();
        if(salirSegundoRecorrido == true){
          estadoActual = ESPERA;
        } else {
          estadoActual = SEGUNDO_RECORRIDO;
        }
      };
  }
}

//==============================================================
//  TAREA 1: Principal, es un loop infinito con un secuencial
/*      SECUENCIA:
         * "Lee" los sensores de distancia y piso
         *  A partir de la lectura de "sensado sensadoAct" calcula el dif de PID
         *  Lee la queue de movimientos
         *  Realiza los cambios en el puente H (hardware/movimiento)
*/
//==============================================================

