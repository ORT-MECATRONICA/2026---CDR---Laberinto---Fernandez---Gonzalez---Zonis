#include  "puenteH.h"
#include "Arduino.h"
#include "config.h"
#include "hardware/encoders/encoders.h"

void inicializarMotores(){
    pinMode(AIN1, OUTPUT);
    pinMode(AIN2, OUTPUT);
    pinMode(BIN1, OUTPUT);
    pinMode(BIN2, OUTPUT);
    
  ledcSetup(0, 50000, 8);
  ledcSetup(1, 50000, 8);
  ledcAttachPin(PWMA, 0);
  ledcAttachPin(PWMB, 1);
}

void movimiento (MOVIMIENTOS movimiento, VELOCIDAD velocidad) {
  switch (movimiento){
    case AVANZAR: {
      digitalWrite(AIN1, HIGH);
      digitalWrite(AIN2, LOW);
      digitalWrite(BIN1, LOW);
      digitalWrite(BIN2, HIGH); 
      ledcWrite(0, velocidad.izquierda);
      ledcWrite(1, velocidad.derecha);
      break;
    }
    
    case RETROCEDER: {
      digitalWrite(AIN1, LOW);
      digitalWrite(AIN2, HIGH);
      digitalWrite(BIN1, HIGH);
      digitalWrite(BIN2, LOW);
      ledcWrite(0, velocidad.izquierda);
      ledcWrite(1, velocidad.derecha);
      break;
    }
    case GIRAR_DER: {
      digitalWrite(AIN1, LOW);
      digitalWrite(AIN2, HIGH);
      digitalWrite(BIN1, LOW);
      digitalWrite(BIN2, HIGH);
      ledcWrite(0, velocidad.izquierda);
      ledcWrite(1, velocidad.derecha);
      break;
    }
    case GIRAR_IZQ: {
      digitalWrite(AIN1, HIGH);
      digitalWrite(AIN2, LOW);
      digitalWrite(BIN1, HIGH);
      digitalWrite(BIN2, LOW);
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

void girar90GradosBloqueante(MOVIMIENTOS direccion) {
    VELOCIDAD velGiro = {VEL_GIRO_IZQ, VEL_GIRO_DER}; 
    movimiento(direccion, velGiro);
    delay(TIEMPO_90_GRADOS);
    movimiento(FRENO_F, {0,0});
}

void avanzarBloqueante(){
    movimiento(AVANZAR, {VEL_BASE_IZQ, VEL_BASE_DER});
    delay(TIEMPO_AVANZAR_BLOQUEANTE);
    movimiento(FRENO_F, {0,0});
}
