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
paredes paredActual = {1,1,1};


//==============================================================
//                     VOID SETUP
//==============================================================

void setup (){
  pinMode(BOTON, INPUT);
  inicializarLogger();
  inicializacionSensoresDist(); // Se descomentó para iniciar sensores
  inicializarMotores(); 
  //inicializarEncoders();
  enviarString("INICIO");
}


//==============================================================
//                       VOID LOOP
//==============================================================

void loop(){
  uint8_t lecturaBoton = digitalRead(BOTON);

  switch(estadoActual){
    case SWITCHEAR_ESTADO:
      if(lecturaBoton == LOW){
        estadoActual = AVANZANDO;
        break;
      }
      break;
    
    case AVANZANDO:
      sensadoActual = actualizarSensado();
      paredActual = digitalizarParedes(sensadoActual, paredActual);
      if (paredActual.paredDer == 0){
        estadoActual = GIRANDO_DER;
        break;
      }
      else if (sensadoActual.distanciaCent < UMBRAL_FRENO_F){
        estadoActual = FRENADO_F;
        break;
      } else {
        int16_t calculoCorreccion = calcularCorreccion(sensadoActual);
        // Evitamos que la corrección sea muy grande (limitamos entre -50 y 50)
        calculoCorreccion = constrain(calculoCorreccion, -50, 50);
         
        // Calculamos las velocidades
        int16_t velIzq = VEL_BASE_IZQ - calculoCorreccion; 
        int16_t velDer = VEL_BASE_DER + calculoCorreccion;
        
        // Evitamos que el PWM de las ruedas sea negativo (menor a 0) o mayor a 255
        velocidadActual.izquierda = constrain(velIzq, 0, 255);
        velocidadActual.derecha = constrain(velDer, 0, 255);
        //Serial.println("Velocidad Izquierda: " + String(velocidadActual.izquierda) + " | Velocidad Derecha: " + String(velocidadActual.derecha));
        movimiento(AVANZAR, velocidadActual);
        break;
      }
    
    //CASO GIRAR DERECHA
    case GIRANDO_DER:
      resetearErrorAnterior();
      velocidadActual.izquierda = VEL_GIRO_IZQ;
      velocidadActual.derecha = VEL_GIRO_DER;
      movimiento(GIRAR_DER, velocidadActual);
      delay(DELAY_TIEMPO_GIRO_90);
      
      sensadoActual = actualizarSensado();
      paredActual = digitalizarParedes(sensadoActual, paredActual);
      estadoActual = AVANZANDO;
      break;
      
    //CASO GIRAR IZQUIERDA
    case GIRANDO_IZQ:
      resetearErrorAnterior();
      velocidadActual.izquierda = VEL_GIRO_IZQ;
      velocidadActual.derecha = VEL_GIRO_DER;
      movimiento(GIRAR_IZQ, velocidadActual);
      delay(DELAY_TIEMPO_GIRO_90);
      
      sensadoActual = actualizarSensado();
      paredActual = digitalizarParedes(sensadoActual, paredActual);
      estadoActual = AVANZANDO;
      break;
      
    //CASO FRENADO F
    case FRENADO_F:
      movimiento(FRENO_F, {0,0});
      delay(DELAY_TIEMPO_FRENADO_EN_F);
      sensadoActual = actualizarSensado();
      paredActual = digitalizarParedes(sensadoActual, paredActual);
      // Evaluamos si podemos girar a la izquierda (si hay espacio)
      if (paredActual.paredIzq == 0){
        estadoActual = GIRANDO_IZQ;
      } else {
        estadoActual = ENCIERRO; // Si no hay izquierda, media vuelta
      }
      break;
      
    //CASO ENCIERRO (Media Vuelta)
    case ENCIERRO:
      resetearErrorAnterior();
      sensadoActual = actualizarSensado();
      paredActual = digitalizarParedes(sensadoActual, paredActual);
      // Giramos hasta que el frente quede despejado
      if (paredActual.paredCent == 0){
        estadoActual = AVANZANDO;
        break;
      } else {
        velocidadActual.izquierda = VEL_GIRO_IZQ;
        velocidadActual.derecha = VEL_GIRO_DER;
        movimiento(GIRAR_DER, velocidadActual);
      }
      break;
  }
}
