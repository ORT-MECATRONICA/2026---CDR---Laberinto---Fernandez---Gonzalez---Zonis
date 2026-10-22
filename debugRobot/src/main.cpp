//==============================================================
//                          PRIMAVERA
// VERSIÓN DEL CÓDIGO CON CALIBRACIÓN DE PULSOS, VELOCIDADES Y KP
// SE LLAMA BACHIN EN HOMENAJE A "PRIMAVERA PORTEÑA", DE PIAZOLLA.
//==============================================================

#include "main.h"
#include <Arduino.h>
#include <Preferences.h>
#include "hardware/logger/logger.h"
#include "hardware/sensoresDistancia/sensoresDistancia.h"
#include "hardware/movimiento/PID.h"
#include "hardware/movimiento/puenteH.h"
#include "hardware/encoders/encoders.h"
#include "hardware/calibracion/calibracion.h"
#include "config.h"

//==============================================================
//                CREACIÓN DE VARIABLES GLOBALES
//==============================================================

sensado sensadoActual = {50, 200, 50};
VELOCIDAD velocidadActual = {0, 0};
MAQUINA_NUEVA estado = LISTO;

// [NO UTILIZADA ACTUALMENTE - MANTENIDA POR REQUISITO]
MAQUINA_NUEVA movimientoAnterior = AVANZANDO;

uint32_t pulsosActuales = 0;
// uint32_t tiempoInicioGiro = 0; // [NO UTILIZADA ACTUALMENTE - MANTENIDA POR REQUISITO]

// Modo de navegación: Right Hand (por defecto) o Left Hand
MODO_SEGUIDOR modoActual = MODO_RIGHT_HAND;

//==============================================================
//                     VOID SETUP
//==============================================================

