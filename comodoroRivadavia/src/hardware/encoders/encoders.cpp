#include "encoders.h"
#include <Arduino.h>
#include <ESP32Encoder.h>
#include "config.h"

ESP32Encoder encoderA;
ESP32Encoder encoderB;
static int32_t anteriorA = 0, anteriorB = 0; // [VELOCIDAD] Cuentas en la última llamada a verAvanceEncoders()

void inicializarEncoders() {    
  ESP32Encoder::useInternalWeakPullResistors = puType::up;
  encoderA.attachFullQuad(ENC_A_1, ENC_B_1);
  encoderB.attachFullQuad(ENC_A_2, ENC_B_2);
  
  encoderA.clearCount();
  encoderB.clearCount();
}

int32_t verPulsosEncoderA() {
    return encoderA.getCount();
}

int32_t verPulsosEncoderB() {
    return encoderB.getCount();
}
void resetearEncoders() {
    anteriorA -= encoderA.getCount(); // [VELOCIDAD] Corre la referencia con el reset: la medición de velocidad no pierde pulsos
    anteriorB -= encoderB.getCount();
    encoderA.clearCount();
    encoderB.clearCount();
}

int32_t verAvanceEncoders() {
    int32_t cuentaA = encoderA.getCount();
    int32_t cuentaB = encoderB.getCount();
    int32_t avance = (abs(cuentaA - anteriorA) + abs(cuentaB - anteriorB)) / 2;
    anteriorA = cuentaA;
    anteriorB = cuentaB;
    return avance;
}
