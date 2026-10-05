#include "main.h"
#include <Arduino.h>
#include "hardware/logger/logger.h"
#include "hardware/sensoresDistancia/sensoresDistancia.h"
#include "hardware/movimiento/PID.h"
#include "hardware/movimiento/puenteH.h"
#include "hardware/encoders/encoders.h"
#include "config.h"

//==============================================================
//                CREACIÓN DE VARIABLES GLOBALES
//==============================================================

sensado sensadoActual = {0,0,0};
VELOCIDAD velocidadActual = {0,0};
MAQUINA_ESTADOS estado = LISTO;
uint32_t pulsosActuales = 0;

//==============================================================
//                     VOID SETUP
//==============================================================

void setup (){
  pinMode(BOTON1, INPUT);
  inicializarLogger();
  inicializarMotores(); 
  inicializarEncoders();
  inicializacionSensoresDist();
  resetearEncoders();
}

//==============================================================
//                       VOID LOOP
//==============================================================

void loop(){
  
  switch (estado) {
    case LISTO: {
      movimiento(FRENO_F, {0,0});
      
      if (digitalRead(BOTON1) == LOW) {
        //VER SI EL ANTIRREBOTE ES ÓPTIMO
        while(digitalRead(BOTON1) == LOW) { delay(10); } // Esperar a que se suelte el botón
        enviarString(">>> INICIANDO AVANCE <<<");
        resetearEncoders();
        resetearErrorAnterior();
        estado = AVANZANDO;
      }
      break;
    }

    case AVANZANDO: {
      pulsosActuales = (abs(verPulsosEncoderA()) + abs(verPulsosEncoderB())) / 2;
      
      if (pulsosActuales < PULSOS_CELDA) {
       /* sensadoActual = actualizarSensado();
        int16_t correccion = calcularCorreccion(sensadoActual);
        velocidadActual.izquierda = constrain(VEL_BASE_IZQ + correccion, 0, 255);
        velocidadActual.derecha = constrain(VEL_BASE_DER - correccion, 0, 255);
        movimiento(AVANZAR, {velocidadActual.izquierda, velocidadActual.derecha});
        */
       movimiento(AVANZAR, {VEL_BASE_IZQ, VEL_BASE_DER});
      } else {
        enviarString(">>> INGRESO A DECISIÓN <<<");
        movimiento(FRENO_F, {0,0});
        estado = DECISION;
      }
      break;
    }

    case DECISION: {
      sensadoActual = actualizarSensado();
      
      //ORDEN Y PROGRESO!!! 
      bool condicionGiroDer = sensadoActual.distanciaDer > UMBRAL_PARED_ESTADO_NORMAL; // TODO LO DEMÁS NO ME IMPORTA, HAGAN LO QUE HAGAN LOS OTROS SENSORES, GIRO!
      bool condicionAvanzar = sensadoActual.distanciaDer < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaCent > UMBRAL_PARED_ESTADO_NORMAL;
      bool condicionGiroIzq = sensadoActual.distanciaDer < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaCent < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaIzq > UMBRAL_PARED_ESTADO_NORMAL;
      bool condicionGiro180 = sensadoActual.distanciaDer < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaCent < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaIzq < UMBRAL_PARED_ESTADO_NORMAL;

      if (condicionGiroDer) {
        enviarString(">>> GIRANDO DERECHA <<<");
        resetearEncoders();
        estado = GIRANDO_DER;
      } else if (condicionAvanzar) {
        resetearEncoders();
        resetearErrorAnterior();
        estado = AVANZANDO;
        enviarString (">>> AVANZANDO <<<");
      } else if (condicionGiroIzq) {
        enviarString(">>> GIRANDO IZQUIERDA <<<");
        resetearEncoders();
        estado = GIRANDO_IZQ;
      } else if (condicionGiro180) {
        enviarString(">>> GIRANDO 180 <<<");
        resetearEncoders();
        estado = GIRANDO_180;
      }
      break;
    }

    case GIRANDO_DER: {
      pulsosActuales = (abs(verPulsosEncoderA())) / 2;
      
      if (pulsosActuales < PULSOS_GIRO_90_DER) {
        movimiento(GIRAR_DER, {VEL_BASE_IZQ, VEL_BASE_DER});
      } else {
        enviarString(">>> GIRANDO DER <<<");
        movimiento(FRENO_F, {0,0});
        estado = AVANZANDO;
        resetearErrorAnterior();
        resetearEncoders();
      }
      break;
    }

    case GIRANDO_IZQ: {
      pulsosActuales = abs(verPulsosEncoderB()); 
      
      if (pulsosActuales < PULSOS_GIRO_90_IZQ) {
        movimiento(GIRAR_IZQ, {VEL_BASE_IZQ, VEL_BASE_DER});
      } else {
        enviarString(">>> GIRANDO IZQUIERDA <<<");
        movimiento(FRENO_F, {0,0});
        estado = AVANZANDO;
        resetearErrorAnterior();
        resetearEncoders();
      }
      break;
    }

    case GIRANDO_180: {
      pulsosActuales = abs(verPulsosEncoderA()); 
      
      if (pulsosActuales < PULSOS_GIRO_180) {
        movimiento(GIRAR_DER, {VEL_BASE_IZQ, VEL_BASE_DER});
      } else {
        enviarString(">>> GIRANDO 180 <<<");
        movimiento(FRENO_F, {0,0});
        estado = AVANZANDO;
        resetearErrorAnterior();
        resetearEncoders();
      }
      break;
    }
  }
}
