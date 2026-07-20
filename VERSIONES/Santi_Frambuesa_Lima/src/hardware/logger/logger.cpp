#include "logger.h"
#include <Arduino.h>
#include <BluetoothSerial.h>

BluetoothSerial SerialBT;

void enviarCelda(uint8_t celda){
    SerialBT.write(celda);
}

void enviarString(String str){
    SerialBT.println(str);
}

void inicializarLogger(){
    SerialBT.begin("Magnesio Aceituna");
    Serial.begin(115200);
}

bool cambioDeCelda(){
    if(SerialBT.available()>0){ return true;} else { return false;}
}

