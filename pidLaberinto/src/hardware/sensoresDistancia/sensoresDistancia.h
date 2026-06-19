// HEADER DE LOS SENSORES DE DISTANCIA. 
// TODAS LAS VARIABLES SON PRIVADAS SALVO UN STRUCT CON EL QUE SE CREARÁ LA VARIABLE DE POSICIÓN EN EL MAIN. 
// NO SE TIENE QUE EXPORTAR EL OBJETO NI NADA, SOLAMENTE EL STRUCT DEFINIDO EN LAS PRIMERAS LINEAS. 

#pragma once
//https://github.com/pololu/vl53l0x-arduino

#include <Arduino.h>
#include <VL53L0X.h> //Biblioteca de Pololu para el sensor de distancia VL53L0X

//Creación de objetos que solamente se van a poder modificar aca
VL53L0X sensorFront, sensorDer, sensorIzq;

//Struct para la medición de sensado
 
struct sensado {
    uint16_t distanciaCent;
    uint16_t distanciaDer;
    uint16_t distanciaIzq;
};

//Funciones para iniciar los sensores y medir la distancia.

//Función para inicializar los tres sensores, llamar en el main.
void inicializacionSensoresDist();

//Función para medir la distancia, llamar en el main. Devuelve un struct con las tres distancias medidas.
sensado actualizarSensado();