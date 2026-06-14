#include <Arduino.h>

#define AIN1 25
#define AIN2 33
#define BIN1 27
#define BIN2 26
#define PWMA 32
#define PWMB 14

void setup() {
  // put your setup code here, to run once:
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);
  pinMode(PWMA, OUTPUT);
  pinMode(PWMB, OUTPUT);
}

void loop() {
  // put your main code here, to run repeatedly:
  digitalWrite(AIN1, HIGH);
  digitalWrite(AIN2, LOW);
  digitalWrite(BIN1, HIGH);
  digitalWrite(BIN2, LOW);
  digitalWrite(PWMA, HIGH);
  digitalWrite(PWMB, HIGH);
  delay(2000);

  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, HIGH);
  digitalWrite(BIN1, LOW);
  digitalWrite(BIN2, HIGH);
  analogWrite(PWMA, 255);
  analogWrite(PWMB, 255);
  digitalWrite(PWMA, HIGH);
  digitalWrite(PWMB, HIGH);
  delay(2000);

  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, LOW);
  digitalWrite(BIN1, LOW);
  digitalWrite(BIN2, LOW);
  digitalWrite(PWMA, HIGH);
  digitalWrite(PWMB, HIGH);
  delay(2000);
}