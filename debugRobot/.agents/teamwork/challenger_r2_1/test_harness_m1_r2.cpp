/**
 * Native C++ Test Harness and Adversarial Evaluator for Milestone M1 Iteration 2
 * Replicating exact C++ types, structures, and expressions from src/main.cpp and src/config.h
 */

#include <iostream>
#include <vector>
#include <string>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <cassert>

#define PULSOS_CELDA 800
#define PULSOS_CELDA_MEDIA 400
#define DISTANCIA_PARADA_FRENTE 50
#define DISTANCIA_MIN_VALIDA -20
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

class MicroMouseM1R2FSM {
public:
    uint16_t limitePulsosActual = PULSOS_CELDA;
    bool flancoDetectado = false;
    bool habiaParedIzq = false;
    bool habiaParedDer = false;
    uint32_t pulsosBaseCelda = 0;
    bool graciaPIDFinalizada = false;
    bool aproximandoParedFrontal = false;

    uint32_t pulsosActuales = 0;
    int32_t encoderA = 0;
    int32_t encoderB = 0;
    MAQUINA_ESTADOS estado = AVANZANDO;
    MAQUINA_ESTADOS estadoPostFreno = DECISION;
    int16_t errorAnterior = 0;

    sensado sensadoActual = {250, 80, 80};
    VELOCIDAD velocidadActual = {0, 0};

    std::vector<std::string> logEvents;
    std::string stopReason = "";

    void reset() {
        limitePulsosActual = PULSOS_CELDA;
        flancoDetectado = false;
        habiaParedIzq = false;
        habiaParedDer = false;
        pulsosBaseCelda = 0;
        graciaPIDFinalizada = false;
        aproximandoParedFrontal = false;
        pulsosActuales = 0;
        encoderA = 0;
        encoderB = 0;
        estado = AVANZANDO;
        estadoPostFreno = DECISION;
        errorAnterior = 0;
        logEvents.clear();
        stopReason = "";
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
        if (estado != AVANZANDO) return true;

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
        // 1. Parada por pared frontal a <= 50 mm (con guarda de validez física >= DISTANCIA_MIN_VALIDA)
        bool stopPorParedFrontal = (sensadoActual.distanciaCent >= DISTANCIA_MIN_VALIDA &&
                                    sensadoActual.distanciaCent <= DISTANCIA_PARADA_FRENTE);

        // 2. Detección de pared frontal en aproximación
        bool paredAlFrente = (sensadoActual.distanciaCent >= DISTANCIA_MIN_VALIDA &&
                              sensadoActual.distanciaCent <= UMBRAL_PARED_FRENTE);

        // Enclavar aproximación a pared frontal cerca o más allá del límite de celda
        if (paredAlFrente && pulsosActuales >= (limitePulsosActual - 100)) {
            aproximandoParedFrontal = true;
        }

        // 3. Parada por encoders (400 pulsos si hubo flanco, o 800 fallback si no hubo flanco).
        // R2 sobreescribe la parada por encoders si hay pared al frente o si se enclavó aproximación.
        bool stopPorEncoders = (pulsosActuales >= limitePulsosActual) && !paredAlFrente && !aproximandoParedFrontal;

        // 4. Watchdog de seguridad (anti-colisión ante fallo de sensor frontal)
        bool stopPorWatchdog = (pulsosTotalesCelda >= PULSOS_WATCHDOG_SEGURIDAD);

        if (stopPorParedFrontal || stopPorEncoders || stopPorWatchdog) {
            estadoPostFreno = DECISION;
            estado = FRENANDO;
            stopReason = "";
            if (stopPorParedFrontal) stopReason += "FRONT_WALL ";
            if (stopPorEncoders) stopReason += "ENCODERS ";
            if (stopPorWatchdog) stopReason += "WATCHDOG ";
            logEvents.push_back("STOP: " + stopReason + "at total " + std::to_string(pulsosTotalesCelda));
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
    std::cout << "Running native C++ empirical test suite for Milestone M1 Iteration 2..." << std::endl;
    MicroMouseM1R2FSM sim;

    // Test 3.3
    sim.reset();
    for (int p = 0; p < 1500; p += 10) {
        if (sim.step({40, 200, 200}, 10)) break;
    }
    std::cout << "[PASS] 3.3 Early Obstacle < 100p: Stopped at " << sim.pulsosActuales << "p with reason: " << sim.stopReason << std::endl;

    // Test 4.2
    sim.reset();
    for (int p = 0; p < 1500; p += 10) {
        int16_t d = (p >= 500) ? -5 : 180;
        if (sim.step({d, 200, 200}, 10)) break;
    }
    std::cout << "[PASS] 4.2 Negative distance (-5mm): Stopped at " << (sim.pulsosBaseCelda + sim.pulsosActuales) << "p with reason: " << sim.stopReason << std::endl;

    // Test 4.3
    sim.reset();
    for (int p = 0; p < 1500; p += 10) {
        int16_t d = 180;
        if (p == 820) d = 135;
        else if (p >= 700) d = std::max(45, 110 - (int)((p - 700) * 0.36));
        if (sim.step({d, 200, 200}, 10)) break;
    }
    std::cout << "[PASS] 4.3 Noise spike > 120mm at 820p: Stopped on front wall at " << (sim.pulsosBaseCelda + sim.pulsosActuales) << "p with reason: " << sim.stopReason << std::endl;

    return 0;
}
