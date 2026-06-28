#include <Arduino.h>
#include <ESP32Encoder.h>
#include <BluetoothSerial.h>

// Definición de pines para el Puente H
#define AIN1 25
#define AIN2 33
#define BIN1 27
#define BIN2 26
#define PWMA 32
#define PWMB 14

// Definición de pines para los Encoders Magnéticos
#define ENC_A_1 16
#define ENC_B_1 17
#define ENC_A_2 4
#define ENC_B_2 2

// Instanciación de periféricos
ESP32Encoder EncoderA;
ESP32Encoder EncoderB; // Agregado para el segundo motor
BluetoothSerial SerialBT;

// Control de flujo temporal para telemetría
unsigned long tiempoAnterior = 0;
const int INTERVALO_BLUETOOTH = 100; // Transmitir datos cada 100 milisegundos

void setup() {
    // 1. Inicialización de Comunicaciones
    SerialBT.begin("Mecarinto");
    
    // 2. Configuración Lógica de Dirección
    pinMode(AIN1, OUTPUT);
    pinMode(AIN2, OUTPUT);
    pinMode(BIN1, OUTPUT);
    pinMode(BIN2, OUTPUT);
    
  ledcSetup(0, 5000, 8);
  ledcSetup(1, 5000, 8);
  ledcAttachPin(PWMA, 0);
  ledcAttachPin(PWMB, 1);
  ESP32Encoder::useInternalWeakPullResistors = puType::up;
  EncoderA.attachFullQuad(ENC_A_1, ENC_B_1);
  EncoderB.attachFullQuad(ENC_A_2, ENC_B_2);
  
  EncoderA.clearCount();
  EncoderB.clearCount();
}

void loop() {
    // A. Lectura de hardware (No bloqueante)
    long pulsosMotorA = EncoderA.getCount();
    long pulsosMotorB = EncoderB.getCount();
    
    // B. Actuadores Físicos
    digitalWrite(AIN1, HIGH);
    digitalWrite(AIN2, LOW);
    digitalWrite(BIN1, HIGH);
    digitalWrite(BIN2, LOW);
    ledcWrite(0, 200);
    ledcWrite(1, 200);
    
    unsigned long tiempoActual = millis();
    if (tiempoActual - tiempoAnterior >= INTERVALO_BLUETOOTH) {
        tiempoAnterior = tiempoActual;
        
        SerialBT.print("MotorA:");
        SerialBT.print(pulsosMotorA);
        SerialBT.print(",MotorB:");
        SerialBT.println(pulsosMotorB);
    }
}