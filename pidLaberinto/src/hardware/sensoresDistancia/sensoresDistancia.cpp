#include "sensoresDistancia.h"
#include <Arduino.h>
#include <Wire.h>
#include <VL53L0X.h> 
#include "config.h"
//LAS REFERENCIAS DE STM ELECTRONICS SON LAS DE LA API FUENTE
//ESTOY USANDO UNA BIBLOTECA DE POLOLU QUE USA ESA REFERENCIA
//https://github.com/pololu/vl53l0x-arduino/blob/master/README.md


VL53L0X sensorDer, sensorIzq, sensorCent;

void inicializacionSensoresDist(){
  // Es fundamental inicializar el bus I2C
  Wire.begin();

  // 1. APAGAR TODOS LOS SENSORES
  // Para usar múltiples sensores en el mismo bus, todos arrancan con la dir 0x29
  // Hay que apagar todos poniendo XSHUT en LOW, y prenderlos de a uno.
  pinMode(xshutPinDer, OUTPUT);
  pinMode(xshutPinCent, OUTPUT);
  pinMode(xshutPinIzq, OUTPUT);
  
  digitalWrite(xshutPinDer, LOW);
  digitalWrite(xshutPinCent, LOW);
  digitalWrite(xshutPinIzq, LOW);
  delay(10); // Dar tiempo para asegurar el apagado


  // 2. Encender y configurar Sensor Derecho
  digitalWrite(xshutPinDer, HIGH);
  delay(10); // esperar boot del sensor

  sensorDer.setTimeout(500);
  if (!sensorDer.init()) {
    Serial.printf("ERROR: fallo init sensor en pin %d\n", xshutPinDer);
    while (true) delay(1000);
  }

  sensorDer.setAddress(adressDer);
  sensorDer.startContinuous(0);

  // 3. Encender y configurar Sensor Central

  digitalWrite(xshutPinCent, HIGH);
  delay(10); // esperar boot del sensor

  sensorCent.setTimeout(500);
  if (!sensorCent.init()) {
    Serial.printf("ERROR: fallo init sensor en pin %d\n", xshutPinCent);
    while (true) delay(1000);
  }

  sensorCent.setAddress(adressCent);
  sensorCent.startContinuous(0);

  // 4. Encender y configurar Sensor Izquierdo

  digitalWrite(xshutPinIzq, HIGH);
  delay(10); // esperar boot del sensor

  sensorIzq.setTimeout(500);
  if (!sensorIzq.init()) {
    Serial.printf("ERROR: fallo init sensor en pin %d\n", xshutPinIzq);
    while (true) delay(1000);
  }

  sensorIzq.setAddress(adressIzq);
  sensorIzq.startContinuous(0);
}

sensado actualizarSensado(){
  sensado lecturaAct = {0,0,0};
  if((sensorIzq.readReg(VL53L0X::RESULT_INTERRUPT_STATUS) & 0x07) != 0){
    lecturaAct.distanciaIzq = sensorIzq.readRangeContinuousMillimeters();
  }
  if((sensorCent.readReg(VL53L0X::RESULT_INTERRUPT_STATUS) & 0x07) != 0){
    lecturaAct.distanciaCent = sensorCent.readRangeContinuousMillimeters();
  }
  if((sensorDer.readReg(VL53L0X::RESULT_INTERRUPT_STATUS) & 0x07) != 0){
    lecturaAct.distanciaDer = sensorDer.readRangeContinuousMillimeters();
  }
  return lecturaAct;
} 