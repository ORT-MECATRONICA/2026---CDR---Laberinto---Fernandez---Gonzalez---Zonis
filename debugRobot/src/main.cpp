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
  static bool robotDetenido = false;

  if (!robotDetenido) {
    movimiento(AVANZAR, {70,70});

    // Cálculos para 2 metros (2000 mm):
    // Diámetro de la rueda: 15 mm -> Perímetro = 15 * PI = 47.1239 mm
    // Vueltas necesarias = 2000 mm / 47.1239 mm = 42.44 vueltas
    // ¡ATENCIÓN! Cambiá el valor de PPR por la cantidad de Pulsos Por Revolución reales de tus motores.
    float PPR = 360.0; // <--- REEMPLAZAR POR EL VALOR DE TUS ENCODERS
    float pulsosObjetivo = 42.44 * PPR;

    int32_t pulsosA = verPulsosEncoderA();
    int32_t pulsosB = verPulsosEncoderB();

    // Comparamos el valor absoluto por si los encoders cuentan en negativo
    if (abs(pulsosA) >= pulsosObjetivo || abs(pulsosB) >= pulsosObjetivo) {
      movimiento(FRENO_F, {0,0});
      robotDetenido = true;
      enviarString("2 metros alcanzados. Robot detenido.");
    }
  }

  if (millis() - lastPrintTime >= 5000) {
    lastPrintTime = millis();
    int32_t pulsosA = verPulsosEncoderA();
    int32_t pulsosB = verPulsosEncoderB();
    String mensaje = "Pulsos A: " + String(pulsosA) + " - Pulsos B: " + String(pulsosB);
    enviarString(mensaje);
  }

  delay(500);
}
