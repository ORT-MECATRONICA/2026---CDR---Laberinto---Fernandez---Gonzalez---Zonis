// HEADER DE LOS SENSORES DE DISTANCIA. 
// TODAS LAS VARIABLES SON PRIVADAS SALVO UN STRUCT CON EL QUE SE CREARÁ LA VARIABLE DE POSICIÓN EN EL MAIN. 
// NO SE TIENE QUE EXPORTAR EL OBJETO NI NADA, SOLAMENTE EL STRUCT DEFINIDO EN LAS PRIMERAS LINEAS. 

#pragma once
//https://github.com/pololu/vl53l0x-arduino

#include <Arduino.h>
#include <VL53L0X.h> //Biblioteca de Pololu para el sensor de distancia VL53L0X


//Struct para la medición de sensado
 
struct sensado {
    int16_t distanciaCent;
    int16_t distanciaDer;
    int16_t distanciaIzq;
    // [DEBOUNCE] true solo en la llamada en que se completó una lectura nueva de LOS 3 sensores.
    // Sirve para contar lecturas reales (~33 ms c/u) y no vueltas del loop.
    bool nueva;
};

//Funciones para iniciar los sensores y medir la distancia.

//Función para inicializar los tres sensores, llamar en el main.
void inicializacionSensoresDist();

//Función para medir la distancia, llamar en el main. Devuelve un struct con las tres distancias medidas.
sensado actualizarSensado();

// [OFFSET DINÁMICO] Recalcula los offsets de los sensores laterales con el robot centrado entre paredes.
void calibrarOffsetsLaterales();

// [DECISION QUIETO] Con el robot QUIETO: mediana de 3 lecturas completas (descarta una lectura falsa suelta).
sensado leerMediana3();
