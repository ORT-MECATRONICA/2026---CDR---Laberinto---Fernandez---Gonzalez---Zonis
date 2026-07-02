#include "main.h"
#include <Arduino.h>
#include "hardware/logger/logger.h"
#include "hardware/sensoresDistancia/sensoresDistancia.h"
#include "hardware/sensorPiso/sensorPiso.h"
#include "memoria/funcionesMapeo.h"

#include "hardware/movimiento/PID.h"

#include "hardware/movimiento/puenteH.h"

// uint8_t errorAnterior = 0; // -ANTIGRAVITY- Eliminada por estar en desuso y chocar con el tipo del PID
//==============================================================
//                CREACIÓN DE VARIABLES GLOBALES
//==============================================================

sensado sensadoActual = {0,0,0};
ESTADOS estadoActual = SWITCHEAR_ESTADO;
VELOCIDAD velocidadActual = {0,0};

//==============================================================
//                     VOID SETUP
//==============================================================

void setup (){
  pinMode(BOTON, INPUT_PULLUP); // -ANTIGRAVITY- Cambiado a INPUT_PULLUP (recordá cambiar de pin si no pusiste resistencia)
  inicializarLogger();
  inicializacionSensoresDist();
  inicializarMotores(); // Faltaba inicializar los motores (PWM/LEDC)
}


//==============================================================
//                       VOID LOOP
//==============================================================

void loop(){
  uint8_t lecturaBoton = digitalRead(BOTON);
  switch(estadoActual){
     case SWITCHEAR_ESTADO:
        avanzar({0,0}); //frena los motores, PWM a 0.
        if(lecturaBoton == LOW){
          estadoActual = AVANZAR;
          Serial.println("Botón presionado, cambiando a estado AVANZAR");
          while(digitalRead(BOTON) == LOW) { delay(10); } // Espera a que se suelte el botón (antirrebote)
        } else {
          estadoActual = SWITCHEAR_ESTADO;
        }
        break;
        
      case AVANZAR:
        sensadoActual = actualizarSensado();
        Serial.println(" Distancia Derecha: " + String(sensadoActual.distanciaDer) + " | Distancia Izquierda: " + String(sensadoActual.distanciaIzq) + " | Distancia Frente: " + String(sensadoActual.distanciaCent));
        int16_t calculoCorreccion = calcularCorreccion(sensadoActual);
        // Evitamos que la corrección sea muy grande (limitamos entre -50 y 50)
        calculoCorreccion = constrain(calculoCorreccion, -50, 50);
         
        // Calculamos las velocidades
        int16_t velIzq = VEL_BASE_IZQ - calculoCorreccion; // -ANTIGRAVITY- Se invirtió el signo (de + a -)
        int16_t velDer = VEL_BASE_DER + calculoCorreccion; // -ANTIGRAVITY- Se invirtió el signo (de - a +)
        
        // Evitamos que el PWM de las ruedas sea negativo (menor a 0) o mayor a 255
        velocidadActual.izquierda = constrain(velIzq, 0, 255);
        velocidadActual.derecha = constrain(velDer, 0, 255);
        //Serial.println("Velocidad Izquierda: " + String(velocidadActual.izquierda) + " | Velocidad Derecha: " + String(velocidadActual.derecha));
        avanzar(velocidadActual);


        //Para frenar cuando tocamos el botón. 
        if(lecturaBoton == LOW){
          avanzar({0,0}); // Frenamos antes de trabarnos en el while
          estadoActual = SWITCHEAR_ESTADO;
          while(digitalRead(BOTON) == LOW) { delay(10); } // Espera a que se suelte el botón (antirrebote)
        } else {
          estadoActual = AVANZAR;
        }
        break;
  }
}
