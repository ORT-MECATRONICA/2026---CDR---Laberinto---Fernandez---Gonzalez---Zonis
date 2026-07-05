#include "main.h"
#include <Arduino.h>
#include "hardware/logger/logger.h"
#include "hardware/sensoresDistancia/sensoresDistancia.h"
#include "hardware/sensorPiso/sensorPiso.h"
#include "memoria/funcionesMapeo.h"
#
#include "hardware/movimiento/PID.h"
#include "hardware/movimiento/puenteH.h"

#include "hardware/movimiento/puenteH.h"

uint8_t errorAnterior = 0;
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
  pinMode(BOTON, INPUT);
  inicializarLogger();
  //inicializacionSensoresDist();
  inicializarMotores(); 
  movimiento(AVANZAR, {230,230});
  delay(500);
}


//==============================================================
//                       VOID LOOP
//==============================================================

void loop(){
  //sensadoActual = actualizarSensado();
  //Serial.println("Distancia Cent: " + String(sensadoActual.distanciaCent) + " Distancia Der: " + String(sensadoActual.distanciaDer) + " Distancia Izq: " + String(sensadoActual.distanciaIzq));
  movimiento(AVANZAR, {100,100});
  delay(500);
}
