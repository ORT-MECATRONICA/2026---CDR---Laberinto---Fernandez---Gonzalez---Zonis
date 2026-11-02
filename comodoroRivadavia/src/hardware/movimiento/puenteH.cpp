#include  "puenteH.h"
#include "Arduino.h"
#include "config.h"
#include "hardware/encoders/encoders.h"

#if CONTROL_VELOCIDAD
// [VELOCIDAD] Estado del control de velocidad
static MOVIMIENTOS modoAnterior = FRENO_F;
static unsigned long ultimoControl = 0;
static float integralVelocidad = 0;
static int16_t ajusteActual = 0;

// [VELOCIDAD] PWM que hay que sumarles a las dos ruedas para que la velocidad promedio sea la que
// darían esos PWM con la batería cargada. Así 150 es siempre la misma velocidad.
static int16_t ajusteVelocidad(MOVIMIENTOS modo, VELOCIDAD objetivo) {
  unsigned long ahora = micros();
  if (modo != modoAnterior) { // Cambió la maniobra: el control arranca de cero
    modoAnterior = modo;
    integralVelocidad = 0;
    ajusteActual = 0;
    ultimoControl = ahora;
    verAvanceEncoders();
  }
  if (modo == FRENO_F || ahora - ultimoControl < PERIODO_CONTROL_MS * 1000UL) return ajusteActual;

  // Velocidad medida en la escala del PWM: 150 = lo que avanza a PWM 150 con la batería cargada
  float medida = verAvanceEncoders() * 10000.0f / (ahora - ultimoControl) * 150 / PULSOS_10MS_PWM150;
  ultimoControl = ahora;
  float error = (objetivo.izquierda + objetivo.derecha) / 2.0f - medida;
  if (fabs(error) < 30) { // Solo acumula cerca de la velocidad pedida (no mientras acelera o frena)
    integralVelocidad = constrain(integralVelocidad + KI_VELOCIDAD * error, -AJUSTE_MAX_VELOCIDAD, AJUSTE_MAX_VELOCIDAD);
  }
  ajusteActual = constrain(KP_VELOCIDAD * error + integralVelocidad, -AJUSTE_MAX_VELOCIDAD, AJUSTE_MAX_VELOCIDAD);
  return ajusteActual;
}
#endif

void inicializarMotores(){
    pinMode(AIN1, OUTPUT);
    pinMode(AIN2, OUTPUT);
    pinMode(BIN1, OUTPUT);
    pinMode(BIN2, OUTPUT);
    
  ledcSetup(0, 20000, 8);
  ledcSetup(1, 20000, 8);
  ledcAttachPin(PWMA, 0);
  ledcAttachPin(PWMB, 1);
}

void movimiento (MOVIMIENTOS movimiento, VELOCIDAD velocidad) {
  #if CONTROL_VELOCIDAD
  int16_t ajuste = ajusteVelocidad(movimiento, velocidad); // [VELOCIDAD] Lo mismo para las dos ruedas
  velocidad.izquierda = constrain(velocidad.izquierda + ajuste, 0, 255);
  velocidad.derecha = constrain(velocidad.derecha + ajuste, 0, 255);
  #endif
  switch (movimiento){
    case AVANZAR: {
      digitalWrite(AIN1, HIGH);
      digitalWrite(AIN2, LOW);
      digitalWrite(BIN1, HIGH);
      digitalWrite(BIN2, LOW);
      ledcWrite(0, velocidad.izquierda);
      ledcWrite(1, velocidad.derecha);
      break;
    }
    
    case RETROCEDER: {
      digitalWrite(AIN1, LOW);
      digitalWrite(AIN2, HIGH);
      digitalWrite(BIN1, LOW);
      digitalWrite(BIN2, HIGH);
      ledcWrite(0, velocidad.izquierda);
      ledcWrite(1, velocidad.derecha);
      break;
    }
    case GIRAR_DER: {
      digitalWrite(AIN1, LOW);
      digitalWrite(AIN2, HIGH);
      digitalWrite(BIN1, HIGH);
      digitalWrite(BIN2, LOW);
      ledcWrite(0, velocidad.izquierda);
      ledcWrite(1, velocidad.derecha);
      break;
    }
    case GIRAR_IZQ: {
      digitalWrite(AIN1, HIGH);
      digitalWrite(AIN2, LOW);
      digitalWrite(BIN1, LOW);
      digitalWrite(BIN2, HIGH);
      ledcWrite(0, velocidad.izquierda);
      ledcWrite(1, velocidad.derecha);
      break;
    }
    case FRENO_F: {
      digitalWrite(AIN1, HIGH);
      digitalWrite(AIN2, HIGH);
      digitalWrite(BIN1, HIGH);
      digitalWrite(BIN2, HIGH);
      ledcWrite(0, 200);
      ledcWrite(1, 200);
      break;
    }
  }
}

void frenar() {
  movimiento(FRENO_F, {0, 0});
  unsigned long inicio = millis();
  verAvanceEncoders();
  do { delay(20); } while (verAvanceEncoders() > 0 && millis() - inicio < TIEMPO_FRENO_MS);
}
