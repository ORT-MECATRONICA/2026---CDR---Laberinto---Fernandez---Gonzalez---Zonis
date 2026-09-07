#include "sensoresDistancia.h"
#include <Arduino.h>
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

  // 1. APAGAR TODOS LOS SENSORES
  // Para usar mÃºltiples sensores en el mismo bus, todos arrancan con la dir 0x29
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
  // Hacer estÃ¡tica la variable para que guarde la Ãºltima mediciÃ³n vÃ¡lida
  // Si el loop corre mÃ¡s rÃ¡pido que los 33ms del sensor, no resetea la distancia a 0.
  static sensado lecturaAct = {0,0,0}; 
  static unsigned long ultimoSensado = 0;

  // Evitamos saturar el bus I2C (El ESP32 es tan rÃ¡pido que ahogaba a los sensores)
  // Limitamos la lectura a cada 20ms (50Hz)
//  if(millis() - ultimoSensado > 20){
    if((sensorIzq.readReg(VL53L0X::RESULT_INTERRUPT_STATUS) & 0x07) != 0){
      uint16_t rawIzq = sensorIzq.readRangeContinuousMillimeters();
      if (rawIzq > 2000) rawIzq = 2000;
      lecturaAct.distanciaIzq = (rawIzq > OFSET_IZQ) ? (rawIzq - OFSET_IZQ) : 0;
    }
    if((sensorCent.readReg(VL53L0X::RESULT_INTERRUPT_STATUS) & 0x07) != 0){
      uint16_t rawCent = sensorCent.readRangeContinuousMillimeters();
      if (rawCent > 2000) rawCent = 2000;
      lecturaAct.distanciaCent = (rawCent > OFSET_CENT) ? (rawCent - OFSET_CENT) : 0;
    }
    if((sensorDer.readReg(VL53L0X::RESULT_INTERRUPT_STATUS) & 0x07) != 0){
      uint16_t rawDer = sensorDer.readRangeContinuousMillimeters();
      if (rawDer > 2000) rawDer = 2000;
      lecturaAct.distanciaDer = (rawDer > OFSET_DER) ? (rawDer - OFSET_DER) : 0;
    }
    ultimoSensado = millis();
  //}
  
  return lecturaAct;
} 
//==============================================================
//                IMPLEMENTACIÓN HC-SR04 CON INTERRUPCIONES
//==============================================================

// Variables volátiles para las ISR (rutinas de servicio de interrupción)
volatile unsigned long inicioPulsoDer = 0;
volatile unsigned long inicioPulsoIzq = 0;
volatile unsigned long inicioPulsoCent = 0;

volatile uint16_t distDerVolatile = 0;
volatile uint16_t distIzqVolatile = 0;
volatile uint16_t distCentVolatile = 0;

// ISR para Sensor Derecho
void IRAM_ATTR isr_echo_der() {
    if (digitalRead(ECHO_DER) == HIGH) {
        inicioPulsoDer = micros();
    } else {
        unsigned long duracion = micros() - inicioPulsoDer;
        distDerVolatile = duracion / 5.8; // Convierte a milímetros (58 para cm)
    }
}

// ISR para Sensor Izquierdo
void IRAM_ATTR isr_echo_izq() {
    if (digitalRead(ECHO_IZQ) == HIGH) {
        inicioPulsoIzq = micros();
    } else {
        unsigned long duracion = micros() - inicioPulsoIzq;
        distIzqVolatile = duracion / 5.8; 
    }
}

// ISR para Sensor Central
void IRAM_ATTR isr_echo_cent() {
    if (digitalRead(ECHO_CENT) == HIGH) {
        inicioPulsoCent = micros();
    } else {
        unsigned long duracion = micros() - inicioPulsoCent;
        distCentVolatile = duracion / 5.8; 
    }
}

void inicializacionSensoresHCSR04() {
    pinMode(TRIG_DER, OUTPUT);
    pinMode(ECHO_DER, INPUT);
    pinMode(TRIG_IZQ, OUTPUT);
    pinMode(ECHO_IZQ, INPUT);
    pinMode(TRIG_CENT, OUTPUT);
    pinMode(ECHO_CENT, INPUT);

    digitalWrite(TRIG_DER, LOW);
    digitalWrite(TRIG_IZQ, LOW);
    digitalWrite(TRIG_CENT, LOW);

    // Adjuntar las interrupciones en modo CHANGE para detectar subidas y bajadas
    attachInterrupt(digitalPinToInterrupt(ECHO_DER), isr_echo_der, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ECHO_IZQ), isr_echo_izq, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ECHO_CENT), isr_echo_cent, CHANGE);
}

sensado actualizarSensadoHCSR04() {
    static unsigned long ultimoTrig = 0;
    static uint8_t sensorTurno = 0;

    // Ejecutar triggers secuenciales cada 15ms para evitar bloqueo y solapamiento acústico
    if (millis() - ultimoTrig >= 15) {
        switch(sensorTurno) {
            case 0:
                digitalWrite(TRIG_DER, HIGH);
                delayMicroseconds(10);
                digitalWrite(TRIG_DER, LOW);
                break;
            case 1:
                digitalWrite(TRIG_CENT, HIGH);
                delayMicroseconds(10);
                digitalWrite(TRIG_CENT, LOW);
                break;
            case 2:
                digitalWrite(TRIG_IZQ, HIGH);
                delayMicroseconds(10);
                digitalWrite(TRIG_IZQ, LOW);
                break;
        }
        
        sensorTurno++;
        if (sensorTurno > 2) sensorTurno = 0;
        
        ultimoTrig = millis();
    }

    sensado lectura;
    lectura.distanciaDer = distDerVolatile;
    lectura.distanciaCent = distCentVolatile;
    lectura.distanciaIzq = distIzqVolatile;

    return lectura;
}
