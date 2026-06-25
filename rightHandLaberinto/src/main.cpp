#include "main.h"
#include <Arduino.h>
#include "hardware/logger/logger.h"
#include "hardware/sensoresDistancia/sensoresDistancia.h"
#include "hardware/sensorPiso/sensorPiso.h"
#include "memoria/funcionesMapeo.h"

#include "hardware/movimiento/PID.h"
#include "hardware/movimiento/puenteH.h"

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
//                       VOID LOOP
//==============================================================

void loop(){
  uint8_t lecturaBoton = digitalRead(BOTON);
  switch(estadoActual){
     case SWITCHEAR_ESTADO:
        movimiento(FRENO_F,{0,0}); //frena los motores, PWM a 0.
        if(lecturaBoton == LOW){
          estadoActual = ACCION;
          while(digitalRead(BOTON) == LOW) { delay(10); } // Espera a que se suelte el botón (antirrebote)
        } else {
          estadoActual = SWITCHEAR_ESTADO;
        }
        break;
        
      case ACCION: //ya no
        sensadoActual = actualizarSensado();
        
        if (sensadoActual.distanciaCent < 100){
          movimiento(GIRAR_DER, {50,50});
        } else {
          if (sensadoActual.distanciaDer < 69 || sensadoActual.distanciaDer > 71){
            int16_t error = sensadoActual.distanciaDer - 70;
            uint16_t correccion = calcularCorreccionRightHand(error);
          }cs  
        }


        //Para frenar cuando tocamos el botón. 
        if(lecturaBoton == LOW){
          movimiento(FRENO_F,{0,0}); // Frenamos antes de trabarnos en el while
          estadoActual = SWITCHEAR_ESTADO;
          while(digitalRead(BOTON) == LOW) { delay(10); } // Espera a que se suelte el botón (antirrebote)
        } else {
          estadoActual = ACCION;
        }
        break;
  }
}
