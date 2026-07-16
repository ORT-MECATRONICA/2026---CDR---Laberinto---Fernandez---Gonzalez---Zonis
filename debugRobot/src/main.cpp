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
  inicializacionSensoresDist(); // Se descomentó para iniciar sensores
  inicializarMotores(); 
  inicializarEncoders();
  enviarString("INICIO");
}


//==============================================================
//                       VOID LOOP
//==============================================================

void loop(){
  // Actualizar las lecturas de los sensores
  sensadoActual = actualizarSensado();

  // Imprimir los datos por el puerto Serial (USB)
  Serial.print("Centro: ");
  Serial.print(sensadoActual.distanciaCent);
  Serial.print(" mm | Derecha: ");
  Serial.print(sensadoActual.distanciaDer);
  Serial.print(" mm | Izquierda: ");
  Serial.print(sensadoActual.distanciaIzq);
  Serial.println(" mm");

  delay(100); // Demora para no saturar la consola
}
