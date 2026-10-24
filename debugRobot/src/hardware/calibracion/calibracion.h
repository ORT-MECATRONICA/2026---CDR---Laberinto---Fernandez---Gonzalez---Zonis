#pragma once
#include <Arduino.h>

struct ParametrosRobot {
    uint16_t pulsosPregiroDer;
    uint16_t pulsosPregiroIzq;
    uint16_t pulsosPostgiro;
    uint16_t pulsosGiro90Der;
    uint16_t pulsosGiro90Izq;
    uint16_t pulsosGiro180Izq;
    uint16_t pulsosGiro180Der;
    uint8_t  velBaseDer;
    uint8_t  velBaseIzq;
    uint8_t  velBaseDerGiro;
    uint8_t  velBaseIzqGiro;
    uint8_t  velBaseDerGiro180;
    uint8_t  velBaseIzqGiro180;
    float    kp;
    float    kd;   // [PID D] Ganancia derivativa (clave BT: KD)
};

extern ParametrosRobot configRobot;

// Carga las constantes desde Preferences (o defaults de config.h si no existen)
void inicializarCalibracion();

// Procesa una trama recibida por Bluetooth/Serial, actualiza memoria y guarda en Flash
String procesarComandoCalibracion(String comando);

// Retorna un resumen de todos los parámetros actuales
String obtenerReporteCalibracion();

// Restaura los valores de fábrica definidos en config.h
void restablecerDefaults();
