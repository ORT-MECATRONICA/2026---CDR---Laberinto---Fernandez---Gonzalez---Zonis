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
    uint8_t  velSospecha;        // [SOSPECHA] clave BT: VS
    uint8_t  velPregiro;         // [SOSPECHA] clave BT: VP
    uint8_t  pasoAceleracion;    // [RAMPA] clave BT: ACEL
    uint8_t  pasoDesaceleracion; // [RAMPA] clave BT: DESAC
    uint8_t  topePid;            // [PID] % de la velocidad, clave BT: TOPE
    uint16_t umbralPared;        // [DIGITAL] clave BT: UPARED
    uint16_t umbralLibre;        // [DIGITAL] clave BT: ULIBRE
    uint16_t pulsosCelda;        // [C4] pulsos de una celda, clave BT: PCELDA (0 = sin calibrar)
    uint16_t distanciaCorte;     // [C12] mm del frente = centro de celda, clave BT: CORTE
    uint16_t distanciaFrenado;   // [C6] mm antes del corte en que empieza a frenar, clave BT: DFRENO
    uint16_t zonaFrenoGiro;      // [C7] pulsos finales del giro que van bajando a VP, clave BT: ZGIRO
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
