#include "logger.h"
#include <Arduino.h>
#include <BluetoothSerial.h>

BluetoothSerial SerialBT;



void enviarString(String str){
    SerialBT.println(str);
}

void inicializarLogger(){
    SerialBT.begin("Magnesio Aceituna");
    Serial.begin(115200);
}


