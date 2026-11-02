//==============================================================
//                          PRIMAVERA
// VERSIÓN DEL CÓDIGO CON CALIBRACIÓN DE PULSOS, VELOCIDADES Y KP
// [VELOCIDAD] Y CONTROL DE VELOCIDAD CON LOS ENCODERS (ver puenteH.cpp)
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
MAQUINA_NUEVA decisionAPriori = AVANZANDO;
MAQUINA_NUEVA decisionAnterior = AVANZANDO;
MAQUINA_NUEVA movimientoAnterior = AVANZANDO;
uint32_t pulsosActuales = 0;
uint32_t pulsosAlDetectar = 0; // [PREGIRO] Pulsos en la primera lectura que pidió el giro que se está confirmando
MODO_SEGUIDOR modoActual = MODO_RIGHT_HAND;
uint8_t contadorDeFiltrado = 0;
String textoLecturas(sensado s) {
  return " I:" + String(s.distanciaIzq) + " C:" + String(s.distanciaCent) + " D:" + String(s.distanciaDer);
}



//==============================================================
//                     VOID SETUP
//==============================================================

void setup (){
  pinMode(BOTON1, INPUT);
  pinMode(BOTON2, INPUT);
  
  inicializarCalibracion();
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
        calibrarOffsetsLaterales(); // [OFFSET DINÁMICO] El robot tiene que estar centrado entre dos paredes
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
      // Botón 1: Inicia Right Hand
      if (digitalRead(BOTON1) == LOW) {
        while(digitalRead(BOTON1) == LOW) { delay(10); }
        modoActual = MODO_RIGHT_HAND;
        enviarString(">>> LARGADA: MODO RIGHT HAND <<<");
        delay(1000); 
        resetearEncoders();
        estado = AVANZANDO;
        break;
      }

      // Botón 2: Inicia Left Hand
      if (digitalRead(BOTON2) == LOW) {
        while(digitalRead(BOTON2) == LOW) { delay(10); } 
        modoActual = MODO_LEFT_HAND;
        enviarString(">>> LARGADA: MODO LEFT HAND <<<");
        delay(1000); 
        resetearEncoders();
        estado = AVANZANDO;
        break;
      }
      break;
    }

    case AVANZANDO: {
      if (digitalRead(BOTON1) == LOW) {
        movimiento(FRENO_F, {0, 0});
        while(digitalRead(BOTON1) == LOW) { delay(10); } 
        movimientoAnterior = AVANZANDO; 
        decisionAnterior = AVANZANDO; // [PREGIRO] La próxima largada arranca sin un giro a medio confirmar
        contadorDeFiltrado = 0;
        enviarString(">>> PARADA POR BOTON 1. VOLVIENDO A LISTO <<<");
        estado = LISTO;
        break;
      }

      pulsosActuales = (abs(verPulsosEncoderA()) + abs(verPulsosEncoderB())) / 2;
      sensadoActual = actualizarSensado();
      int16_t correccion = calcularCorreccion(sensadoActual);
      // [PREGIRO] Mientras confirma un giro ya va a la velocidad del pregiro: así llega igual al punto
      // de freno, venga rápido o lento. Si la lectura no se confirma, vuelve a la base.
      bool confirmandoGiro = decisionAnterior != AVANZANDO;
      int16_t baseIzq = confirmandoGiro ? VEL_PREGIRO : configRobot.velBaseIzq;
      int16_t baseDer = confirmandoGiro ? VEL_PREGIRO : configRobot.velBaseDer;
      velocidadActual.izquierda = constrain(baseIzq + correccion, 0, 255);
      velocidadActual.derecha = constrain(baseDer - correccion, 0, 255);
      if (sensadoActual.distanciaCent < UMBRAL_FRENTE && !confirmandoGiro) {
        velocidadActual.izquierda = velocidadActual.izquierda/2;
        velocidadActual.derecha = velocidadActual.derecha/2;
      }
      movimiento(AVANZAR, {velocidadActual.izquierda, velocidadActual.derecha});

      // [DEBOUNCE] Solo se decide con lecturas nuevas de los 3 sensores (~33 ms). Contar vueltas del
      // loop (1-2 ms) era contar la misma lectura repetida: una sola lectura mala decidía un giro.
      if (!sensadoActual.nueva) break;

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
      //DEBOUNCE DE DECISIÓN
      //if (condicion) instruccion;
      if (condicionGiroDer) decisionAPriori = PREGIRO_DER;
      if (condicionGiroIzq) decisionAPriori = PREGIRO_IZQ;
      if (condicionAvanzar) decisionAPriori = AVANZANDO;
      if (condicionGiro180) decisionAPriori = GIRANDO_180;

      if (decisionAPriori == decisionAnterior){
        contadorDeFiltrado++; // [DEBOUNCE] "contador = contador ++" no sumaba nunca
      } else {
        contadorDeFiltrado = 1; // [DEBOUNCE] Esta lectura ya es la primera de la decisión nueva
        pulsosAlDetectar = pulsosActuales; // [PREGIRO] El pregiro se cuenta desde acá, no desde que se confirma
      }
      decisionAnterior = decisionAPriori; // [DEBOUNCE] Se guarda siempre, también si es AVANZANDO

      // [DEBOUNCE] Confirmar AVANZANDO no cambia nada (antes reseteaba el PID y los encoders cada 3 lecturas)
      if (contadorDeFiltrado >= CANT_LECTURAS_DEBOUNCE && decisionAPriori != AVANZANDO){
        estado = decisionAPriori; //ACA SE CAMBIA DE ESTADO
        resetearErrorAnterior();
        // [PREGIRO] Los pregiros siguen contando desde la primera detección; el 180 arranca de cero
        if (decisionAPriori == GIRANDO_180) resetearEncoders();
        enviarString(">>> DECISION FILTRADA <<<");
        contadorDeFiltrado = 0;
        decisionAnterior = AVANZANDO; // [PREGIRO] Al volver a AVANZANDO, el próximo giro guarda sus propios pulsos

      } 
      break;
    }

    case PREGIRO_DER: {
      pulsosActuales = (abs(verPulsosEncoderB()) + abs(verPulsosEncoderA())) / 2;
      sensadoActual = actualizarSensado();
      // [PREGIRO] Pulsos desde la primera lectura que pidió el giro (incluye lo que avanzó confirmando)
      if (pulsosActuales - pulsosAlDetectar < configRobot.pulsosPregiroDer) {
        movimiento(AVANZAR, {VEL_PREGIRO, VEL_PREGIRO});
      } else {
        frenar(); // [VELOCIDAD] Frena y espera a que paren las ruedas (antes: FRENO_F + delay fijo)
        estado = GIRANDO_DER;
        resetearErrorAnterior();
        resetearEncoders();
      }
      break;
    }

    case PREGIRO_IZQ: {
      pulsosActuales = (abs(verPulsosEncoderB()) + abs(verPulsosEncoderA())) / 2;
      sensadoActual = actualizarSensado();
      // [PREGIRO] Pulsos desde la primera lectura que pidió el giro (incluye lo que avanzó confirmando)
      if (pulsosActuales - pulsosAlDetectar < configRobot.pulsosPregiroIzq) {
        movimiento(AVANZAR, {VEL_PREGIRO, VEL_PREGIRO});
      } else {
        frenar(); // [VELOCIDAD] Frena y espera a que paren las ruedas (antes: FRENO_F + delay fijo)
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
        frenar(); // [FIX 1] [VELOCIDAD] Espera a que paren las ruedas: la inercia del giro no entra al POSTGIRO
        enviarString(">>> GIRANDO DER <<<"); // [PRECISION] Log después de frenar (antes se mandaba girando)
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
        frenar(); // [FIX 1] [VELOCIDAD] Espera a que paren las ruedas: la inercia del giro no entra al POSTGIRO
        enviarString(">>> GIRANDO IZQUIERDA <<<"); // [PRECISION] Log después de frenar (antes se mandaba girando)
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
        movimiento(AVANZAR, {VEL_POSTGIRO, VEL_POSTGIRO});
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
      bool giro180Der = (movimientoAnterior == GIRANDO_IZQ);
      uint16_t pulsos180 = giro180Der ? configRobot.pulsosGiro180Der : configRobot.pulsosGiro180Izq;
      if (pulsosActuales < pulsos180) {
        movimiento(giro180Der ? GIRAR_DER : GIRAR_IZQ, {configRobot.velBaseIzqGiro180, configRobot.velBaseDerGiro180});
      } else {
        frenar(); // [VELOCIDAD] Espera a que paren las ruedas
        enviarString(giro180Der ? ">>> FIN GIRO 180 DER <<<" : ">>> FIN GIRO 180 IZQ <<<"); // [PRECISION] Log después de frenar
        movimientoAnterior = giro180Der ? GIRANDO_DER : GIRANDO_IZQ;
        estado = POSTGIRO;
        resetearErrorAnterior();
        resetearEncoders();
      }
      break;
    }
  }
}