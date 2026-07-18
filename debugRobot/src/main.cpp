#include "main.h"
#include <Arduino.h>
#include "hardware/logger/logger.h"
#include "hardware/sensoresDistancia/sensoresDistancia.h"
#include "hardware/sensorPiso/sensorPiso.h"
#include "memoria/funcionesMapeo.h"
#include "hardware/movimiento/PID.h"
#include "hardware/movimiento/puenteH.h"
#include "hardware/encoders/encoders.h"

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
  inicializacionSensoresDist(); // Se descomentó para iniciar sensores
  inicializarMotores(); 
  inicializarEncoders();
  enviarString("INICIO");
}


//==============================================================
//                       VOID LOOP
//==============================================================

void loop(){
  uint8_t lecturaBoton = digitalRead(BOTON);

  switch(estadoActual){
    case SWITCHEAR_ESTADO:
      movimiento(FRENO_F, {0,0});
      if(lecturaBoton == LOW){
        estadoActual = AVANZANDO;
        while(digitalRead(BOTON) == LOW) { delay(10); } // Antirrebote
      }
      break;

    case AVANZANDO:
      sensadoActual = actualizarSensado();

      if(lecturaBoton == LOW){
        estadoActual = SWITCHEAR_ESTADO;
        while(digitalRead(BOTON) == LOW) { delay(10); }
        break;
      }

      // Regla de la mano derecha muy básica (sin encoders)
      if (sensadoActual.distanciaDer > UMBRAL_PARED_ESTADO_NORMAL) {
        // No hay pared a la derecha, giramos a la derecha
        estadoActual = GIRANDO_DER;
      } else if (sensadoActual.distanciaCent < UMBRAL_PARED_FRENTE) {
        // Hay pared al frente, giramos a la izquierda
        estadoActual = GIRANDO_IZQ;
      } else {
        // Hay pared a la derecha pero está libre adelante, avanzamos
        movimiento(AVANZAR, {VEL_BASE_DER, VEL_BASE_IZQ});
      }
      break;

    case GIRANDO_DER:
      movimiento(GIRAR_DER, {VEL_GIRO_DER, VEL_GIRO_IZQ});
      // Como no usamos encoders, giramos "a ciegas" usando tiempo (aprox 90 grados)
      delay(400); // ADVERTENCIA: Ajustar según carga de batería y superficie
      
      // Avanzamos un poco para acomodarnos en la nueva celda y encontrar pared nuevamente
      movimiento(AVANZAR, {VEL_BASE_DER, VEL_BASE_IZQ});
      delay(400); // ADVERTENCIA: Ajustar
      
      estadoActual = AVANZANDO;
      
      // Freno de seguridad
      if(lecturaBoton == LOW){
        estadoActual = SWITCHEAR_ESTADO;
        while(digitalRead(BOTON) == LOW) { delay(10); }
      }
      break;

    case GIRANDO_IZQ:
      movimiento(GIRAR_IZQ, {VEL_GIRO_DER, VEL_GIRO_IZQ});
      sensadoActual = actualizarSensado();
      
      // Giramos a la izquierda hasta que el frente se despeje
      if(sensadoActual.distanciaCent > UMBRAL_PARED_FRENTE + 50){ // +50 como histéresis
        estadoActual = AVANZANDO;
      }

      // Freno de seguridad
      if(lecturaBoton == LOW){
        estadoActual = SWITCHEAR_ESTADO;
        while(digitalRead(BOTON) == LOW) { delay(10); }
      }
      break;
      
    case FRENADO_F:
      movimiento(FRENO_F, {0,0});
      delay(DELAY_TIEMPO_FRENADO_EN_F);
      estadoActual = SWITCHEAR_ESTADO;
      break;
      
    default:
      estadoActual = SWITCHEAR_ESTADO;
      break;
  }
}
