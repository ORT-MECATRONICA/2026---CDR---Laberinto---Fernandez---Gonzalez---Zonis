#include "rightHand.h"
#include <stdlib.h>

static SUBESTADOS subestadoActual = AVANZANDO;
static FILTRO_DEBOUNCE filtroDebounce = {0, 0, 0, 0};

static inline void resetDebounce(FILTRO_DEBOUNCE &f) {
    f.apertDer = 0;
    f.apertIzq = 0;
    f.paredFrente = 0;
    f.callejon = 0;
}

ESTADOS right_hand() {
    switch (subestadoActual) {
        case AVANZANDO: {

            sensado sensadoActual = actualizarSensado();
            //Forma de escribir los ifs en monolinea
            bool hayParedDer   = (sensadoActual.distanciaDer <= UMBRAL_PARED_ESTADO_NORMAL);
            bool hayParedCent  = (sensadoActual.distanciaCent <= UMBRAL_PARED_FRENTE);
            bool hayParedIzq   = (sensadoActual.distanciaIzq <= UMBRAL_PARED_ESTADO_NORMAL);

            bool condApertDer    = !hayParedDer;
            bool condCallejon    = (hayParedDer && hayParedCent && hayParedIzq);
            bool condParedFrente = hayParedCent;
            bool condApertIzq    = !hayParedIzq;

            // Contrato de debounce obligatorio: Si es verdadero incrementa, si es falso resetea a 0 inmediatamente
            if (condApertDer) {
                filtroDebounce.apertDer++;
            } else {
                filtroDebounce.apertDer = 0;
            }

            if (condCallejon) {
                filtroDebounce.callejon++;
            } else {
                filtroDebounce.callejon = 0;
            }

            if (condParedFrente) {
                filtroDebounce.paredFrente++;
            } else {
                filtroDebounce.paredFrente = 0;
            }

            if (condApertIzq) {
                filtroDebounce.apertIzq++;
            } else {
                filtroDebounce.apertIzq = 0;
            }

            //JERARQUÍAS (SE MODIFICA EN LEFT HAND)
            if (filtroDebounce.apertDer >= DEBOUNCE_LECTURAS) {
                // Prioridad 1: Doblar a la derecha si hay apertura derecha
                resetDebounce(filtroDebounce);
                resetearEncoders();
                subestadoActual = PREPARANDOME_PARA_GIRAR_DER;
            } else if (filtroDebounce.callejon >= DEBOUNCE_LECTURAS) {
                // Prioridad 2: Callejón sin salida detectado simultáneamente en las 3 paredes -> Giro de 180°
                resetDebounce(filtroDebounce);
                resetearEncoders();
                resetearErrorAnterior();
                subestadoActual = GIRANDO_180;
            } else if (filtroDebounce.paredFrente >= DEBOUNCE_LECTURAS && (filtroDebounce.apertIzq >= DEBOUNCE_LECTURAS || condApertIzq)) {
                // Prioridad 3: Frente bloqueado y apertura a la izquierda -> Doblar a la izquierda
                resetDebounce(filtroDebounce);
                resetearEncoders();
                subestadoActual = PREPARANDOME_PARA_GIRAR_IZQ;
            } else {
                // Prioridad 4: Seguir avanzando recto con seguimiento de paredes mediante PID
                int16_t correccion = calcularCorreccion(sensadoActual);
                movimiento(AVANZAR, {
                    .izquierda = (int16_t)(VEL_BASE_IZQ + correccion),
                    .derecha = (int16_t)(VEL_BASE_DER - correccion)
                });
            }
            break;
        }

        case PREPARANDOME_PARA_GIRAR_DER: {
             Serial.println("PREPARANDOME PARA GIRAR DER");
            movimiento(AVANZAR, {VEL_BASE_IZQ, VEL_BASE_DER});
            if (abs((long)verPulsosEncoderA()) >= PULSOS_AVANCE_PREGIRO) {
                movimiento(FRENO_F, {0, 0});
                resetearEncoders();
                subestadoActual = GIRANDO_DER;
            }
            break;
        }

        case GIRANDO_DER: {
             Serial.println("GIRANDO DER");
            movimiento(GIRAR_DER, {VEL_GIRO_IZQ, VEL_GIRO_DER});
            if (abs((long)verPulsosEncoderA()) >= PULSOS_90_GRADOS) {
                movimiento(FRENO_F, {0, 0});
                resetearEncoders();
                resetearErrorAnterior();
                subestadoActual = POST_GIRO_AVANZAR;
            }
            break;
        }

        case PREPARANDOME_PARA_GIRAR_IZQ: {
             Serial.println("PREPARANDOME PARA GIRAR IZQ");
            movimiento(AVANZAR, {VEL_BASE_IZQ, VEL_BASE_DER});
            if (abs((long)verPulsosEncoderA()) >= PULSOS_AVANCE_PREGIRO_IZQ) {
                movimiento(FRENO_F, {0, 0});
                resetearEncoders();
                subestadoActual = GIRANDO_IZQ;
            }
            break;
        }

        case GIRANDO_IZQ: {
            Serial.println("GIRANDO IZQ");
            movimiento(GIRAR_IZQ, {VEL_GIRO_IZQ, VEL_GIRO_DER});
            if (abs((long)verPulsosEncoderA()) >= PULSOS_90_GRADOS) {
                movimiento(FRENO_F, {0, 0});
                resetearEncoders();
                resetearErrorAnterior();
                subestadoActual = POST_GIRO_AVANZAR;
            }
            break;
        }

        case GIRANDO_180: {
             Serial.println("GIRANDO 180");
            movimiento(GIRAR_DER, {VEL_GIRO_IZQ, VEL_GIRO_DER});
            if (abs((long)verPulsosEncoderA()) >= PULSOS_180_GRADOS) {
                movimiento(FRENO_F, {0, 0});
                resetearEncoders();
                resetearErrorAnterior();
                subestadoActual = AVANZANDO;
            }
            break;
        }

        case POST_GIRO_AVANZAR: {
             Serial.println("POST GIRO");
            movimiento(AVANZAR, {VEL_BASE_IZQ, VEL_BASE_DER});
            if (abs((long)verPulsosEncoderA()) >= PULSOS_AVANZAR_POST_GIRO) {
                resetearEncoders();
                resetearErrorAnterior();
                resetDebounce(filtroDebounce);
                subestadoActual = AVANZANDO;
            }
            break;
        }

        case FIN: {
            movimiento(FRENO_F, {0, 0});
            return HUB;
        }

        default: {
            subestadoActual = AVANZANDO;
            break;
        }
    }
    return RIGHT_HAND;
}
    