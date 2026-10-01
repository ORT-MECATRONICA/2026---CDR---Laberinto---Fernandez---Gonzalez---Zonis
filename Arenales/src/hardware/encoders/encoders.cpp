#include "encoders.h"
#include <Arduino.h>
#include <ESP32Encoder.h>
#include "config.h"

ESP32Encoder encoderA;
ESP32Encoder encoderB;

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
    encoderA.clearCount();
    encoderB.clearCount();
}
