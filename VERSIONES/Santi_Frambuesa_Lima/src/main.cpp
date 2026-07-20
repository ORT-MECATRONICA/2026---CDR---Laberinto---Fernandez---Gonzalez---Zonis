#include "main.h"
#include <Arduino.h>
#include "hardware/logger/logger.h"
#include "hardware/sensoresDistancia/sensoresDistancia.h"
#include "hardware/sensorPiso/sensorPiso.h"
#include "hardware/movimiento/PID.h"
#include "hardware/movimiento/puenteH.h"
#include "hardware/encoders/encoders.h"
#include "config.h"

//==============================================================
//                CREACIÓN DE VARIABLES GLOBALES
//==============================================================

sensado sensadoActual = {0,0,0};
ESTADOS estadoActual = SWITCHEAR_ESTADO;
VELOCIDAD velocidadActual = {0,0};
unsigned long tiempoInicioGiro = 0;



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
    case SWITCHEAR_ESTADO: {
      actualizarSensado();
      if(lecturaBoton == LOW){
        estadoActual = AVANZANDO;
        resetearErrorAnterior();
        break;
      }
      break;
    }

    case AVANZANDO: {
      sensadoActual = actualizarSensado();
      int16_t calculoCorreccion = calcularCorreccion(sensadoActual);
      calculoCorreccion = constrain(calculoCorreccion, -50, 50);
      
      int16_t velIzq = VEL_BASE_IZQ - calculoCorreccion; 
      int16_t velDer = VEL_BASE_DER + calculoCorreccion; 
      
      // Evitamos que el PWM de las ruedas sea negativo o mayor a 255
      velocidadActual.izquierda = constrain(velIzq, 0, 255);
      velocidadActual.derecha = constrain(velDer, 0, 255);
      movimiento(AVANZAR, velocidadActual);

      // --- DETECCIÓN DE INTERSECCIONES ---
      static unsigned long tiempoViendoHueco = 0;
      bool hayParedFrente = (sensadoActual.distanciaCent < UMBRAL_FRENO_F);
      bool hayHuecoLaterales = (sensadoActual.distanciaDer > UMBRAL_LECTURA) || (sensadoActual.distanciaIzq > UMBRAL_LECTURA);

      if (hayParedFrente) {
          // Si chocamos contra la pared frontal, vamos a decisión de inmediato
          estadoActual = DECISION;
          tiempoViendoHueco = 0;
      } 
      else if (hayHuecoLaterales) {
          // Si vemos un hueco lateral, avanzamos 150ms para entrar a la intersección
          if (tiempoViendoHueco == 0) {
              tiempoViendoHueco = millis();
          } else if (millis() - tiempoViendoHueco > 200) {
              estadoActual = DECISION;
              tiempoViendoHueco = 0;
          }
      } 
      else {
          // Pasillo recto normal
          tiempoViendoHueco = 0;
      }
      break;
    }

    //==============================================================
    case DECISION: {
      // Clavamos los frenos para matar la inercia (evita patinar contra la pared frontal)
      VELOCIDAD velFreno = {0, 0};
      movimiento(FRENO_F, velFreno);
      delay(80); // Pequeña pausa para que las llantas se agarren al piso y frene del todo
      
      sensadoActual = actualizarSensado();
      
      // REGLA DE LA MANO DERECHA ESTRICTA
      if (sensadoActual.distanciaDer > UMBRAL_LECTURA) {
          estadoActual = GIRANDO_DER;
          tiempoInicioGiro = millis();
      } 
      else if (sensadoActual.distanciaCent > UMBRAL_LECTURA) {
          // Si avanzamos recto por la intersección, también debemos ignorar el PID un instante
          estadoActual = SALIENDO_INTERSECCION;
          tiempoInicioGiro = millis();
      } 
      else if (sensadoActual.distanciaIzq > UMBRAL_LECTURA) {
          estadoActual = GIRANDO_IZQ;
          tiempoInicioGiro = millis();
      } 
      else {
          estadoActual = ENCIERRO;
          tiempoInicioGiro = millis();
      }
      break;
    }

    //==============================================================
    case GIRANDO_DER: {
      actualizarSensado(); // Mantener lecturas activas
      VELOCIDAD velGiro = {VEL_GIRO_IZQ, VEL_GIRO_DER};
      movimiento(GIRAR_DER, velGiro);
      
      if(millis() - tiempoInicioGiro >= DELAY_TIEMPO_GIRO_90) {
        resetearErrorAnterior();
        estadoActual = SALIENDO_INTERSECCION;
        tiempoInicioGiro = millis();
      }
      break;
    }

    //==============================================================
    case GIRANDO_IZQ: {
      actualizarSensado(); // Mantener lecturas activas
      VELOCIDAD velGiro = {VEL_GIRO_IZQ, VEL_GIRO_DER};
      movimiento(GIRAR_IZQ, velGiro);
      
      if(millis() - tiempoInicioGiro >= DELAY_TIEMPO_GIRO_90) {
        resetearErrorAnterior();
        estadoActual = SALIENDO_INTERSECCION;
        tiempoInicioGiro = millis();
      }
      break;
    }

    //==============================================================
    case FRENADO_F: {
      break;
    }

    //==============================================================
    case ENCIERRO: {
      actualizarSensado(); // Mantener lecturas activas
      VELOCIDAD velGiro = {VEL_GIRO_IZQ, VEL_GIRO_DER};
      movimiento(GIRAR_DER, velGiro);
      
      if(millis() - tiempoInicioGiro >= (DELAY_TIEMPO_GIRO_90 * 2)) {
        resetearErrorAnterior();
        estadoActual = SALIENDO_INTERSECCION;
        tiempoInicioGiro = millis();
      }
      break;
    }

    //==============================================================
    case SALIENDO_INTERSECCION: {
      actualizarSensado(); 
      // Avanza recto a velocidad constante sin hacerle caso a las paredes (Sin PID)
      VELOCIDAD velSalida = {VEL_BASE_IZQ, VEL_BASE_DER};
      movimiento(AVANZAR, velSalida);
      
      // Avanza a ciegas por 250ms para salir del centro de la intersección y entrar al pasillo
      if(millis() - tiempoInicioGiro >= 3000) { 
        resetearErrorAnterior();
        estadoActual = AVANZANDO;
      }
      break;
    }
  }
}
