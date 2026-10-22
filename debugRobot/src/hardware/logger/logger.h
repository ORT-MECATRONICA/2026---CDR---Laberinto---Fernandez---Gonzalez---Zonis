//Logger del sistema. Utiliza principalmente bluetooth. Tiene muy poca prioridad

#pragma once
#include <Arduino.h>

void inicializarLogger();
void enviarString(String str);

// [NO UTILIZADA ACTUALMENTE - MANTENIDA POR REQUISITO]
bool cambioDeCelda();

bool hayDatosBT();
void procesarTramaBT();