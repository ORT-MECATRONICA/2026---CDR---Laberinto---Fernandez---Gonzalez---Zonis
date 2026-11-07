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

// [E7] Milisegundos que cada sensor lleva sin dar una lectura válida MIENTRAS se lo está leyendo:
// [0] izq, [1] cent, [2] der. Los delay() del main (frenadas, largada) no suman, pero tampoco
// lo ponen en cero: así un sensor colgado se detecta aunque el robot frene a cada rato.
static uint32_t msSinLectura[3] = {0, 0, 0};

// [E13] Filtro del sensor de enfrente. Una lectura que salta más de SALTO_FRENTE respecto de la
// última aceptada no se usa enseguida: se guarda como "candidata" y, si la lectura siguiente queda
// cerca de ella (el salto se repite), recién ahí se acepta. Así una lectura falsa suelta (un 20 en
// medio del pasillo, o un 2000 contra la pared) no frena el robot, no corta un PREGIRO o un
// POSTGIRO por la mitad (y gira corrido) y tampoco lo deja de frenar. Acercándose a una pared las
// lecturas cambian de a poco, así que el frenado no se atrasa. No se reinicia en las pausas: en
// los giros los sensores se siguen leyendo y el filtro queda al día.
static int16_t frenteAceptado = 0;
static int16_t frenteCandidato = 0;
static bool hayCandidato = false;
static bool reiniciarFrente = true; // solo al arrancar o al reiniciar los sensores

// [DECISION QUIETO] Mediana de 3 valores
static int16_t mediana3(int16_t a, int16_t b, int16_t c) {
  if (a > b) { int16_t t = a; a = b; b = t; } // ahora a <= b
  if (b > c) b = c;                           // b = min(b, c)
  return (a > b) ? a : b;                     // max(a, min(b, c)) = mediana
}

