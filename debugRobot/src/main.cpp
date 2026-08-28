#include "main.h"
#include <Arduino.h>
#include "hardware/logger/logger.h"
#include "hardware/sensoresDistancia/sensoresDistancia.h"
#include "hardware/sensorPiso/sensorPiso.h"
#include "memoria/funcionesMapeo.h"
#include "hardware/movimiento/PID.h"
#include "hardware/movimiento/puenteH.h"
#include "hardware/encoders/encoders.h"
#include "config.h"

//==============================================================
//                CREACIÓN DE VARIABLES GLOBALES
//==============================================================

// Dejamos las originales para no romper dependencias de otros archivos
sensado sensadoActual = {0,0,0};

VELOCIDAD velocidadActual = {0,0};

enum PASOS_TEST {
    ESPERAR_BOTON,
    EJECUTANDO_TEST,
    MOSTRANDO_RESULTADOS
};

PASOS_TEST pasoTest = ESPERAR_BOTON; // Controla la secuencia de la prueba

//==============================================================
//                     VOID SETUP
//==============================================================

void setup (){
  pinMode(BOTON, INPUT);
  inicializarLogger();
  inicializacionSensoresDist(); 
  inicializarMotores(); 
  inicializarEncoders();
  Serial.println("--- TEST DE DESEMPENO DE MOTORES ---");
  Serial.println("Presiona el boton para iniciar la prueba.");
  Serial.println("El robot avanzara recto por 2 segundos a la misma potencia.");
}

// Función auxiliar para esperar el botón limpiamente
void esperarBoton() {
    movimiento(FRENO_F, {0,0});
    // Esperar a que se presione
    while(digitalRead(BOTON) == HIGH) { delay(10); }
    // Esperar a que se suelte
    while(digitalRead(BOTON) == LOW) { delay(10); }
    delay(500); // Tiempo de seguridad
}

//==============================================================
//                       VOID LOOP
//==============================================================

void loop(){
    switch(pasoTest) {
        case ESPERAR_BOTON:
            esperarBoton();
            Serial.println("-> INICIANDO PRUEBA...");
            resetearEncoders();
            pasoTest = EJECUTANDO_TEST;
            break;
            
        case EJECUTANDO_TEST:
            movimiento(AVANZAR, {100, 100}); // Misma potencia en ambos
            delay(2000);
            movimiento(FRENO_F, {0,0});
            pasoTest = MOSTRANDO_RESULTADOS;
            break;

        case MOSTRANDO_RESULTADOS: {
            // Obtenemos los pulsos y aplicamos valor absoluto por las dudas (depende de la polaridad)
            int32_t pulsosA = abs(verPulsosEncoderA());
            int32_t pulsosB = abs(verPulsosEncoderB());
            
            Serial.println("--- RESULTADOS ---");
            Serial.print("Pulsos Encoder A: "); Serial.println(pulsosA);
            Serial.print("Pulsos Encoder B: "); Serial.println(pulsosB);
            
            int32_t diferencia = abs(pulsosA - pulsosB);
            
            if (pulsosA > pulsosB + 15) {
                Serial.print("-> El Motor A giro mas rapido (Diferencia de ");
                Serial.print(diferencia);
                Serial.println(" pulsos). El Motor B es mas debil o tiene friccion.");
            } else if (pulsosB > pulsosA + 15) {
                Serial.print("-> El Motor B giro mas rapido (Diferencia de ");
                Serial.print(diferencia);
                Serial.println(" pulsos). El Motor A es mas debil o tiene friccion.");
            } else {
                Serial.println("-> Ambos motores rinden parejo. Estan balanceados.");
            }
            Serial.println("");
            Serial.println("Presiona el boton para repetir la prueba.");
            
            pasoTest = ESPERAR_BOTON;
            break;
        }
    }
}
