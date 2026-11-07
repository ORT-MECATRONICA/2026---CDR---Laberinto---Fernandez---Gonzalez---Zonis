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

// [180 OPUESTO] Último giro realizado (GIRANDO_DER / GIRANDO_IZQ). Ahora se usa para elegir el lado del 180.
// AVANZANDO = todavía no giró.
MAQUINA_NUEVA movimientoAnterior = AVANZANDO;

uint32_t pulsosActuales = 0;
// uint32_t tiempoInicioGiro = 0; // [NO UTILIZADA ACTUALMENTE - MANTENIDA POR REQUISITO]

// Modo de navegación: Right Hand (por defecto) o Left Hand
MODO_SEGUIDOR modoActual = MODO_RIGHT_HAND;

// [DEBOUNCE] Último estado pedido por los sensores y cuántas lecturas seguidas lo pidieron
MAQUINA_NUEVA estadoDetectado = AVANZANDO;
uint8_t lecturasIguales = 0;

// [LOG LECTURAS] Arma el texto " I:x C:y D:z" con las 3 distancias, igual que el log del 180
String textoLecturas(sensado s) {
  return " I:" + String(s.distanciaIzq) + " C:" + String(s.distanciaCent) + " D:" + String(s.distanciaDer);
}

// [DECISION QUIETO] Se llama con el robot FRENADO al terminar el pregiro (centro de la celda).
// Vuelve a leer los sensores (mediana de 3 lecturas) y decide con las mismas prioridades que
// AVANZANDO. Si lo detectado en movimiento era una lectura falsa, no gira y sigue avanzando.
MAQUINA_NUEVA decidirQuieto(const char* origen) {
  sensado s = leerMediana3();
  bool derLibre    = s.distanciaDer  >= UMBRAL_PARED_ESTADO_NORMAL;
  bool izqLibre    = s.distanciaIzq  >= UMBRAL_PARED_ESTADO_NORMAL;
  bool frenteLibre = s.distanciaCent >= UMBRAL_FRENTE;

  MAQUINA_NUEVA siguiente;
  if (modoActual == MODO_RIGHT_HAND) {
    // ORDEN DE PRIORIDADES RIGHT HAND: 1° Derecha, 2° Frente, 3° Izquierda, 4° 180
    if (derLibre)         siguiente = GIRANDO_DER;
    else if (frenteLibre) siguiente = AVANZANDO;
    else if (izqLibre)    siguiente = GIRANDO_IZQ;
    else                  siguiente = GIRANDO_180;
  } else {
    // ORDEN DE PRIORIDADES LEFT HAND: 1° Izquierda, 2° Frente, 3° Derecha, 4° 180
    if (izqLibre)         siguiente = GIRANDO_IZQ;
    else if (frenteLibre) siguiente = AVANZANDO;
    else if (derLibre)    siguiente = GIRANDO_DER;
    else                  siguiente = GIRANDO_180;
  }

  // [LOG LECTURAS] Lectura con el robot quieto y lo que decidió
  String accion = (siguiente == GIRANDO_DER) ? " -> GIRA DERECHA <<<"
                : (siguiente == GIRANDO_IZQ) ? " -> GIRA IZQUIERDA <<<"
                : (siguiente == GIRANDO_180) ? " -> GIRA 180 <<<"
                : " -> SIGUE DERECHO (era lectura falsa) <<<";
  enviarString(String(origen) + accion + textoLecturas(s));
  return siguiente;
}

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
        movimiento(FRENO_F, {0, 0}); // [PRECISION] Frena ANTES de esperar que se suelte el botón
        while(digitalRead(BOTON1) == LOW) { delay(10); } // Antirrebote al soltar
        estadoDetectado = AVANZANDO; lecturasIguales = 0; // [DEBOUNCE] Arranca de cero en la próxima largada
        movimientoAnterior = AVANZANDO; // [180 OPUESTO] La próxima largada arranca sin "último giro"
        enviarString(">>> PARADA POR BOTON 1. VOLVIENDO A LISTO <<<");
        estado = LISTO;
        break;
      }

      pulsosActuales = (abs(verPulsosEncoderA()) + abs(verPulsosEncoderB())) / 2;
      sensadoActual = actualizarSensado();

      // [DECISION QUIETO] Seguridad: si la pared de enfrente ya está muy cerca, frena y decide quieto
      // aunque el debounce todavía no haya confirmado nada (si las lecturas laterales titilan, el
      // debounce se reinicia y antes el robot podía seguir derecho hasta chocar de frente).
      if (sensadoActual.distanciaCent < 50) {
        movimiento(FRENO_F, {0,0});
        delay(TIEMPO_FRENO_MS);
        estado = decidirQuieto(">>> FRENTE MUY CERCA");
        resetearErrorAnterior();
        resetearEncoders();
        estadoDetectado = AVANZANDO; lecturasIguales = 0;
        break;
      }

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

      // [DEBOUNCE] Estado que piden los sensores en esta vuelta (las 4 condiciones son excluyentes)
      MAQUINA_NUEVA detectado = condicionGiroDer ? PREGIRO_DER
                              : condicionAvanzar ? AVANZANDO
                              : condicionGiroIzq ? PREGIRO_IZQ
                              : GIRANDO_180;

      // [DEBOUNCE] Solo se cuenta cuando llegó una lectura completa nueva de los 3 sensores
      if (sensadoActual.nueva) {
        if (detectado == estadoDetectado) {
          if (lecturasIguales < 255) lecturasIguales++;
        } else {
          estadoDetectado = detectado;
          lecturasIguales = 1;
          // Los pulsos del pregiro se cuentan desde la PRIMERA detección (igual que antes),
          // así PRED/PREI no hay que recalibrarlos por la demora de confirmar.
          resetearEncoders();
        }
      }
      bool confirmado = detectado == estadoDetectado && lecturasIguales >= LECTURAS_CONFIRMACION;

      if (detectado != AVANZANDO && !confirmado) {
        // [DEBOUNCE] Todavía no hay 3 lecturas iguales: sigue avanzando con el PID de arriba
      } else if (condicionGiroDer) {
        enviarString(">>> GIRANDO DERECHA <<<" + textoLecturas(sensadoActual)); // [LOG LECTURAS]
        // resetearEncoders(); // [DEBOUNCE] Ahora se resetea en la primera detección (ver arriba)
        estado = PREGIRO_DER;
      } else if (condicionAvanzar) {
        // Continúa avanzando
        estado = AVANZANDO;
      } else if (condicionGiroIzq) {
        enviarString(">>> GIRANDO IZQUIERDA <<<" + textoLecturas(sensadoActual)); // [LOG LECTURAS]
        // resetearEncoders(); // [DEBOUNCE] Ahora se resetea en la primera detección (ver arriba)
        estado = PREGIRO_IZQ;
      } else if (condicionGiro180) {
        // [DECISION QUIETO] Antes frenaba acá mismo, a ~100 mm de la pared, con el robot todavía en
        // la celda anterior: los laterales podían estar mirando la celda equivocada (180 cuando había
        // que girar). Ahora se acerca a la pared igual que en un giro con el frente bloqueado (pregiro
        // del lado no prioritario, que corta por pulsos o con la pared a 50 mm), frena y decide quieto
        // al final del pregiro. El 180 se confirma ahí con la mediana de 3 lecturas.
        enviarString(">>> PARED AL FRENTE (POSIBLE 180) <<<" + textoLecturas(sensadoActual)); // [LOG LECTURAS]
        estado = (modoActual == MODO_RIGHT_HAND) ? PREGIRO_IZQ : PREGIRO_DER;
      } else {
        enviarString(">>> ERROR DE SENSADO <<<");
        movimiento(FRENO_F, {0,0});
      }

      // [DEBOUNCE] Si salió de AVANZANDO, el contador arranca de cero para la próxima decisión
      if (estado != AVANZANDO) {
        estadoDetectado = AVANZANDO;
        lecturasIguales = 0;
      }
      break;
    }

    case PREGIRO_DER: {
      pulsosActuales = (abs(verPulsosEncoderB()) + abs(verPulsosEncoderA())) / 2;
      sensadoActual = actualizarSensado();
      if (pulsosActuales < configRobot.pulsosPregiroDer && sensadoActual.distanciaCent >= 50) {
        movimiento(AVANZAR, {configRobot.velBaseIzq, configRobot.velBaseDer});
      } else {
        movimiento(FRENO_F, {0,0});
        delay(TIEMPO_FRENO_MS); // [FIX 1] Sostener el freno antes de resetear encoders
        // [DECISION QUIETO] Antes giraba directo. Ahora, frenado en el centro de la celda, vuelve a
        // leer y decide: si la abertura era falsa no gira (y el log se manda con el robot quieto)
        estado = decidirQuieto(">>> FIN PREGIRO DERECHA");
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
        movimiento(FRENO_F, {0,0});
        delay(TIEMPO_FRENO_MS); // [FIX 1] Sostener el freno antes de resetear encoders
        // [DECISION QUIETO] Antes giraba directo. Ahora, frenado en el centro de la celda, vuelve a
        // leer y decide: puede ser izquierda, derecha, 180 o seguir (si la detección era falsa)
        estado = decidirQuieto(">>> FIN PREGIRO IZQUIERDA");
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
        movimiento(FRENO_F, {0,0});
        enviarString(">>> GIRANDO DER <<<"); // [PRECISION] Log después de frenar (antes se mandaba girando)
        delay(TIEMPO_FRENO_MS); // [FIX 1] Sostener el freno: la inercia del giro ya no entra al POSTGIRO
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
        movimiento(FRENO_F, {0,0});
        enviarString(">>> GIRANDO IZQUIERDA <<<"); // [PRECISION] Log después de frenar (antes se mandaba girando)
        delay(TIEMPO_FRENO_MS); // [FIX 1] Sostener el freno: la inercia del giro ya no entra al POSTGIRO
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
      if (pulsosActuales < configRobot.pulsosPostgiro && sensadoActual.distanciaCent >= 50) { // [PRECISION] Corte por pared al frente, igual que PREGIRO
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
      // [180 OPUESTO] Gira hacia el lado contrario al último giro: si el último fue a la izquierda,
      // el 180 es a la derecha; si fue a la derecha (o todavía no giró), a la izquierda como antes.
      // movimientoAnterior recién cambia al terminar el giro, así que el lado no cambia a mitad del 180.
      bool giro180Der = (movimientoAnterior == GIRANDO_IZQ);
      uint16_t pulsos180 = giro180Der ? configRobot.pulsosGiro180Der : configRobot.pulsosGiro180Izq;
      if (pulsosActuales < pulsos180) {
        movimiento(giro180Der ? GIRAR_DER : GIRAR_IZQ, {configRobot.velBaseIzqGiro180, configRobot.velBaseDerGiro180});
      } else {
        movimiento(FRENO_F, {0,0});
        enviarString(giro180Der ? ">>> FIN GIRO 180 DER <<<" : ">>> FIN GIRO 180 IZQ <<<"); // [PRECISION] Log después de frenar
        delay(TIEMPO_FRENO_MS); // [FIX 1] Sostener el freno: la inercia del giro ya no entra al POSTGIRO
        // [180 OPUESTO] Se guarda el lado del 180, así el próximo 180 seguido sale para el otro lado
        movimientoAnterior = giro180Der ? GIRANDO_DER : GIRANDO_IZQ;
        estado = POSTGIRO;
        resetearErrorAnterior();
        resetearEncoders();
      }
      break;
    }
  }
}