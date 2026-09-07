#include "main.h"
#include "hardware/movimiento/puenteH.h"
#include "hardware/logger/logger.h"
#include <Arduino.h>
#include <BluetoothSerial.h>

extern BluetoothSerial SerialBT;

void setup() {
    inicializarLogger();
    inicializarMotores();
    SerialBT.println("Esperando comandos en formato: *PWMA;PWMB;ACCION#");
}

void loop() {
    if (SerialBT.available() > 0) {
        if (SerialBT.read() == '*') {
            String trama = SerialBT.readStringUntil('#');
            
            int idx1 = trama.indexOf(';');
            int idx2 = trama.indexOf(';', idx1 + 1);
            
            if (idx1 != -1 && idx2 != -1) {
                int pwmA = trama.substring(0, idx1).toInt();
                int pwmB = trama.substring(idx1 + 1, idx2).toInt();
                int accion = trama.substring(idx2 + 1).toInt();
                
                VELOCIDAD vel = {(int16_t)pwmA, (int16_t)pwmB};
                
                Serial.printf("Recibido -> PWMA: %d, PWMB: %d, ACCION: %d\n", pwmA, pwmB, accion);
                
                if (accion == 1) {
                    Serial.println("Ejecutando: AVANZAR");
                    movimiento(AVANZAR, vel);
                    delay(5000);
                    movimiento(FRENO_F, vel);
                } else if (accion == 2) {
                    Serial.println("Ejecutando: RETROCEDER");
                    movimiento(RETROCEDER, vel);
                    delay(5000);
                    movimiento(FRENO_F, vel);
                } 
                else if (accion == 3) {
                    Serial.println("Ejecutando: GIRAR_DER");
                    movimiento(GIRAR_DER, vel);
                    delay(5000);
                    movimiento(FRENO_F, vel);
                } else if (accion == 4) {
                    Serial.println("Ejecutando: GIRAR_IZQ");
                    movimiento(GIRAR_IZQ, vel);
                    delay(5000);
                    movimiento(FRENO_F, vel);
                } else {
                    Serial.println("Accion desconocida. Deteniendo.");
                    movimiento(FRENO_F, vel);
                }
            }
        }
    }
}
