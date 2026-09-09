//Logger del sistema. Utiliza principalmente bluetooth. Tiene muy poca prioridad

#pragma once
#include <Arduino.h>

        //Función para enviar un CHAR a través de bluetooth
        void enviarCelda(uint8_t celda);
        //Función para inicializar el bluetooth, llamar en el main.
        void inicializarLogger();
        //Función para enviar un STRING a través de bluetooth
        void enviarLog(char* mensaje); 

        void enviarString(String str);

        uint8_t leerAccion(); 
        bool cambioDeCelda(); //Función para detectar si se ha producido un cambio de celda, se puede usar para sincronizar el envío de datos con el movimiento del robot. Devuelve true si se ha producido un cambio de celda, false en caso contrario. Llamar en el loop del main.