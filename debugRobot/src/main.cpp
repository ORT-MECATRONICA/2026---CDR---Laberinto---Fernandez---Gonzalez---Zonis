#include "main.h"
#include <Arduino.h>
#include "hardware/logger/logger.h"
#include "hardware/sensoresDistancia/sensoresDistancia.h"
#include "hardware/sensorPiso/sensorPiso.h"
#include "memoria/funcionesMapeo.h"
#
#include "hardware/movimiento/PID.h"
#include "hardware/movimiento/puenteH.h"
#include "hardware/encoders/encoders.h"

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
  inicializarEncoders();
  inicializarLogger();
  enviarString("INICIO");
}


//==============================================================
//                       VOID LOOP
//==============================================================

void loop(){
  static unsigned long lastPrintTime = 0;

  //sensadoActual = actualizarSensado();
  //Serial.println("Distancia Cent: " + String(sensadoActual.distanciaCent) + " Distancia Der: " + String(sensadoActual.distanciaDer) + " Distancia Izq: " + String(sensadoActual.distanciaIzq));
  movimiento(AVANZAR, {100,100});

  if (millis() - lastPrintTime >= 5000) {
    lastPrintTime = millis();
    int32_t pulsosA = verPulsosEncoderA();
    int32_t pulsosB = verPulsosEncoderB();
    String mensaje = "Pulsos A: " + String(pulsosA) + " - Pulsos B: " + String(pulsosB);
    enviarString(mensaje);
  }

  delay(500);
}
