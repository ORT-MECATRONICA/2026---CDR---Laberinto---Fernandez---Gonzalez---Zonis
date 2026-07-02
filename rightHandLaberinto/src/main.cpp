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
  pinMode(BOTON, INPUT);
  inicializarLogger();
  inicializacionSensoresDist();
  inicializarMotores(); 
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
          estadoActual = AVANZANDO;
          Serial.println("SWITCHEAR_ESTADO -> AVANZANDO");
          while(digitalRead(BOTON) == LOW) { delay(10); } // Espera a que se suelte el botón (antirrebote)
        }
        break;
        
      case AVANZANDO: 
      Serial.println("AVANZANDO");
        sensadoActual = actualizarSensado();
        
        // Para frenar cuando tocamos el botón (Prioridad absoluta)
        if(lecturaBoton == LOW){
          movimiento(FRENO_F,{0,0});  
          estadoActual = SWITCHEAR_ESTADO;
          while(digitalRead(BOTON) == LOW) { delay(10); } 
          break; 
        }

        //======== NAVEGACIÓN - RIGHT HAND =========

        if (sensadoActual.distanciaDer < 150){
          estadoActual = GIRANDO_DER;
        } else if (sensadoActual.distanciaIzq < 150){ 
          estadoActual = GIRANDO_IZQ;
        } else {
          movimiento(AVANZAR, {VEL_BASE_DER, VEL_BASE_IZQ});
        }
        break;


      case GIRANDO_DER:
        movimiento(GIRAR_DER,{VEL_GIRO_DER,VEL_GIRO_IZQ});
        sensadoActual = actualizarSensado();
        if(sensadoActual.distanciaDer < 70){
          estadoActual = AVANZANDO;
        }
        if(lecturaBoton == LOW){
          movimiento(FRENO_F,{0,0}); 
          estadoActual = SWITCHEAR_ESTADO;
          while(digitalRead(BOTON) == LOW) { delay(10); }
        }
        break;

      case GIRANDO_IZQ:
        movimiento(GIRAR_IZQ,{VEL_GIRO_DER,VEL_GIRO_IZQ});
        sensadoActual = actualizarSensado();
        // REVISAR ESTO
        if(sensadoActual.distanciaCent > 120){
          estadoActual = AVANZANDO;
        }
        // Agregamos chequeo de botón por seguridad durante el giro
        if(lecturaBoton == LOW){
          movimiento(FRENO_F,{0,0}); 
          estadoActual = SWITCHEAR_ESTADO;
          while(digitalRead(BOTON) == LOW) { delay(10); }
        }
        break;
      
      case FRENADO_F:
        movimiento(FRENO_F,{0,0});
        //coherencia
        delay(DELAY_TIEMPO_FRENADO_EN_F); //MUCHO CUIDADO CON ESTO !!! POR FAVOR
        estadoActual = SWITCHEAR_ESTADO;
        break;
  }
}
