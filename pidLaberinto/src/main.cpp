#include "main.h"
#include <Arduino.h>
#include "hardware/logger/logger.h"
#include "hardware/sensoresDistancia/sensoresDistancia.h"
#include "hardware/sensorPiso/sensorPiso.h"
#include "memoria/funcionesMapeo.h"

#include "hardware/movimiento/puenteH.h"

uint8_t errorAnterior = 0;
//==============================================================
//                CREACIÓN DE VARIABLES GLOBALES
//==============================================================

sensado sensadoActual = {0,0,0};
ESTADOS estadoActual = SWITCHEAR_ESTADO;
VELOCIDAD velocidadActual = {0,0};

//==============================================================
//                     VOID SETUP
//==============================================================

void setup (){
  pinMode(BOTON, INPUT); // Es necesario inicializar el pin
  inicializarLogger();
  inicializacionSensoresDist();
  inicializarMotores(); // Faltaba inicializar los motores (PWM/LEDC)
}


//==============================================================
//    ANULAMOS VOID LOOP DE ARDUINO PARA NO CONSUMIR RECURSOS
//==============================================================

void loop(){
  uint8_t lecturaBoton = digitalRead(BOTON);
  switch(estadoActual){
     case SWITCHEAR_ESTADO:
        avanzar({0,0}); //frena los motores, PWM a 0.
        if(lecturaBoton == LOW){
          estadoActual = AVANZAR;
          while(digitalRead(BOTON) == LOW) { delay(10); } // Espera a que se suelte el botón (antirrebote)
        } else {
          estadoActual = SWITCHEAR_ESTADO;
        }
        break;
        
      case AVANZAR:
        sensadoActual = actualizarSensado();
        uint8_t error = sensadoActual.distanciaIzq - sensadoActual.distanciaDer;
    uint8_t correccion = (KP * error) + (KD * (error - errorAnterior)) ;
    errorAnterior = error; //CHECKEAR SI O SI!!
     uint8_t calculoCorreccion = correccion;
        velocidadActual.izquierda = VEL_BASE_IZQ + calculoCorreccion;
        velocidadActual.derecha = VEL_BASE_DER - calculoCorreccion;
        Serial.println("hlla");
        avanzar(velocidadActual);


        //Para frenar cuando tocamos el botón. 
        if(lecturaBoton == LOW){
          avanzar({0,0}); // Frenamos antes de trabarnos en el while
          estadoActual = SWITCHEAR_ESTADO;
          while(digitalRead(BOTON) == LOW) { delay(10); } // Espera a que se suelte el botón (antirrebote)
        } else {
          estadoActual = AVANZAR;
        }
        break;
  }
}
