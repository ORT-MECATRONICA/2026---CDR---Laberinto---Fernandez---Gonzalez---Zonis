//Logger del sistema. Utiliza principalmente bluetooth. Tiene muy poca prioridad

#pragma once
#include <Arduino.h>

void inicializarLogger();
void enviarString(String str);

// [C1] [LOGS EN MOVIMIENTO] SerialBT.println puede trabar el loop hasta 1 s por paquete si el
// enlace Bluetooth se congestiona (cola de 32 paquetes, espera de 1000 ms en BluetoothSerial).
// Con los motores andando eso es manejar a ciegas. Mientras el robot anda, los mensajes se
// GUARDAN con guardarLog() y se mandan con enviarLogsGuardados() cuando está frenado.
void guardarLog(String str);
void enviarLogsGuardados();

// [NO UTILIZADA ACTUALMENTE - MANTENIDA POR REQUISITO]
bool cambioDeCelda();

bool hayDatosBT();
void procesarTramaBT();