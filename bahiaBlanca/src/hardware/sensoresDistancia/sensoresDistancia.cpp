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

struct FiltroMediana {
    uint16_t buffer[FILTRO_MEDIANA_N];
    uint8_t index;
    uint8_t count;
    uint16_t valorMediana;
};

static FiltroMediana filtroIzq = {{0}, 0, 0, 0};
static FiltroMediana filtroCent = {{0}, 0, 0, 0};
static FiltroMediana filtroDer = {{0}, 0, 0, 0};

static sensado lecturaAct = {0, 0, 0};

static uint16_t actualizarFiltroMediana(FiltroMediana &f, uint16_t nuevoValor) {
    f.buffer[f.index] = nuevoValor;
    f.index = (f.index + 1) % FILTRO_MEDIANA_N;
    if (f.count < FILTRO_MEDIANA_N) {
        f.count++;
    }

    uint16_t temp[FILTRO_MEDIANA_N];
    for (uint8_t i = 0; i < f.count; i++) {
        temp[i] = f.buffer[i];
    }

    // In-place insertion sort
    for (uint8_t i = 1; i < f.count; i++) {
        uint16_t key = temp[i];
        int8_t j = i - 1;
        while (j >= 0 && temp[j] > key) {
            temp[j + 1] = temp[j];
            j--;
        }
        temp[j + 1] = key;
    }

    f.valorMediana = temp[f.count / 2];
    return f.valorMediana;
}

static void prellenarFiltro(FiltroMediana &f, uint16_t valorInicial) {
    for (uint8_t i = 0; i < FILTRO_MEDIANA_N; i++) {
        f.buffer[i] = valorInicial;
    }
    f.index = 0;
    f.count = FILTRO_MEDIANA_N;
    f.valorMediana = valorInicial;
}

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
  delay(DELAY_BOOT_SENSOR_MS); // Dar tiempo para asegurar el apagado


  // 2. Encender y configurar Sensor Derecho
  digitalWrite(xshutPinDer, HIGH);
  delay(DELAY_BOOT_SENSOR_MS); // esperar boot del sensor

  sensorDer.setTimeout(TIMEOUT_SENSOR_MS);
  if (!sensorDer.init()) {
    Serial.printf("ERROR: fallo init sensor en pin %d\n", xshutPinDer);
    while (true) delay(1000);
  }

  sensorDer.setAddress(adressDer);
  sensorDer.startContinuous(0);

  // 3. Encender y configurar Sensor Central

  digitalWrite(xshutPinCent, HIGH);
  delay(DELAY_BOOT_SENSOR_MS); // esperar boot del sensor

  sensorCent.setTimeout(TIMEOUT_SENSOR_MS);
  if (!sensorCent.init()) {
    Serial.printf("ERROR: fallo init sensor en pin %d\n", xshutPinCent);
    while (true) delay(1000);
  }

  sensorCent.setAddress(adressCent);
  sensorCent.startContinuous(0);

  // 4. Encender y configurar Sensor Izquierdo

  digitalWrite(xshutPinIzq, HIGH);
  delay(DELAY_BOOT_SENSOR_MS); // esperar boot del sensor

  sensorIzq.setTimeout(TIMEOUT_SENSOR_MS);
  if (!sensorIzq.init()) {
    Serial.printf("ERROR: fallo init sensor en pin %d\n", xshutPinIzq);
    while (true) delay(1000);
  }

  sensorIzq.setAddress(adressIzq);
  sensorIzq.startContinuous(0);

  // 5. Prellenar filtros de mediana para evitar ceros iniciales
  uint16_t rawInitDer = sensorDer.readRangeContinuousMillimeters();
  if (rawInitDer > DISTANCIA_MAX_VALIDA) rawInitDer = DISTANCIA_MAX_VALIDA;
  prellenarFiltro(filtroDer, rawInitDer);

  uint16_t rawInitCent = sensorCent.readRangeContinuousMillimeters();
  if (rawInitCent > DISTANCIA_MAX_VALIDA) rawInitCent = DISTANCIA_MAX_VALIDA;
  prellenarFiltro(filtroCent, rawInitCent);

  uint16_t rawInitIzq = sensorIzq.readRangeContinuousMillimeters();
  if (rawInitIzq > DISTANCIA_MAX_VALIDA) rawInitIzq = DISTANCIA_MAX_VALIDA;
  prellenarFiltro(filtroIzq, rawInitIzq);

  lecturaAct.distanciaDer = (filtroDer.valorMediana > OFSET_DER) ? (filtroDer.valorMediana - OFSET_DER) : 0;
  lecturaAct.distanciaCent = (filtroCent.valorMediana > OFSET_CENT) ? (filtroCent.valorMediana - OFSET_CENT) : 0;
  lecturaAct.distanciaIzq = (filtroIzq.valorMediana > OFSET_IZQ) ? (filtroIzq.valorMediana - OFSET_IZQ) : 0;
}

sensado actualizarSensado(){
  if((sensorIzq.readReg(VL53L0X::RESULT_INTERRUPT_STATUS) & 0x07) != 0){
    uint16_t rawIzq = sensorIzq.readRangeContinuousMillimeters();
    if (rawIzq > DISTANCIA_MAX_VALIDA) rawIzq = DISTANCIA_MAX_VALIDA;
    uint16_t filtradoIzq = actualizarFiltroMediana(filtroIzq, rawIzq);
    lecturaAct.distanciaIzq = (filtradoIzq > OFSET_IZQ) ? (filtradoIzq - OFSET_IZQ) : 0;
  }
  if((sensorCent.readReg(VL53L0X::RESULT_INTERRUPT_STATUS) & 0x07) != 0){
    uint16_t rawCent = sensorCent.readRangeContinuousMillimeters();
    if (rawCent > DISTANCIA_MAX_VALIDA) rawCent = DISTANCIA_MAX_VALIDA;
    uint16_t filtradoCent = actualizarFiltroMediana(filtroCent, rawCent);
    lecturaAct.distanciaCent = (filtradoCent > OFSET_CENT) ? (filtradoCent - OFSET_CENT) : 0;
  }
  if((sensorDer.readReg(VL53L0X::RESULT_INTERRUPT_STATUS) & 0x07) != 0){
    uint16_t rawDer = sensorDer.readRangeContinuousMillimeters();
    if (rawDer > DISTANCIA_MAX_VALIDA) rawDer = DISTANCIA_MAX_VALIDA;
    uint16_t filtradoDer = actualizarFiltroMediana(filtroDer, rawDer);
    lecturaAct.distanciaDer = (filtradoDer > OFSET_DER) ? (filtradoDer - OFSET_DER) : 0;
  }
  
  return lecturaAct;
} 