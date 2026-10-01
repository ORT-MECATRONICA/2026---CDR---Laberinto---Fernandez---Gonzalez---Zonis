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
