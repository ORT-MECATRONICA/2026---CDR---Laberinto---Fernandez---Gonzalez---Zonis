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

// Variables para el estado FRENANDO
unsigned long tiempoInicioFreno = 0;
MAQUINA_ESTADOS estadoPostFreno = LISTO;


uint16_t PULSOS_GIRO_90_DER = 300;
uint16_t PULSOS_GIRO_90_IZQ = 280;
uint16_t PULSOS_GIRO_180 = 300;

// Variables para control de celda y odometría adaptativa (R1, R2, R3, R4)
uint16_t limitePulsosActual = PULSOS_CELDA;
bool flancoDetectado = false;
bool habiaParedIzq = false;
bool habiaParedDer = false;
uint32_t pulsosBaseCelda = 0;
bool graciaPIDFinalizada = false;
bool aproximandoParedFrontal = false;

//==============================================================
//                FUNCIONES AUXILIARES DE ESTADO
//==============================================================

void iniciarAvanceCelda() {
  resetearEncoders();
  resetearErrorAnterior();
  limitePulsosActual = PULSOS_CELDA;
  flancoDetectado = false;
  habiaParedIzq = false;
  habiaParedDer = false;
  pulsosBaseCelda = 0;
  graciaPIDFinalizada = false;
  aproximandoParedFrontal = false;
  sensadoActual = actualizarSensado();
  enviarString(">>> AVANZANDO <<<");
  estado = AVANZANDO;
}

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
      
      if (hayDatosBT()) {
        estado = INFORMACION_RECIBIDA;
        break;
      }
      
      if (digitalRead(BOTON1) == LOW) {
        //VER SI EL ANTIRREBOTE ES ÓPTIMO
        while(digitalRead(BOTON1) == LOW) { delay(10); } // Esperar a que se suelte el botón
        enviarString(">>> INICIANDO AVANCE <<<");
        iniciarAvanceCelda();
      }
      break;
    }

    case INFORMACION_RECIBIDA: {
      procesarTramaBT(PULSOS_GIRO_90_DER, PULSOS_GIRO_90_IZQ);
      estado = LISTO;
      break;
    }

    case AVANZANDO: {
      int32_t pulsosA = abs(verPulsosEncoderA());
      int32_t pulsosB = abs(verPulsosEncoderB());
      pulsosActuales = (pulsosA + pulsosB) / 2;
      uint32_t pulsosTotalesCelda = pulsosBaseCelda + pulsosActuales;

      sensadoActual = actualizarSensado();

      // R1: Detección y reseteo por flanco lateral
      if (!flancoDetectado) {
        if (sensadoActual.distanciaIzq > 0 && sensadoActual.distanciaIzq <= UMBRAL_PARED_ESTADO_NORMAL) {
          habiaParedIzq = true;
        }
        if (sensadoActual.distanciaDer > 0 && sensadoActual.distanciaDer <= UMBRAL_PARED_ESTADO_NORMAL) {
          habiaParedDer = true;
        }

        bool flancoIzq = habiaParedIzq && (sensadoActual.distanciaIzq > UMBRAL_PARED_ESTADO_NORMAL);
        bool flancoDer = habiaParedDer && (sensadoActual.distanciaDer > UMBRAL_PARED_ESTADO_NORMAL);

        if ((flancoIzq || flancoDer) && pulsosTotalesCelda > PULSOS_MIN_DETECCION_FLANCO) {
          flancoDetectado = true;
          pulsosBaseCelda += pulsosActuales;
          resetearEncoders();
          pulsosActuales = 0;
          limitePulsosActual = PULSOS_CELDA_MEDIA;
          enviarString(">>> FLANCO DETECTADO: RESET A 400 PULSOS <<<");
        }
      }

      // R2: Condiciones de parada
      // 1. Parada por pared frontal a <= 50 mm (con guarda de validez física >= DISTANCIA_MIN_VALIDA)
      bool stopPorParedFrontal = (sensadoActual.distanciaCent >= DISTANCIA_MIN_VALIDA &&
                                  sensadoActual.distanciaCent <= DISTANCIA_PARADA_FRENTE);

      // 2. Detección de pared frontal en aproximación
      bool paredAlFrente = (sensadoActual.distanciaCent >= DISTANCIA_MIN_VALIDA &&
                            sensadoActual.distanciaCent <= UMBRAL_PARED_FRENTE);

      // Enclavar aproximación a pared frontal cerca o más allá del límite de celda
      if (paredAlFrente && pulsosActuales >= (limitePulsosActual - 100)) {
        aproximandoParedFrontal = true;
      }

      // 3. Parada por encoders (400 pulsos si hubo flanco, o 800 fallback si no hubo flanco).
      // R2 sobreescribe la parada por encoders si hay pared al frente o si se enclavó aproximación.
      bool stopPorEncoders = (pulsosActuales >= limitePulsosActual) && !paredAlFrente && !aproximandoParedFrontal;

      // 4. Watchdog de seguridad (anti-colisión ante fallo de sensor frontal)
      bool stopPorWatchdog = (pulsosTotalesCelda >= PULSOS_WATCHDOG_SEGURIDAD);

      if (stopPorParedFrontal || stopPorEncoders || stopPorWatchdog) {
        enviarString(">>> INGRESO A DECISIÓN (FRENANDO) <<<");
        movimiento(FRENO_F, {0,0});
        tiempoInicioFreno = millis();
        estadoPostFreno = DECISION;
        estado = FRENANDO;
      } else {
        // R3: Corrección PID y gracia post-giro (ceguera en los primeros 100 pulsos con bumpless transfer)
        int16_t correccion = calcularCorreccion(sensadoActual);
        if (!graciaPIDFinalizada) {
          if (pulsosTotalesCelda < PULSOS_GRACIA_PID) {
            correccion = 0;
          } else {
            graciaPIDFinalizada = true;
          }
        }

        // FIX PID: El Motor A (velocidadActual.izquierda) está mecánicamente en la rueda DERECHA.
        // Por eso invertimos los signos de la corrección y asignamos cruzadas las velocidades base.
        velocidadActual.izquierda = constrain(VEL_BASE_DER - correccion, 0, 255);
        velocidadActual.derecha = constrain(VEL_BASE_IZQ + correccion, 0, 255);

        movimiento(AVANZAR, {velocidadActual.izquierda, velocidadActual.derecha});
      }
      break;
    }

    case DECISION: {
      sensadoActual = actualizarSensado();
      enviarString(String(sensadoActual.distanciaDer) + " | " + String(sensadoActual.distanciaCent) + " | " + String(sensadoActual.distanciaIzq));
      //ORDEN Y PROGRESO!!! 
      bool condicionGiroDer = sensadoActual.distanciaDer > UMBRAL_PARED_ESTADO_NORMAL; // TODO LO DEMÁS NO ME IMPORTA, HAGAN LO QUE HAGAN LOS OTROS SENSORES, GIRO!
      // BUG FIX: Se agregó el igual (<=) en lugar de estricto (<) para evitar quedarse atrapado si la lectura es exactamente 130
      bool condicionAvanzar = sensadoActual.distanciaDer <= UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaCent > UMBRAL_PARED_ESTADO_NORMAL;
      bool condicionGiroIzq = sensadoActual.distanciaDer <= UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaCent <= UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaIzq > UMBRAL_PARED_ESTADO_NORMAL;
      bool condicionGiro180 = sensadoActual.distanciaDer <= UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaCent <= UMBRAL_PARED_ESTADO_NORMAL && sensadoActual.distanciaIzq <= UMBRAL_PARED_ESTADO_NORMAL;

      if (condicionGiroDer) {
        enviarString(">>> GIRANDO DERECHA <<<");
        resetearEncoders();
        estado = GIRANDO_DER;
      } else if (condicionAvanzar) {
        iniciarAvanceCelda();
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
      pulsosActuales = abs(verPulsosEncoderA());
      
      if (pulsosActuales < PULSOS_GIRO_90_DER) {
        movimiento(GIRAR_DER, {VEL_BASE_IZQ, VEL_BASE_DER});
      } else {
        enviarString(">>> FIN GIRO DER <<<");
        movimiento(FRENO_F, {0,0});
        tiempoInicioFreno = millis();
        estadoPostFreno = AVANZANDO;
        estado = FRENANDO;
      }
      break;
    }

    case GIRANDO_IZQ: {
      pulsosActuales = abs(verPulsosEncoderB()); 
      
      if (pulsosActuales < PULSOS_GIRO_90_IZQ) {
        movimiento(GIRAR_IZQ, {VEL_BASE_IZQ, VEL_BASE_DER});
      } else {
        enviarString(">>> FIN GIRO IZQ <<<");
        movimiento(FRENO_F, {0,0});
        tiempoInicioFreno = millis();
        estadoPostFreno = AVANZANDO;
        estado = FRENANDO;
      }
      break;
    }

    case GIRANDO_180: {
      pulsosActuales = abs(verPulsosEncoderA()); 
      
      if (pulsosActuales < PULSOS_GIRO_180) {
        movimiento(GIRAR_DER, {VEL_BASE_IZQ, VEL_BASE_DER});
      } else {
        enviarString(">>> FIN GIRO 180 <<<");
        movimiento(FRENO_F, {0,0});
        tiempoInicioFreno = millis();
        estadoPostFreno = AVANZANDO;
        estado = FRENANDO;
      }
      break;
    }
    case FRENANDO: {
      movimiento(FRENO_F, {0,0}); // Mantener el freno activo
      if (millis() - tiempoInicioFreno >= 150) { // 150ms de pausa estabilizadora
        if (estadoPostFreno == AVANZANDO) {
          iniciarAvanceCelda();
        } else {
          resetearErrorAnterior();
          resetearEncoders();
          estado = estadoPostFreno;
        }
      }
      break;
    }
  }
}