// [OFFSET DINÁMICO] Con el robot centrado entre dos paredes (~80 mm de cada una), promedia
// 10 lecturas de cada lateral y calcula el offset para que, ya restado, den LECTURA_CENTRADO_LATERAL.
// Si una lectura no tiene sentido (sin pared, timeout), deja el offset anterior de ese lado.
void calibrarOffsetsLaterales() {
  delay(300); // margen para sacar la mano después de soltar el botón

  uint32_t sumaIzq = 0, sumaDer = 0;
  for (int i = 0; i < 10; i++) {
    sumaIzq += sensorIzq.readRangeContinuousMillimeters(); // espera la próxima medición (~33 ms)
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

// [E7] Prende y configura UN sensor (los que faltan configurar tienen que estar apagados con
// XSHUT, porque todos arrancan en la dirección 0x29). "s = VL53L0X()" deja el objeto como recién
// creado (dirección 0x29): hace falta para poder REINICIAR un sensor que ya tenía su dirección
// nueva. En el arranque no cambia nada.
static bool iniciarSensor(VL53L0X &s, uint8_t pinXshut, uint8_t direccion) {
  s = VL53L0X();
  digitalWrite(pinXshut, HIGH);
  delay(10); // esperar boot del sensor

  // [E7] Antes 500 ms: si un sensor se trababa a mitad de una lectura, el loop podía quedar medio
  // segundo congelado con los motores andando. Una medición tarda ~33 ms, 100 ms sobra.
  s.setTimeout(100);
  bool ok = s.init();
  if (!ok) {
    Serial.printf("ERROR: fallo init sensor en pin %d\n", pinXshut);
  }

  s.setAddress(direccion);
  s.startContinuous(0);
  return ok;
}

// [E7] Ahora devuelve false si algún sensor no contestó, y se puede volver a llamar con el
// robot andando para REINICIAR los tres (apaga todos con XSHUT y los prende de a uno, igual
// que al arrancar). Los offsets laterales calibrados no se pierden.
bool inicializacionSensoresDist(){
// Dentro del setup(), después de Wire.begin:
Wire.begin(SDA, SCL); // Si ya estaba iniciado (reinicio) no hace nada

// Forzar Pull-Ups internos a nivel de silicio sin alterar el multiplexor
gpio_set_pull_mode((gpio_num_t)SDA, GPIO_PULLUP_ONLY);
gpio_set_pull_mode((gpio_num_t)SCL, GPIO_PULLUP_ONLY);

Wire.setClock(100000); // Recomendable combinar con la reducción de velocidad/
// [E7] Antes 50 ms por transacción: con el bus I2C trabado (ruido de motores) cada vuelta del
// loop podía tardar cientos de ms. Una transacción normal a 100 kHz tarda menos de 1 ms.
Wire.setTimeOut(10);

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
  // 3. Encender y configurar Sensor Central
  // 4. Encender y configurar Sensor Izquierdo
  bool ok = true;
  if (!iniciarSensor(sensorDer, xshutPinDer, adressDer)) ok = false;
  if (!iniciarSensor(sensorCent, xshutPinCent, adressCent)) ok = false;
  if (!iniciarSensor(sensorIzq, xshutPinIzq, adressIzq)) ok = false;

  // [E7] El watchdog empieza a contar desde cero
  for (uint8_t i = 0; i < 3; i++) msSinLectura[i] = 0;
  reiniciarFrente = true; // [E13]
  return ok;
}

sensado actualizarSensado(){
  // Hacer estática la variable para que guarde la última medición válida
  // Si el loop corre más rápido que los 33ms del sensor, no resetea la distancia a 0.
  static sensado lecturaAct = {0,0,0};
  static unsigned long ultimoSensado = 0;
  // [DEBOUNCE] Qué sensores ya entregaron dato desde la última lectura completa (bit0 izq, bit1 cent, bit2 der)
  static uint8_t sensoresActualizados = 0;
  lecturaAct.nueva = false;

  // [E7] Tiempo desde la llamada anterior. Si es más de 100 ms hubo un delay() en el main
  // (frenada, largada, calibración): en ese rato nadie leía los sensores, así que no se suma.
  unsigned long ahora = millis();
  uint32_t paso = ahora - ultimoSensado;
  if (paso > 100) paso = 0;
  ultimoSensado = ahora;
  for (uint8_t i = 0; i < 3; i++) msSinLectura[i] += paso; // una lectura válida lo vuelve a 0

  // Evitamos saturar el bus I2C (El ESP32 es tan rápido que ahogaba a los sensores)
  // Limitamos la lectura a cada 20ms (50Hz)
//  if(millis() - ultimoSensado > 20){
    // [PRECISION] 65535 = timeout o error de I2C (no es una distancia). Antes se recortaba a 2000 y
    // contaba como "no hay pared" -> abertura o frente libre falsos. Ahora se descarta y queda la
    // última lectura buena de ese sensor.
    if((sensorIzq.readReg(VL53L0X::RESULT_INTERRUPT_STATUS) & 0x07) != 0){
      uint16_t rawIzq = sensorIzq.readRangeContinuousMillimeters();
      if (rawIzq != 65535) {
        if (rawIzq > 2000) rawIzq = 2000;
        lecturaAct.distanciaIzq = rawIzq - offsetIzq; // [OFFSET DINÁMICO] antes OFSET_IZQ
        sensoresActualizados |= 0x01; // [DEBOUNCE]
        msSinLectura[0] = 0; // [E7]
      }
    }
    if((sensorCent.readReg(VL53L0X::RESULT_INTERRUPT_STATUS) & 0x07) != 0){
      uint16_t rawCent = sensorCent.readRangeContinuousMillimeters();
      if (rawCent != 65535) {
        if (rawCent > 2000) rawCent = 2000;
        // [E13] Filtro de saltos del frente (antes se usaba la última lectura sola)
        int16_t nuevaCent = rawCent - OFSET_CENT;
        if (reiniciarFrente) {
          frenteAceptado = nuevaCent;           // primera lectura: no hay con qué compararla
          hayCandidato = false;
          reiniciarFrente = false;
        } else if (abs(nuevaCent - frenteAceptado) <= SALTO_FRENTE ||
                   (hayCandidato && abs(nuevaCent - frenteCandidato) <= SALTO_FRENTE)) {
          frenteAceptado = nuevaCent;           // cambio chico, o un salto que se repitió: es real
          hayCandidato = false;
        } else {
          frenteCandidato = nuevaCent;          // salto grande: espera a la lectura siguiente
          hayCandidato = true;
        }
        lecturaAct.distanciaCent = frenteAceptado;
        sensoresActualizados |= 0x02; // [DEBOUNCE]
        msSinLectura[1] = 0; // [E7]
      }
    }
    if((sensorDer.readReg(VL53L0X::RESULT_INTERRUPT_STATUS) & 0x07) != 0){
      uint16_t rawDer = sensorDer.readRangeContinuousMillimeters();
      if (rawDer != 65535) {
        if (rawDer > 2000) rawDer = 2000;
        lecturaAct.distanciaDer =  rawDer - offsetDer; // [OFFSET DINÁMICO] antes OFSET_DER
        sensoresActualizados |= 0x04; // [DEBOUNCE]
        msSinLectura[2] = 0; // [E7]
      }
    }
    // [DEBOUNCE] Los 3 sensores tienen dato nuevo -> es una lectura completa
    if (sensoresActualizados == 0x07) {
      sensoresActualizados = 0;
      lecturaAct.nueva = true;
    }
  //}

  return lecturaAct;
}

// [E7] Nombre del primer sensor que lleva más de TIEMPO_SENSOR_COLGADO_MS sin dar una lectura
// válida, o nullptr si andan los tres. Antes, si un sensor dejaba de contestar (ruido de los
// motores en el I2C, falso contacto), quedaba su última lectura para siempre: con el de enfrente
// colgado el robot no veía nunca la pared, y como "nueva" no llegaba más, dejaba de decidir.
const char* sensorSinRespuesta() {
  static const char* nombres[3] = {"IZQUIERDO", "CENTRAL", "DERECHO"};
  for (uint8_t i = 0; i < 3; i++) {
    if (msSinLectura[i] > TIEMPO_SENSOR_COLGADO_MS) return nombres[i];
  }
  return nullptr;
}

// [DECISION QUIETO] Para usar con el robot QUIETO: junta 3 lecturas completas nuevas (los 3
// sensores) y devuelve la mediana de cada sensor. La mediana descarta una lectura falsa suelta
// (ej. un 1950 por un error), cosa que un promedio no hace. Tarda ~100 ms.
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
