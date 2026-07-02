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
          break; // Salimos del case para no ejecutar el resto
        }

        // Lógica de navegación (Right Hand Rule)
        if (sensadoActual.distanciaDer < 150){
          // Pared enfrente -> Girar a la izquierda
          estadoActual = GIRANDO_DER;
        } else if (sensadoActual.distanciaIzq > 150){ 
          // Hueco a la derecha -> Girar a la derecha
          estadoActual = GIRANDO_IZQ;
        } else {
          // No hay pared enfrente y hay pared a la derecha -> Avanzar con PID
          if(sensadoActual.distanciaDer < 69 || sensadoActual.distanciaDer > 71){
            int16_t error = sensadoActual.distanciaDer - 70;
            uint16_t correccion = calcularCorreccionRightHand(error);
            velocidadActual.izquierda = VEL_BASE_IZQ + correccion; //checkear el sentido +/-
            velocidadActual.derecha = VEL_BASE_DER - correccion; // lo mismo
            movimiento(AVANZAR, velocidadActual);
          } else {
            // Centrado perfecto (añadido para que no se quede trabado con velocidad anterior extraña)
            velocidadActual.izquierda = VEL_BASE_IZQ;
            velocidadActual.derecha = VEL_BASE_DER;
            movimiento(AVANZAR, velocidadActual);
          }
        }
        break;


      case GIRANDO_DER:
        movimiento(GIRAR_DER,{VEL_GIRO_DER,VEL_GIRO_IZQ});
        sensadoActual = actualizarSensado();
        // Cuando vuelve a ver una pared a la derecha a distancia normal, termina el giro
        if(sensadoActual.distanciaDer < 70){
          estadoActual = AVANZANDO;
        }
        // Agregamos chequeo de botón por seguridad durante el giro
        if(lecturaBoton == LOW){
          movimiento(FRENO_F,{0,0}); 
          estadoActual = SWITCHEAR_ESTADO;
          while(digitalRead(BOTON) == LOW) { delay(10); }
        }
        break;

      case GIRANDO_IZQ:
        movimiento(GIRAR_IZQ,{VEL_GIRO_DER,VEL_GIRO_IZQ});
        sensadoActual = actualizarSensado();
        // Dejamos de girar cuando el camino enfrente se despeje
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
        delay(750); //MUCHO CUIDADO CON ESTO !!! POR FAVOR
        estadoActual = SWITCHEAR_ESTADO;
        break;
  }
}
