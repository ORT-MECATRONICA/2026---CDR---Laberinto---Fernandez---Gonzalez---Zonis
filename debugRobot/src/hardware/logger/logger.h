//Logger del sistema. Utiliza principalmente bluetooth. Tiene muy poca prioridad

#pragma once
#include <Arduino.h>

        //Función para inicializar el bluetooth, llamar en el main.
        void inicializarLogger();
        //Función para enviar un STRING a través de bluetooth
        void enviarString(String str);
 

        bool cambioDeCelda(); //Función para detectar si se ha producido un cambio de celda, se puede usar para sincronizar el envío de datos con el movimiento del robot. Devuelve true si se ha producido un cambio de celda, false en caso contrario. Llamar en el loop del main.
        
        // Funciones para recepción de trama BT
        bool hayDatosBT();
        void procesarTramaBT(uint16_t &pulsosDer, uint16_t &pulsosIzq);