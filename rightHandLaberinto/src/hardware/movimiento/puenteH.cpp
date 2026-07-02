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

void movimiento (MOVIMIENTOS movimiento, VELOCIDAD velocidad) {
  if (velocidad.derecha == 0){
    velocidad.derecha = 50;
  } else if (velocidad.izquierda == 0){
    velocidad.izquierda = 50; 
  }

  switch (movimiento){
    case AVANZAR: {
      digitalWrite(AIN1, LOW);
      digitalWrite(AIN2, HIGH);
      digitalWrite(BIN1, HIGH);
      digitalWrite(BIN2, LOW);
      ledcWrite(0, velocidad.izquierda);
      ledcWrite(1, velocidad.derecha);
      break;
    }
    case RETROCEDER: {
      digitalWrite(AIN1, HIGH);
      digitalWrite(AIN2, LOW);
      digitalWrite(BIN1, LOW);
      digitalWrite(BIN2, HIGH);
      ledcWrite(0, velocidad.izquierda);
      ledcWrite(1, velocidad.derecha);
      break;
    }
    case GIRAR_DER: {
      digitalWrite(AIN1, HIGH);
      digitalWrite(AIN2, LOW);
      digitalWrite(BIN1, HIGH);
      digitalWrite(BIN2, LOW);
      ledcWrite(0, velocidad.izquierda);
      ledcWrite(1, velocidad.derecha);
      break;
    }
    case GIRAR_IZQ: {
      digitalWrite(AIN1, LOW);
      digitalWrite(AIN2, HIGH);
      digitalWrite(BIN1, LOW);
      digitalWrite(BIN2, HIGH);
      ledcWrite(0, velocidad.izquierda);
      ledcWrite(1, velocidad.derecha);
      break;
    }
    case FRENO_F: {
      digitalWrite(AIN1, LOW);
      digitalWrite(AIN2, LOW);
      digitalWrite(BIN1, LOW);
      digitalWrite(BIN2, LOW);
      ledcWrite(0, 0);
      ledcWrite(1, 0);
      break;
    }
  }
}
