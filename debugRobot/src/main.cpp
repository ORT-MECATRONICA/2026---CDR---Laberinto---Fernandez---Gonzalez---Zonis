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

// Arma el texto " I:x C:y D:z" con las 3 distancias para los logs
String textoLecturas(sensado s) {
  return " I:" + String(s.distanciaIzq) + " C:" + String(s.distanciaCent) + " D:" + String(s.distanciaDer);
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
        movimiento(FRENO_F, {0, 0}); // Frena ANTES de esperar que se suelte el botón
        while(digitalRead(BOTON1) == LOW) { delay(10); } // Antirrebote al soltar
        movimientoAnterior = AVANZANDO; // [180 OPUESTO] La próxima largada arranca sin "último giro"
        enviarString(">>> PARADA POR BOTON 1. VOLVIENDO A LISTO <<<");
        estado = LISTO;
        break;
      }

      sensadoActual = actualizarSensado();

      int16_t correccion = calcularCorreccion(sensadoActual);
      velocidadActual.izquierda = constrain(configRobot.velBaseIzq + correccion, 0, 255);
      velocidadActual.derecha = constrain(configRobot.velBaseDer - correccion, 0, 255);
      movimiento(AVANZAR, {velocidadActual.izquierda, velocidadActual.derecha});

      // "Prioritario" = el lado de la mano elegida; "opuesto" = el otro
      bool derLibre = sensadoActual.distanciaDer >= UMBRAL_PARED_ESTADO_NORMAL;
      bool izqLibre = sensadoActual.distanciaIzq >= UMBRAL_PARED_ESTADO_NORMAL;
      bool frenteLibre = sensadoActual.distanciaCent >= UMBRAL_FRENTE;
      bool prioritarioLibre = (modoActual == MODO_RIGHT_HAND) ? derLibre : izqLibre;
      bool opuestoLibre     = (modoActual == MODO_RIGHT_HAND) ? izqLibre : derLibre;
      MAQUINA_NUEVA pregiroPrioritario = (modoActual == MODO_RIGHT_HAND) ? PREGIRO_DER : PREGIRO_IZQ;
      MAQUINA_NUEVA pregiroOpuesto      = (modoActual == MODO_RIGHT_HAND) ? PREGIRO_IZQ : PREGIRO_DER;

      // ORDEN DE PRIORIDADES: 1° lado de la mano, 2° Frente, 3° lado opuesto, 4° 180
      if (prioritarioLibre) {
        enviarString((pregiroPrioritario == PREGIRO_DER ? ">>> GIRANDO DERECHA <<<" : ">>> GIRANDO IZQUIERDA <<<") + textoLecturas(sensadoActual));
        estado = pregiroPrioritario;
        resetearEncoders();
      } else if (frenteLibre) {
        // Sigue avanzando
      } else if (opuestoLibre) {
        enviarString((pregiroOpuesto == PREGIRO_DER ? ">>> GIRANDO DERECHA <<<" : ">>> GIRANDO IZQUIERDA <<<") + textoLecturas(sensadoActual));
        estado = pregiroOpuesto;
        resetearEncoders();
      } else {
        // Pared al frente y a los dos lados: frena y gira 180
        movimiento(FRENO_F, {0,0});
        enviarString(">>> GIRANDO 180 <<<" + textoLecturas(sensadoActual));
        delay(TIEMPO_FRENO_MS);
        estado = GIRANDO_180;
        resetearErrorAnterior();
        resetearEncoders();
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
        movimiento(FRENO_F, {0,0});
        delay(TIEMPO_FRENO_MS); // [FIX 1] Sostener el freno antes de resetear encoders
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