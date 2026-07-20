#include "main.h"
#include <Arduino.h>
#include "hardware/logger/logger.h"
#include "hardware/sensoresDistancia/sensoresDistancia.h"
#include "hardware/sensorPiso/sensorPiso.h"
#include "memoria/funcionesMapeo.h"
#include "hardware/movimiento/PID.h"
#include "hardware/movimiento/puenteH.h"
#include "hardware/encoders/encoders.h"
#include "config.h"

//==============================================================
//                CREACIÓN DE VARIABLES GLOBALES
//==============================================================

sensado sensadoActual = {0,0,0};
ESTADOS estadoActual = ESPERANDO_INICIO;
VELOCIDAD velocidadActual = {0,0};

//==============================================================
//                     VOID SETUP
//==============================================================

void setup (){
  pinMode(BOTON, INPUT);
  inicializarLogger();
  inicializacionSensoresDist(); 
  inicializarMotores(); 
  inicializarEncoders();
}


//==============================================================
//                       VOID LOOP
//==============================================================

void loop(){
  switch (estadoActual) {
      case ESPERANDO_INICIO:
          if (digitalRead(BOTON) == LOW) {
              // Esperamos a que se suelte el botón y damos 1 segundo para sacar la mano
              while(digitalRead(BOTON) == LOW) { delay(10); }
              delay(1000); 
              estadoActual = SWITCHEAR_ESTADO;
          }
          break;

      case SWITCHEAR_ESTADO:
          sensadoActual = actualizarSensado();
          
          if (sensadoActual.distanciaDer > UMBRAL_PARED_ESTADO_NORMAL) {
              estadoActual = GIRANDO_DER;
              resetearEncoders();
              break;
          } else if (sensadoActual.distanciaCent > UMBRAL_PARED_ESTADO_NORMAL) {
              estadoActual = AVANZANDO;
              resetearEncoders();
              break;
          } else if (sensadoActual.distanciaIzq > UMBRAL_PARED_ESTADO_NORMAL) {
              estadoActual = GIRANDO_IZQ;
              resetearEncoders();
              break;
          } else {
              estadoActual = AVANZANDO; //MOD
              resetearEncoders();
          }
          break;

      case AVANZANDO: {
          sensadoActual = actualizarSensado();
          int16_t calculoCorreccion = calcularCorreccion(sensadoActual);
          calculoCorreccion = constrain(calculoCorreccion, -50, 50);
          
          int16_t velIzq = VEL_BASE_IZQ - calculoCorreccion; 
          int16_t velDer = VEL_BASE_DER + calculoCorreccion; 
          
          velocidadActual.izquierda = constrain(velIzq, 0, 255);
          velocidadActual.derecha = constrain(velDer, 0, 255);
          if(sensadoActual.distanciaCent < 150){
            int16_t derecha = velocidadActual.derecha / 2;
            int16_t izquierda = velocidadActual.izquierda / 2;
            velocidadActual = {izquierda, derecha};
          }
          movimiento(AVANZAR, velocidadActual);
          estadoActual = SWITCHEAR_ESTADO;
          break;
      }

      case GIRANDO_DER:
          if(abs(verPulsosEncoderA()) < 500) {
            estadoActual = AVANZANDO;
            break;
          } else {
            movimiento(GIRAR_DER, {VEL_GIRO_IZQ, VEL_GIRO_DER});
            delay(2500);
            resetearEncoders();
            estadoActual = AVANZANDO_CIEGO;
            resetearErrorAnterior();
            break;
          }

      case GIRANDO_IZQ:
          
          movimiento(GIRAR_IZQ, {VEL_GIRO_IZQ, VEL_GIRO_DER});
          delay(2500);
          estadoActual = AVANZANDO_CIEGO;
          resetearEncoders();
          resetearErrorAnterior();
          break;
          
      case GIRANDO_180:
          movimiento(GIRAR_IZQ, {VEL_GIRO_IZQ, VEL_GIRO_DER});
          if (abs(verPulsosEncoderA()) >= (PULSOS_90_GRADOS * 2) || abs(verPulsosEncoderB()) >= (PULSOS_90_GRADOS * 2)) {
              resetearErrorAnterior();
              estadoActual = FRENADO_F;
          }
          break;

      case FRENADO_F:
          movimiento(FRENO_F, {0,0});
          delay(DELAY_TIEMPO_FRENADO_EN_F);
          estadoActual = SWITCHEAR_ESTADO;
          break;

      case AVANZANDO_CIEGO:
          movimiento(AVANZAR, {150,150});
         if (abs(verPulsosEncoderA()) >= PULSOS_AVANZAR_BLOQUEANTE) {
              resetearErrorAnterior();
              estadoActual = AVANZANDO;
          } else {
            estadoActual = AVANZANDO_CIEGO;
          }
          break;
          
      case ACCION:
          estadoActual = SWITCHEAR_ESTADO;
          break;
  }
}
