#include "logger.h"
#include <Arduino.h>
#include <BluetoothSerial.h>

BluetoothSerial SerialBT;



void enviarString(String str){
    SerialBT.println(str);
}

void inicializarLogger(){
    SerialBT.begin("Manati");
    Serial.begin(115200);
}

bool cambioDeCelda(){
    if(SerialBT.available()>0){ return true;} else { return false;}
}

bool hayDatosBT() {
    return SerialBT.available() > 0;
}

void procesarTramaBT(uint16_t &pulsosDer, uint16_t &pulsosIzq) {
    String trama = SerialBT.readStringUntil('\n');
    trama.trim();
    trama.toUpperCase();
    
    bool actualizado = false;
    
    int idxD = trama.indexOf('D');
    if (idxD != -1) {
        int valD = trama.substring(idxD + 1).toInt();
        if (valD > 0) {
            pulsosDer = valD;
            actualizado = true;
        }
    }
    
    int idxI = trama.indexOf('I');
    if (idxI != -1) {
        int valI = trama.substring(idxI + 1).toInt();
        if (valI > 0) {
            pulsosIzq = valI;
            actualizado = true;
        }
    }

    if (idxD == -1 && idxI == -1 && trama.indexOf(',') != -1) {
        int idxComa = trama.indexOf(',');
        int valD = trama.substring(0, idxComa).toInt();
        int valI = trama.substring(idxComa + 1).toInt();
        if (valD > 0) pulsosDer = valD;
        if (valI > 0) pulsosIzq = valI;
        if (valD > 0 || valI > 0) actualizado = true;
    }

    if (actualizado) {
        enviarString(">>> PULSOS ACTUALIZADOS <<<");
        enviarString("DER: " + String(pulsosDer) + " | IZQ: " + String(pulsosIzq));
    }
}
