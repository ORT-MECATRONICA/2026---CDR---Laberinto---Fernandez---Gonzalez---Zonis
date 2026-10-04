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
        estado = AVANZANDO;
      }
      break;
    }

    case AVANZANDO: {
      pulsosActuales = abs(verPulsosEncoderA()); 
      
      if (pulsosActuales < PULSOS_CELDA) {
        movimiento(AVANZAR, {VEL_BASE_IZQ, VEL_BASE_DER});
      } else {
        enviarString(">>> INGRESO A DECISIÓN <<<");
        movimiento(FRENO_F, {0,0});
        estado = DECISION;
      }
      break;
    }

    case DECISION: {
      //ESTO CAMBIA PARA LA PRIORIDAD
      sensadoActual = actualizarSensado();
      if (sensadoActual.distanciaIzq < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaDer < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaCent < UMBRAL_PARED_ESTADO_NORMAL) {
        enviarString(">>> GIRANDO 180 <<<");
        resetearEncoders();
        estado = GIRANDO_180;
      } else if (sensadoActual.distanciaIzq < UMBRAL_PARED_ESTADO_NORMAL&& sensadoActual.distanciaDer > UMBRAL_PARED_ESTADO_NORMAL) {
        resetearEncoders();
        estado = GIRANDO_DER;
      } else if (sensadoActual.distanciaDer < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaIzq > UMBRAL_PARED_ESTADO_NORMAL) {
        enviarString(">>> GIRANDO IZQUIERDA <<<");
        resetearEncoders();
        estado = GIRANDO_IZQ;
      } else {
        enviarString(">>> AVANZANDO <<<");
        resetearEncoders();
        estado = AVANZANDO;
      }
      break;
    }

    case GIRANDO_DER: {
      pulsosActuales = abs(verPulsosEncoderA()); 
      
      if (pulsosActuales < PULSOS_GIRO_90) {
        movimiento(GIRAR_DER, {VEL_BASE_IZQ, VEL_BASE_DER});
      } else {
        enviarString(">>> GIRANDO DER <<<");
        movimiento(FRENO_F, {0,0});
        estado = AVANZANDO;
        resetearEncoders();
      }
      break;
    }

    case GIRANDO_IZQ: {
      pulsosActuales = abs(verPulsosEncoderA()); 
      
      if (pulsosActuales < PULSOS_GIRO_90) {
        movimiento(GIRAR_IZQ, {VEL_BASE_IZQ, VEL_BASE_DER});
      } else {
        enviarString(">>> GIRANDO IZQUIERDA <<<");
        movimiento(FRENO_F, {0,0});
        estado = AVANZANDO;
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
        resetearEncoders();
      }
      break;
    }
  }
}
