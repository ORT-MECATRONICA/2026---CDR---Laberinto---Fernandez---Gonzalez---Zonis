#include "encoders.h"
#include <Arduino.h>
#include <ESP32Encoder.h>
#include "config.h"

ESP32Encoder encoderA;
ESP32Encoder encoderB;

void inicializarEncoders() {
    pinMode(AIN1, OUTPUT);
    pinMode(AIN2, OUTPUT);
    pinMode(BIN1, OUTPUT);
    pinMode(BIN2, OUTPUT);
    
  ledcSetup(0, 5000, 8);
  ledcSetup(1, 5000, 8);
  ledcAttachPin(PWMA, 0);
  ledcAttachPin(PWMB, 1);
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