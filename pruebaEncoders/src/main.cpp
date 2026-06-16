#include <Arduino.h>
#include <BluetoothSerial.h>

// Definición de pines para el Motor A
#define PWMA 32  // Pin de PWM para Motor A
#define AIN1 33  // Dirección 1 para Motor A
#define AIN2 25  // Dirección 2 para Motor A

// Definición de pines para el Motor B
#define PWMB 14   // Pin de PWM para Motor B
#define BIN1 27  // Dirección 1 para Motor B
#define BIN2 26  // Dirección 2 para Motor B



void setup() {
  Serial.begin(115200);
  
  // Configurar pines de dirección y standby como salida
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);
  /*
  // Configurar PWM para ESP32
  ledcSetup(PWM_CHANNEL_A, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(PWMA, PWM_CHANNEL_A);

  ledcSetup(PWM_CHANNEL_B, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(PWMB, PWM_CHANNEL_B); */
} 

void loop() {
  digitalWrite(AIN1, HIGH);
  
}