#include "PID.h"
#include "hardware/sensoresDistancia/sensoresDistancia.h"
#include "hardware/calibracion/calibracion.h"
#include "config.h"

// [NO UTILIZADA ACTUALMENTE - MANTENIDA POR REQUISITO]
// KI = 0

// Valores fijados hardcodeados según especificación
static const int16_t TOPE = 30;             // Máx. corrección (con base 40 es 20%)
static const int16_t SETPOINT_PARED = 55;  // mm que lee un sensor cuando está centrado
static const int16_t SALTO_MAX = 20;       // mm: un cambio mayor entre lecturas no es movimiento real
                                           // (borde de una pared o lectura falsa): reinicia el D
static const uint8_t N_HIST = 4;
static int16_t historial[N_HIST];          // error de las últimas lecturas nuevas (0 = la más reciente)
static int8_t  modoAnterior = -1;          // 0 sin paredes, 1 solo izq, 2 solo der, 3 ambas, -1 reiniciar
static float   derivada = 0;               // cambio del error por lectura (mm)

int16_t calcularCorreccion(sensado mediciones) {
    // [PID] Un valor negativo = pared MUY cerca (lectura cruda menor que el offset). Antes se
    // descartaba (> 0) y, si del otro lado no había pared, el robot dejaba de corregir justo
    // cuando estaba por chocar. Ahora cuenta como pared a 0 mm (corrección máxima).
    int16_t izq = mediciones.distanciaIzq < 0 ? 0 : mediciones.distanciaIzq;
    int16_t der = mediciones.distanciaDer < 0 ? 0 : mediciones.distanciaDer;
    // [PID] Umbral propio del PID (UMBRAL_PARED_PID = 110, ver config.h) en vez de 170: cuando una
    // pared empieza o termina la lectura pasa de a poco por 55..170, y esas lecturas a medias
    // hacían que el PID diera un volantazo en cada abertura.
    bool hayIzq = izq < UMBRAL_PARED_PID;
    bool hayDer = der < UMBRAL_PARED_PID;

    int16_t error;
    int8_t modo;
    if (hayIzq && hayDer) {
        error = izq - der;
        modo = 3;
    } else if (hayDer) {
        error = 2 * (SETPOINT_PARED - der);
        modo = 2;
    } else if (hayIzq) {
        error = 2 * (izq - SETPOINT_PARED);
        modo = 1;
    } else {
        modoAnterior = 0;
        return 0;
    }

    if (modo != modoAnterior || abs(error - historial[0]) > SALTO_MAX) {
        // [PID D] Cambió el tipo de pared, hubo un salto o se reinició: el historial arranca con
        // el error actual, así el D empieza en 0 en vez de dar un pico
        for (uint8_t i = 0; i < N_HIST; i++) historial[i] = error;
        derivada = 0;
        modoAnterior = modo;
    } else if (mediciones.nueva) {
        // [PID D] Lectura nueva: se corre el historial y se recalcula la pendiente
        for (uint8_t i = N_HIST - 1; i > 0; i--) historial[i] = historial[i - 1];
        historial[0] = error;
        derivada = (historial[0] - historial[N_HIST - 1]) / (float)(N_HIST - 1);
    }

    // Ganancias calibrables vía Bluetooth / Preferences (KP=valor, KD=valor)
    return constrain((int16_t)(configRobot.kp * error + configRobot.kd * derivada), -TOPE, TOPE);
}

void resetearErrorAnterior() {
    // [PID D] Después de un giro o una frenada el historial ya no sirve: se reinicia sin pico
    modoAnterior = -1;
    derivada = 0;
}