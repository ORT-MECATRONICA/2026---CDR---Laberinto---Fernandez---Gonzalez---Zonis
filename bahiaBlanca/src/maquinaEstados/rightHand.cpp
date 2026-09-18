#include "rightHand.h"

ESTADOS estadoActual = AVANZANDO;
sensado sensadoActual;
FILTRO_SENSORES filtroSensores = {0, 0, 0};

bool right_hand() {
    switch (estadoActual){
        case AVANZANDO: {
            sensadoActual = actualizarSensado();

            if(sensadoActual.distanciaDer > UMBRAL_PARED_ESTADO_NORMAL){
                filtroSensores.counterDer++;
            } else if (sensadoActual.distanciaCent > UMBRAL_PARED_ESTADO_NORMAL){
                filtroSensores.counterCent++;
            } else if (sensadoActual.distanciaIzq > UMBRAL_PARED_ESTADO_NORMAL){
                filtroSensores.counterIzq++;
            }

            if(filtroSensores.counterDer > 5){
                estadoActual = PREPARANDOME_PARA_GIRAR_IZQ;
            } else if (filtroSensores.counterCent > 5) {
                //ACÁ EMPIEZA LA LÓGICA DEL PID
                long error = calcularCorreccion(sensadoActual);
                constrain(error, -50, 50);
                movimiento(AVANZAR, {
                    .izquierda = VEL_BASE_IZQ + error,
                    .derecha = VEL_BASE_DER - error
                });
                //ACA TERMINA EL PID
            } else if(filtroSensores.counterIzq > 5){
                estadoActual = PREPARANDOME_PARA_GIRAR_IZQ;
            } else {
                //NO ENTIENDO QUE PUEDO HACER ACÁ
            }

            //ACÁ EMPIEZA LA LÓGICA DEL PID
            long error = calcularCorreccion(sensadoActual);
            constrain(error, -50, 50);
            movimiento(AVANZAR, {
                .izquierda = VEL_BASE_IZQ + error,
                .derecha = VEL_BASE_DER - error
            });
            //ACA TERMINA EL PID
        }
    }
}