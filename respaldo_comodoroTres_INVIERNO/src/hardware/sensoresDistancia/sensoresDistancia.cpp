#include "sensoresDistancia.h"
#include <Arduino.h>
#include <Wire.h>
#include <VL53L0X.h>
#include "config.h"
#include "hardware/logger/logger.h"
#include "driver/gpio.h" // Librería nativa de ESP-IDF
//LAS REFERENCIAS DE STM ELECTRONICS SON LAS DE LA API FUENTE
//ESTOY USANDO UNA BIBLOTECA DE POLOLU QUE USA ESA REFERENCIA
//https://github.com/pololu/vl53l0x-arduino/blob/master/README.md


VL53L0X sensorDer, sensorIzq, sensorCent;

// [OFFSET DINÁMICO] Offsets laterales en uso. Arrancan con los de config.h y se recalculan
// en calibrarOffsetsLaterales()
static int16_t offsetIzq = OFSET_IZQ;
static int16_t offsetDer = OFSET_DER;

// [C3] [WATCHDOG SENSORES] Cuándo entregó cada sensor su última lectura válida y si arrancó bien.
// Índices: 0 izq, 1 cent, 2 der (el mismo orden que los bits de sensoresActualizados)
static unsigned long ultimaLectura[3] = {0, 0, 0};
static bool sensorIniciado[3] = {false, false, false};

// [OFFSET DINÁMICO] Con el robot centrado entre dos paredes (~80 mm de cada una), promedia
// 10 lecturas de cada lateral y calcula el offset para que, ya restado, den LECTURA_CENTRADO_LATERAL.
// Si una lectura no tiene sentido (sin pared, timeout), deja el offset anterior de ese lado.
void calibrarOffsetsLaterales() {
  delay(300); // margen para sacar la mano después de soltar el botón

  uint32_t sumaIzq = 0, sumaDer = 0;
  for (int i = 0; i < 10; i++) {
    sumaIzq += sensorIzq.readRangeContinuousMillimeters(); // espera la próxima medición (~20 ms)
    sumaDer += sensorDer.readRangeContinuousMillimeters();
  }
  int16_t promIzq = sumaIzq / 10;
  int16_t promDer = sumaDer / 10;

  // Rango razonable de lectura cruda con pared cerca (descarta "sin pared" y timeouts = 65535)
  if (promIzq > 20 && promIzq < 200) offsetIzq = promIzq - LECTURA_CENTRADO_LATERAL;
  if (promDer > 20 && promDer < 200) offsetDer = promDer - LECTURA_CENTRADO_LATERAL;

  enviarString(">>> OFFSETS LATERALES: IZQ=" + String(offsetIzq) + " (crudo " + String(promIzq) +
               ") DER=" + String(offsetDer) + " (crudo " + String(promDer) + ") <<<");
}

// [C3] Prende UN sensor (su XSHUT ya en LOW), le cambia la dirección y lo pone a medir.
// Devuelve false si no respondió. [C2] Antes de medir le fija el tiempo de medición.
static bool iniciarUnSensor(VL53L0X &sensor, uint8_t pinXshut, uint8_t direccion, const char* nombre) {
  digitalWrite(pinXshut, HIGH);
  delay(10); // esperar boot del sensor

  sensor = VL53L0X(); // [C3] Objeto nuevo: arranca en 0x29 (necesario al REINICIAR los sensores)
  sensor.setTimeout(500);
  if (!sensor.init()) {
    enviarString(String(">>> ERROR: no responde el sensor ") + nombre + " (XSHUT " + String(pinXshut) + ") <<<");
    return false;
  }
  sensor.setAddress(direccion);
  // [C2] [SENSORES 50 HZ] 20 ms por medición en vez de los ~33 ms que deja init()
  if (!sensor.setMeasurementTimingBudget(PRESUPUESTO_SENSOR_US)) {
    enviarString(String(">>> AVISO: el sensor ") + nombre + " no aceptó el tiempo de medición <<<");
  }
  sensor.startContinuous(0);
  return true;
}

 void inicializacionSensoresDist(){
// Dentro del setup(), después de Wire.begin:
Wire.begin(SDA, SCL);

// Forzar Pull-Ups internos a nivel de silicio sin alterar el multiplexor
gpio_set_pull_mode((gpio_num_t)SDA, GPIO_PULLUP_ONLY);
gpio_set_pull_mode((gpio_num_t)SCL, GPIO_PULLUP_ONLY);

Wire.setClock(100000); // Recomendable combinar con la reducción de velocidad/

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

  // 2, 3 y 4. Encender y configurar de a uno: derecho, central, izquierdo
  // [C3] Ahora se anota si cada uno arrancó bien (antes solo avisaba por USB)
  sensorIniciado[2] = iniciarUnSensor(sensorDer, xshutPinDer, adressDer, "DERECHO");
  sensorIniciado[1] = iniciarUnSensor(sensorCent, xshutPinCent, adressCent, "CENTRAL");
  sensorIniciado[0] = iniciarUnSensor(sensorIzq, xshutPinIzq, adressIzq, "IZQUIERDO");

  // [C3] El watchdog cuenta desde ahora
  unsigned long ahora = millis();
  for (uint8_t i = 0; i < 3; i++) ultimaLectura[i] = ahora;
}

