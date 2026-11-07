#include "PID.h"
#include "hardware/sensoresDistancia/sensoresDistancia.h"
#include "hardware/calibracion/calibracion.h"
#include "config.h"

// [NO UTILIZADA ACTUALMENTE - MANTENIDA POR REQUISITO]
// KI = 0

// Valores fijados hardcodeados según especificación
// [PID] TOPE ya no es fijo (antes 8: con base 40 es 20%). Ahora es configRobot.topePid % de la
// velocidad del momento: con un 8 fijo, a velocidad alta el PID no alcanzaba a corregir.
static const int16_t SETPOINT_PARED = 55;  // mm que lee un sensor cuando está centrado

// [PID D] Término derivativo. Mide qué tan rápido se acerca a una pared, o sea el ÁNGULO del
// robot: corrige la torcedura (por ejemplo, al salir de un giro) ANTES de llegar a la pared.
// El P solo reacciona cuando el robot ya se corrió de lugar.
// Por qué fallaba el D en las versiones anteriores (historial de git de este archivo):
//  - Se actualizaba por tiempo (cada 30 ms) o en cada vuelta del loop, sin sincronizar con los
//    sensores (~33 ms): el D salía a pulsos (0, pico, 0, pico...) en vez de un valor parejo.
//  - Una versión dividía por dt en segundos: multiplicaba el ruido del sensor por ~50.
//  - Otra versión no devolvía nada cuando no habían pasado 30 ms (corrección basura).
//  - resetearErrorAnterior() ponía errorAnterior = 0: en la primera lectura después de cada
//    giro el D daba un pico enorme justo al salir del giro.
// Ahora: se actualiza SOLO con lecturas nuevas de los 3 sensores, es la pendiente de las
// últimas lecturas (menos ruido), se mantiene entre lecturas y se reinicia SIN pico.
static const int16_t SALTO_MAX = 20;       // mm: un cambio mayor entre lecturas no es movimiento real
                                           // (borde de una pared o lectura falsa): reinicia el D

// [C11] [D NORMALIZADA] Con sensores a 20 ms hay más lecturas por segundo: el cambio del error
// POR LECTURA es más chico y el mismo KD frenaría menos. Por eso la pendiente se mide en una
// ventana de ~100 ms (como antes: 4 lecturas de 33 ms) y se expresa "por cada 33 ms".
// Así el KD que ya tenías calibrado (KD=1) significa lo mismo con 20 ms o con 33 ms.
static const uint8_t N_HIST = (PRESUPUESTO_SENSOR_US <= 25000) ? 6 : 4;
static const float ESCALA_D = 33000.0f / (float)PRESUPUESTO_SENSOR_US / (float)(N_HIST - 1);
static int16_t historial[6];               // error de las últimas lecturas nuevas (0 = la más reciente)
static int8_t  modoAnterior = -1;          // 0 sin paredes, 1 solo izq, 2 solo der, 3 ambas, -1 reiniciar
static float   derivada = 0;               // cambio del error cada 33 ms (mm)

int16_t calcularCorreccion(sensado mediciones, int16_t velocidadBase, bool usarIzq, bool usarDer) {
    // [PID] Un valor negativo = pared MUY cerca (lectura cruda menor que el offset). Antes se
    // descartaba (> 0) y, si del otro lado no había pared, el robot dejaba de corregir justo
    // cuando estaba por chocar. Ahora cuenta como pared a 0 mm (corrección máxima).
    int16_t izq = mediciones.distanciaIzq < 0 ? 0 : mediciones.distanciaIzq;
    int16_t der = mediciones.distanciaDer < 0 ? 0 : mediciones.distanciaDer;
    // [C8] Solo paredes SEGURAS (menos de UPARED, 85): con el umbral viejo de 110 entraban las
    // lecturas dudosas (85..110) de una pared que se está terminando y el robot se tiraba hacia la
    // abertura (el "volantazo"). En un pasillo de 25 cm siempre queda al menos una pared segura.
    // Además el main puede apagar un lado (usarIzq/usarDer): el lado que se está abriendo no se usa.
    bool hayIzq = usarIzq && izq < (int16_t)configRobot.umbralPared;
    bool hayDer = usarDer && der < (int16_t)configRobot.umbralPared;

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
        derivada = (historial[0] - historial[N_HIST - 1]) * ESCALA_D; // [C11]
    }

    // [PID] Tope en % de la velocidad del momento (calibrable por BT con TOPE=valor)
    int16_t tope = (int32_t)velocidadBase * configRobot.topePid / 100;
    if (tope < 1) tope = 1;

    // Ganancias calibrables vía Bluetooth / Preferences (KP=valor, KD=valor)
    return constrain((int16_t)(configRobot.kp * error + configRobot.kd * derivada), -tope, tope);
}

void resetearErrorAnterior() {
    // [PID D] Después de un giro o una frenada el historial ya no sirve: se reinicia sin pico
    modoAnterior = -1;
    derivada = 0;
}
