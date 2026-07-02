#include  "puenteH.h"
#include "Arduino.h"
#include "config.h"

void inicializarMotores(){
    pinMode(AIN1, OUTPUT);
    pinMode(AIN2, OUTPUT);
    pinMode(BIN1, OUTPUT);
    pinMode(BIN2, OUTPUT);
    
  ledcSetup(0, 5000, 8);
  ledcSetup(1, 5000, 8);
  ledcAttachPin(PWMA, 0);
  ledcAttachPin(PWMB, 1);
}

void avanzar(VELOCIDAD velocidad)
{
    digitalWrite(AIN1, HIGH);
    digitalWrite(AIN2, LOW);
    digitalWrite(BIN1, LOW);
    digitalWrite(BIN2, HIGH);
    ledcWrite(0, velocidad.izquierda);
    ledcWrite(1, velocidad.derecha);
}
