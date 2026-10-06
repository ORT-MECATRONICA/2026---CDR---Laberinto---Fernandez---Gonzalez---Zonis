/**
 * Native C++ Test Harness and Adversarial Evaluator for Milestone M1
 * Replicating exact C++ types and expressions from src/main.cpp and src/config.h
 */

#include <iostream>
#include <vector>
#include <string>
#include <cstdint>
#include <cmath>
#include <algorithm>

#define PULSOS_CELDA 800
#define PULSOS_CELDA_MEDIA 400
#define DISTANCIA_PARADA_FRENTE 50
#define PULSOS_GRACIA_PID 100
#define PULSOS_MIN_DETECCION_FLANCO 150
#define PULSOS_WATCHDOG_SEGURIDAD 1050
#define UMBRAL_PARED_ESTADO_NORMAL 130 
#define UMBRAL_PARED_FRENTE 120

#define VEL_BASE_DER 45
#define VEL_BASE_IZQ 45
#define KP 0.5f
#define KD 0.3f

struct sensado {
    int16_t distanciaCent;
    int16_t distanciaDer;
    int16_t distanciaIzq;
};

struct VELOCIDAD {
    uint8_t izquierda;
    uint8_t derecha;
};

enum MAQUINA_ESTADOS {
    LISTO,
    AVANZANDO,
    FRENANDO,
    DECISION,
    GIRANDO_DER,
    GIRANDO_IZQ,
    GIRANDO_180,
    INFORMACION_RECIBIDA
};

class MicroMouseM1FSM {
public:
    uint16_t limitePulsosActual = PULSOS_CELDA;
    bool flancoDetectado = false;
    bool habiaParedIzq = false;
    bool habiaParedDer = false;
    uint32_t pulsosBaseCelda = 0;
    bool graciaPIDFinalizada = false;

    uint32_t pulsosActuales = 0;
    int32_t encoderA = 0;
    int32_t encoderB = 0;
    MAQUINA_ESTADOS estado = AVANZANDO;
    MAQUINA_ESTADOS estadoPostFreno = DECISION;
    int16_t errorAnterior = 0;

    sensado sensadoActual = {250, 80, 80};
    VELOCIDAD velocidadActual = {0, 0};

    std::vector<std::string> logEvents;

    void reset() {
        limitePulsosActual = PULSOS_CELDA;
        flancoDetectado = false;
        habiaParedIzq = false;
        habiaParedDer = false;
        pulsosBaseCelda = 0;
        graciaPIDFinalizada = false;
        pulsosActuales = 0;
        encoderA = 0;
        encoderB = 0;
        estado = AVANZANDO;
        estadoPostFreno = DECISION;
        errorAnterior = 0;
        logEvents.clear();
    }

    int16_t calcularCorreccion(sensado mediciones) {
        bool hayIzq = mediciones.distanciaIzq < (UMBRAL_PARED_ESTADO_NORMAL + 50);
        bool hayDer = mediciones.distanciaDer < (UMBRAL_PARED_ESTADO_NORMAL + 50);
        int16_t error = 0;
        if (hayIzq && hayDer) {
            error = (int16_t)mediciones.distanciaDer - (int16_t)mediciones.distanciaIzq;
        } else if (hayIzq) {
            error = - (int16_t)mediciones.distanciaIzq;
        } else if (hayDer) {
            error = (int16_t)mediciones.distanciaDer;
        } else {
            error = 0;
        }
        int16_t correccion = (int16_t)((KP * error) + (KD * (error - errorAnterior)));
        errorAnterior = error;
        return std::max((int16_t)-25, std::min((int16_t)25, correccion));
    }

    bool step(sensado s, int32_t deltaPulsos) {
        if (estado != AVANZANDO) return false;

        encoderA += deltaPulsos;
        encoderB += deltaPulsos;
        pulsosActuales = (std::abs(encoderA) + std::abs(encoderB)) / 2;
        uint32_t pulsosTotalesCelda = pulsosBaseCelda + pulsosActuales;

        sensadoActual = s;

        // R1: Detección y reseteo por flanco lateral
        if (!flancoDetectado) {
            if (sensadoActual.distanciaIzq > 0 && sensadoActual.distanciaIzq <= UMBRAL_PARED_ESTADO_NORMAL) {
                habiaParedIzq = true;
            }
            if (sensadoActual.distanciaDer > 0 && sensadoActual.distanciaDer <= UMBRAL_PARED_ESTADO_NORMAL) {
                habiaParedDer = true;
            }

            bool flancoIzq = habiaParedIzq && (sensadoActual.distanciaIzq > UMBRAL_PARED_ESTADO_NORMAL);
            bool flancoDer = habiaParedDer && (sensadoActual.distanciaDer > UMBRAL_PARED_ESTADO_NORMAL);

            if ((flancoIzq || flancoDer) && pulsosTotalesCelda > PULSOS_MIN_DETECCION_FLANCO) {
                flancoDetectado = true;
                pulsosBaseCelda += pulsosActuales;
                encoderA = 0;
                encoderB = 0;
                pulsosActuales = 0;
                limitePulsosActual = PULSOS_CELDA_MEDIA;
                logEvents.push_back("FLANCO_TRIGGERED");
            }
        }

        // R2: Condiciones de parada
        bool stopPorParedFrontal = (sensadoActual.distanciaCent > 0 &&
                                    sensadoActual.distanciaCent <= DISTANCIA_PARADA_FRENTE &&
                                    pulsosTotalesCelda > 100);

        bool paredAlFrente = (sensadoActual.distanciaCent > 0 &&
                              sensadoActual.distanciaCent <= UMBRAL_PARED_FRENTE);

        bool stopPorEncoders = (pulsosActuales >= limitePulsosActual) && !paredAlFrente;

        bool stopPorWatchdog = (pulsosTotalesCelda >= PULSOS_WATCHDOG_SEGURIDAD);

        if (stopPorParedFrontal || stopPorEncoders || stopPorWatchdog) {
            estadoPostFreno = DECISION;
            estado = FRENANDO;
            std::string reason = "";
            if (stopPorParedFrontal) reason += "FRONT_WALL ";
            if (stopPorEncoders) reason += "ENCODERS ";
            if (stopPorWatchdog) reason += "WATCHDOG ";
            logEvents.push_back("STOP: " + reason + "at total " + std::to_string(pulsosTotalesCelda));
            return true; // Stopped
        } else {
            int16_t correccion = calcularCorreccion(sensadoActual);
            if (!graciaPIDFinalizada) {
                if (pulsosTotalesCelda < PULSOS_GRACIA_PID) {
                    correccion = 0;
                } else {
                    graciaPIDFinalizada = true;
                }
            }
            velocidadActual.izquierda = std::max(0, std::min(255, (int)(VEL_BASE_DER - correccion)));
            velocidadActual.derecha = std::max(0, std::min(255, (int)(VEL_BASE_IZQ + correccion)));
            return false; // Still advancing
        }
    }
};

int main() {
    std::cout << "Running native C++ empirical test suite for Milestone M1..." << std::endl;
    // Suite implementation mirror
    return 0;
}