// [C3] true si los 3 sensores respondieron al iniciarse
bool sensoresIniciadosOK() {
  return sensorIniciado[0] && sensorIniciado[1] && sensorIniciado[2];
}

// [C3] Bits de los sensores que no entregan una lectura válida hace más de TIEMPO_SENSOR_COLGADO_MS
// (bit0 izq, bit1 cent, bit2 der). 0 = todos andan.
uint8_t sensoresColgados() {
  uint8_t colgados = 0;
  unsigned long ahora = millis();
  for (uint8_t i = 0; i < 3; i++) {
    if (ahora - ultimaLectura[i] > TIEMPO_SENSOR_COLGADO_MS) colgados |= (1 << i);
  }
  return colgados;
}

// [C3] Apaga y vuelve a prender los 3 sensores (como en el setup). Tarda ~100 ms.
// Devuelve true si los 3 respondieron.
bool reiniciarSensores() {
  Wire.end(); // libera el bus por si quedó trabado
  inicializacionSensoresDist();
  return sensoresIniciadosOK();
}

sensado actualizarSensado(){
  // Hacer estática la variable para que guarde la última medición válida
  // Si el loop corre más rápido que los 20 ms del sensor, no resetea la distancia a 0.
  static sensado lecturaAct = {0,0,0};
  // [DEBOUNCE] Qué sensores ya entregaron dato desde la última lectura completa (bit0 izq, bit1 cent, bit2 der)
  static uint8_t sensoresActualizados = 0;
  lecturaAct.nueva = false;

    // [PRECISION] 65535 = timeout o error de I2C (no es una distancia). Antes se recortaba a 2000 y
    // contaba como "no hay pared" -> abertura o frente libre falsos. Ahora se descarta y queda la
    // última lectura buena de ese sensor.
    if((sensorIzq.readReg(VL53L0X::RESULT_INTERRUPT_STATUS) & 0x07) != 0){
      uint16_t rawIzq = sensorIzq.readRangeContinuousMillimeters();
      if (rawIzq != 65535) {
        if (rawIzq > 2000) rawIzq = 2000;
        lecturaAct.distanciaIzq = rawIzq - offsetIzq; // [OFFSET DINÁMICO] antes OFSET_IZQ
        sensoresActualizados |= 0x01; // [DEBOUNCE]
        ultimaLectura[0] = millis();  // [C3]
      }
    }
    if((sensorCent.readReg(VL53L0X::RESULT_INTERRUPT_STATUS) & 0x07) != 0){
      uint16_t rawCent = sensorCent.readRangeContinuousMillimeters();
      if (rawCent != 65535) {
        if (rawCent > 2000) rawCent = 2000;
        lecturaAct.distanciaCent =  rawCent - OFSET_CENT;
        sensoresActualizados |= 0x02; // [DEBOUNCE]
        ultimaLectura[1] = millis();  // [C3]
      }
    }
    if((sensorDer.readReg(VL53L0X::RESULT_INTERRUPT_STATUS) & 0x07) != 0){
      uint16_t rawDer = sensorDer.readRangeContinuousMillimeters();
      if (rawDer != 65535) {
        if (rawDer > 2000) rawDer = 2000;
        lecturaAct.distanciaDer =  rawDer - offsetDer; // [OFFSET DINÁMICO] antes OFSET_DER
        sensoresActualizados |= 0x04; // [DEBOUNCE]
        ultimaLectura[2] = millis();  // [C3]
      }
    }
    // [DEBOUNCE] Los 3 sensores tienen dato nuevo -> es una lectura completa
    if (sensoresActualizados == 0x07) {
      sensoresActualizados = 0;
      lecturaAct.nueva = true;
    }

  return lecturaAct;
}

// [DECISION QUIETO] Mediana de 3 valores
static int16_t mediana3(int16_t a, int16_t b, int16_t c) {
  if (a > b) { int16_t t = a; a = b; b = t; } // ahora a <= b
  if (b > c) b = c;                           // b = min(b, c)
  return (a > b) ? a : b;                     // max(a, min(b, c)) = mediana
}

// [DECISION QUIETO] Para usar con el robot QUIETO: junta 3 lecturas completas nuevas (los 3
// sensores) y devuelve la mediana de cada sensor. La mediana descarta una lectura falsa suelta
// (ej. un 1950 por un error), cosa que un promedio no hace. Tarda ~60 ms (antes ~100 ms).
sensado leerMediana3() {
  sensado l[3];
  uint8_t n = 0;
  uint32_t inicio = millis();
  while (n < 3 && millis() - inicio < 500) { // 500 ms de seguridad por si un sensor no responde
    sensado s = actualizarSensado();
    if (s.nueva) l[n++] = s;
  }
  if (n < 3) return actualizarSensado(); // no llegaron 3 lecturas: usa la última disponible

  sensado r;
  r.distanciaIzq  = mediana3(l[0].distanciaIzq,  l[1].distanciaIzq,  l[2].distanciaIzq);
  r.distanciaCent = mediana3(l[0].distanciaCent, l[1].distanciaCent, l[2].distanciaCent);
  r.distanciaDer  = mediana3(l[0].distanciaDer,  l[1].distanciaDer,  l[2].distanciaDer);
  r.nueva = false;
  return r;
}
