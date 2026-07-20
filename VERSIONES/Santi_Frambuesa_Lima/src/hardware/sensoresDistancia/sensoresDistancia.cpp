#include "sensoresDistancia.h"
#include <Arduino.h>
#include "hardware/logger/logger.h"
#include <Wire.h>
#include <VL53L0X.h> 
#include "config.h"
#include "hardware/logger/logger.h"
//LAS REFERENCIAS DE STM ELECTRONICS SON LAS DE LA API FUENTE
//ESTOY USANDO UNA BIBLOTECA DE POLOLU QUE USA ESA REFERENCIA
//https://github.com/pololu/vl53l0x-arduino/blob/master/README.md


VL53L0X sensorDer, sensorIzq, sensorCent;

void inicializacionSensoresDist(){
  // Es fundamental inicializar el bus I2C
  Wire.begin();
  Wire.setTimeOut(100); // 100ms timeout para evitar que el ESP32 se congele por ruido de los motores


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
  // Hacer estática la variable para que guarde la última medición válida
  static sensado lecturaAct = {0,0,0}; 

  if((sensorIzq.readReg(VL53L0X::RESULT_INTERRUPT_STATUS) & 0x07) != 0){
    int32_t izq = (int32_t)sensorIzq.readRangeContinuousMillimeters() - OFSET_IZQ;
    if (izq < 0) izq = 0;
    if (izq > 300) izq = 300;
    lecturaAct.distanciaIzq = izq;
  }
  if((sensorCent.readReg(VL53L0X::RESULT_INTERRUPT_STATUS) & 0x07) != 0){
    int32_t cent = (int32_t)sensorCent.readRangeContinuousMillimeters() - OFSET_CENT;
    if (cent < 0) cent = 0;
    if (cent > 300) cent = 300;
    lecturaAct.distanciaCent = cent;
  }
  if((sensorDer.readReg(VL53L0X::RESULT_INTERRUPT_STATUS) & 0x07) != 0){
    int32_t der = (int32_t)sensorDer.readRangeContinuousMillimeters() - OFSET_DER;
    if (der < 0) der = 0;
    if (der > 300) der = 300;
    lecturaAct.distanciaDer = der;
  }

  static unsigned long ultimoPrint = 0;
  if(millis() - ultimoPrint > 100){
    enviarString("I:" + String(lecturaAct.distanciaIzq) + " C:" + String(lecturaAct.distanciaCent) + " D:" + String(lecturaAct.distanciaDer));
    ultimoPrint = millis();
  }
  
  return lecturaAct;
} 

