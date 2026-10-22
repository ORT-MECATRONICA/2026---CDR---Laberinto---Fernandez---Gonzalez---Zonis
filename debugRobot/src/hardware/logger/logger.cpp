#include "logger.h"
#include "config.h"
#include "hardware/calibracion/calibracion.h"

#ifdef USAR_BLUETOOTH

#include <BluetoothSerial.h>
BluetoothSerial SerialBT;

void inicializarLogger(){
    SerialBT.begin("Manati");
    Serial.begin(115200);
}

void enviarString(String str){
    SerialBT.println(str);
    Serial.println(str);
}

// [NO UTILIZADA ACTUALMENTE - MANTENIDA POR REQUISITO]
bool cambioDeCelda(){
    if (SerialBT.available() > 0) { return true; } else { return false; }
}

bool hayDatosBT() {
    return SerialBT.available() > 0;
}

void procesarTramaBT() {
    String trama = SerialBT.readStringUntil('\n');
    trama.trim();
    if (trama.length() > 0) {
        String respuesta = procesarComandoCalibracion(trama);
        enviarString(respuesta);
    }
}

#else

// Modo sin Bluetooth: todo se redirige a Serial por USB (ahorro de Flash y RAM)
void inicializarLogger(){
    Serial.begin(115200);
}

void enviarString(String str){
    Serial.println(str);
}

bool cambioDeCelda(){
    return false;
}

bool hayDatosBT() {
    return false;
}

void procesarTramaBT() {
    // Bluetooth no habilitado
}

#endif