void setup (){
  pinMode(BOTON1, INPUT);
  pinMode(BOTON2, INPUT);
  
  inicializarCalibracion(); // Carga parámetros desde Flash (Preferences) o defaults
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
      
      #ifdef USAR_BLUETOOTH
      if (hayDatosBT()) {
        estado = INFORMACION_RECIBIDA;
        break;
      }
      #endif
      
      // Presionar Botón 1 para avisar que se terminó de cargar datos
      if (digitalRead(BOTON1) == LOW) {
        while(digitalRead(BOTON1) == LOW) { delay(10); } // Antirrebote al soltar
        enviarString(">>> DATOS CARGADOS. SELECCIONE: B1 (RIGHT HAND) o B2 (LEFT HAND) <<<");
        estado = ESPERANDO_LARGADA;
      }
      break;
    }

    case INFORMACION_RECIBIDA: {
      #ifdef USAR_BLUETOOTH
      procesarTramaBT();
      #endif
      estado = LISTO;
      break;
    }

    case ESPERANDO_LARGADA: {
      movimiento(FRENO_F, {0,0});
      // En este estado el robot ya NO revisa SerialBT.available

      // Botón 1: Inicia Right Hand
      if (digitalRead(BOTON1) == LOW) {
        while(digitalRead(BOTON1) == LOW) { delay(10); } // Antirrebote al soltar
        modoActual = MODO_RIGHT_HAND;
        enviarString(">>> LARGADA: MODO RIGHT HAND <<<");
        delay(1000); // 1 segundo de margen para sacar la mano
        resetearEncoders();
        estado = AVANZANDO;
        break;
      }

      // Botón 2: Inicia Left Hand
      if (digitalRead(BOTON2) == LOW) {
        while(digitalRead(BOTON2) == LOW) { delay(10); } // Antirrebote al soltar
        modoActual = MODO_LEFT_HAND;
        enviarString(">>> LARGADA: MODO LEFT HAND <<<");
        delay(1000); // 1 segundo de margen para sacar la mano
        resetearEncoders();
        estado = AVANZANDO;
        break;
      }
      break;
    }

    case AVANZANDO: {
      // Parar el robot presionando el botón 1 (ÚNICAMENTE CUANDO ESTÁ EN EL ESTADO AVANZANDO)
      if (digitalRead(BOTON1) == LOW) {
        while(digitalRead(BOTON1) == LOW) { delay(10); } // Antirrebote al soltar
        movimiento(FRENO_F, {0, 0});
        enviarString(">>> PARADA POR BOTON 1. VOLVIENDO A LISTO <<<");
        estado = LISTO;
        break;
      }

      pulsosActuales = (abs(verPulsosEncoderA()) + abs(verPulsosEncoderB())) / 2;
      sensadoActual = actualizarSensado();
            
      int16_t correccion = calcularCorreccion(sensadoActual);
      velocidadActual.izquierda = constrain(configRobot.velBaseIzq + correccion, 0, 255);
      velocidadActual.derecha = constrain(configRobot.velBaseDer - correccion, 0, 255);
      movimiento(AVANZAR, {velocidadActual.izquierda, velocidadActual.derecha});
     
      bool condicionGiroDer = false;
      bool condicionAvanzar = false;
      bool condicionGiroIzq = false;
      bool condicionGiro180 = false;

      if (modoActual == MODO_RIGHT_HAND) {
        // ORDEN DE PRIORIDADES RIGHT HAND: 1° Derecha, 2° Frente, 3° Izquierda, 4° 180
        condicionGiroDer = sensadoActual.distanciaDer >= UMBRAL_PARED_ESTADO_NORMAL;
        condicionAvanzar = sensadoActual.distanciaDer < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaCent >= UMBRAL_FRENTE;
        condicionGiroIzq = sensadoActual.distanciaDer < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaCent < UMBRAL_FRENTE && sensadoActual.distanciaIzq >= UMBRAL_PARED_ESTADO_NORMAL;
        condicionGiro180 = sensadoActual.distanciaDer < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaCent < UMBRAL_FRENTE && sensadoActual.distanciaIzq < UMBRAL_PARED_ESTADO_NORMAL;
      } else {
        // ORDEN DE PRIORIDADES LEFT HAND: 1° Izquierda, 2° Frente, 3° Derecha, 4° 180
        condicionGiroIzq = sensadoActual.distanciaIzq >= UMBRAL_PARED_ESTADO_NORMAL;
        condicionAvanzar = sensadoActual.distanciaIzq < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaCent >= UMBRAL_FRENTE;
        condicionGiroDer = sensadoActual.distanciaIzq < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaCent < UMBRAL_FRENTE && sensadoActual.distanciaDer >= UMBRAL_PARED_ESTADO_NORMAL;
        condicionGiro180 = sensadoActual.distanciaIzq < UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaCent < UMBRAL_FRENTE && sensadoActual.distanciaDer < UMBRAL_PARED_ESTADO_NORMAL;
      }

      if (condicionGiroDer) {
        enviarString(">>> GIRANDO DERECHA <<<");
        resetearEncoders();
        estado = PREGIRO_DER;
      } else if (condicionAvanzar) {
        // Continúa avanzando
        estado = AVANZANDO;
      } else if (condicionGiroIzq) {
        enviarString(">>> GIRANDO IZQUIERDA <<<");
        resetearEncoders();
        estado = PREGIRO_IZQ;
      } else if (condicionGiro180) {
        enviarString(">>> GIRANDO 180 <<<");
        resetearEncoders();
        estado = GIRANDO_180;
      } else {
        enviarString(">>> ERROR DE SENSADO <<<");
        movimiento(FRENO_F, {0,0});
      }
      break;
    }

    case PREGIRO_DER: {
      pulsosActuales = (abs(verPulsosEncoderB()) + abs(verPulsosEncoderA())) / 2;
      sensadoActual = actualizarSensado();
      if (pulsosActuales < configRobot.pulsosPregiroDer && sensadoActual.distanciaCent >= 50) {
        movimiento(AVANZAR, {configRobot.velBaseIzq, configRobot.velBaseDer});
      } else {
        enviarString(">>> PREGIRO DERECHA <<<");
        movimiento(FRENO_F, {0,0});
        estado = GIRANDO_DER;
        resetearErrorAnterior();
        resetearEncoders();
      }
      break;
    }

    case PREGIRO_IZQ: {
      pulsosActuales = (abs(verPulsosEncoderB()) + abs(verPulsosEncoderA())) / 2;
      sensadoActual = actualizarSensado();
      if (pulsosActuales < configRobot.pulsosPregiroIzq && sensadoActual.distanciaCent >= 50) {
        movimiento(AVANZAR, {configRobot.velBaseIzq, configRobot.velBaseDer});
      } else {
        enviarString(">>> PREGIRO IZQUIERDA <<<");
        movimiento(FRENO_F, {0,0});
        estado = GIRANDO_IZQ;
        resetearErrorAnterior();
        resetearEncoders();
      }
      break;
    }

    case GIRANDO_DER: {
      pulsosActuales = (abs(verPulsosEncoderB()) + abs(verPulsosEncoderA())) / 2;
      bool cortePorPulsosDer = pulsosActuales < configRobot.pulsosGiro90Der;
      if (cortePorPulsosDer) {
        movimiento(GIRAR_DER, {configRobot.velBaseIzqGiro, configRobot.velBaseDerGiro});
      } else {
        enviarString(">>> GIRANDO DER <<<");
        movimiento(FRENO_F, {0,0});
        movimientoAnterior = GIRANDO_DER;
        estado = POSTGIRO;
        resetearErrorAnterior();
        resetearEncoders();
      }
      break;
    }

    case GIRANDO_IZQ: {
      pulsosActuales = (abs(verPulsosEncoderB()) + abs(verPulsosEncoderA())) / 2;
      bool cortePorPulsosIzq = pulsosActuales < configRobot.pulsosGiro90Izq;
      if (cortePorPulsosIzq) {
        movimiento(GIRAR_IZQ, {configRobot.velBaseIzqGiro, configRobot.velBaseDerGiro});
      } else {
        enviarString(">>> GIRANDO IZQUIERDA <<<");
        movimiento(FRENO_F, {0,0});
        movimientoAnterior = GIRANDO_IZQ;
        estado = POSTGIRO;
        resetearErrorAnterior();
        resetearEncoders();
      }
      break;
    }

    case POSTGIRO: {
      pulsosActuales = (abs(verPulsosEncoderB()) + abs(verPulsosEncoderA())) / 2; 
      sensadoActual = actualizarSensado();
      if (pulsosActuales < configRobot.pulsosPostgiro) {
        movimiento(AVANZAR, {configRobot.velBaseIzq, configRobot.velBaseDer});
      } else {
        enviarString(">>> FIN POSTGIRO <<<");
        estado = AVANZANDO;
        resetearErrorAnterior();
        resetearEncoders();
      }
      break;
    }

    case GIRANDO_180: {
      pulsosActuales = (abs(verPulsosEncoderB()) + abs(verPulsosEncoderA())) / 2;
      if (pulsosActuales < configRobot.pulsosGiro180Izq) {
        movimiento(GIRAR_IZQ, {configRobot.velBaseIzqGiro180, configRobot.velBaseDerGiro180});
      } else {
        enviarString(">>> FIN GIRO 180 IZQ <<<");
        movimiento(FRENO_F, {0,0});
        movimientoAnterior = GIRANDO_IZQ;
        estado = POSTGIRO;
        resetearErrorAnterior();
        resetearEncoders();
      }
      break;
    }
  }
}