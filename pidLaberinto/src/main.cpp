#include "main.h"
#include <Arduino.h>
#include "hardware/logger/logger.h"
#include "hardware/sensoresDistancia/sensoresDistancia.h"
#include "hardware/sensorPiso/sensorPiso.h"
#include "memoria/funcionesMapeo.h"
#include "hardware/movimiento/PID.h"
#include "hardware/movimiento/puenteH.h"


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
  inicializarLogger();
  inicializacionSensoresDist();
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
        } else {
          estadoActual = SWITCHEAR_ESTADO;
        }
        break;
      case AVANZAR:
      sensadoActual = actualizarSensado();
        uint8_t calculoCorreccion = calcularCorreccion(sensadoActual);
        velocidadActual.izquierda = VEL_BASE_IZQ + calculoCorreccion;
        velocidadActual.derecha = VEL_BASE_DER - calculoCorreccion;
        avanzar(velocidadActual);


        //Para frenar cuando tocamos el botón. 
        //NO LE PUSE ANTIREBOTE
        if(lecturaBoton == LOW){
          estadoActual = SWITCHEAR_ESTADO;
        } else {
          estadoActual = AVANZAR;
        }

  }
}
