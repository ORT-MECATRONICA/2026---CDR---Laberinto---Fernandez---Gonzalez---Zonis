#include "encoders.h"
#include <Arduino.h>
#include <ESP32Encoder.h>
#include "config.h"

ESP32Encoder encoderA;
ESP32Encoder encoderB;
volatile uint32_t contador_pulsos = 0;

void IRAM_ATTR isr_boton() {
    contador_pulsos++;
}

void inicializarEncoders() {    
  /*ESP32Encoder::useInternalWeakPullResistors = puType::up;
  encoderA.attachFullQuad(ENC_B_1, ENC_A_1);
  encoderB.attachFullQuad(ENC_A_2, ENC_B_2);
  
  encoderA.clearCount();
  encoderB.clearCount(); */
  pinMode(BOTON1, INPUT);
  attachInterrupt(digitalPinToInterrupt(BOTON1), isr_boton, FALLING);
}
/*
int32_t verPulsosEncoderA() {
    return encoderA.getCount();
}

int32_t verPulsosEncoderB() {
    return encoderB.getCount();
}
*/
int32_t verPulsosEncoderA(){
    return contador_pulsos;
}
void resetearEncoders(){
    contador_pulsos = 0;
}
/*
void resetearEncoders() {
    encoderA.clearCount();
    encoderB.clearCount();
}
*/