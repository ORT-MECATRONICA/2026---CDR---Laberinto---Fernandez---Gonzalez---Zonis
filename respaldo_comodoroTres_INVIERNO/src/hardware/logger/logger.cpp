#include "logger.h"
#include "config.h"
#include "hardware/calibracion/calibracion.h"

// [C1] [LOGS EN MOVIMIENTO] Mensajes guardados mientras el robot anda. Si se llena, el último
// lugar avisa que se perdieron mensajes (nunca bloquea).
static String logsGuardados[MAX_LOGS_GUARDADOS];
static uint8_t cantidadLogs = 0;
static uint16_t logsPerdidos = 0;

void guardarLog(String str) {
    if (cantidadLogs < MAX_LOGS_GUARDADOS) {
        logsGuardados[cantidadLogs++] = "[" + String(millis()) + "] " + str;
    } else {
        logsPerdidos++;
    }
}

void enviarLogsGuardados() {
    for (uint8_t i = 0; i < cantidadLogs; i++) {
        enviarString(logsGuardados[i]);
        logsGuardados[i] = ""; // libera la memoria del String
    }
    cantidadLogs = 0;
    if (logsPerdidos > 0) {
        enviarString(">>> (" + String(logsPerdidos) + " mensajes perdidos: cola llena) <<<");
        logsPerdidos = 0;
    }
}

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
