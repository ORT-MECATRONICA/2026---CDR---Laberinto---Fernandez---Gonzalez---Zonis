//==============================================================
//                          VERANO
// VERSIÓN NO BLOQUEANTE PARA ALTA VELOCIDAD: TRES VELOCIDADES (CRUCERO, SOSPECHA Y PREGIRO),
// SENSORES DIGITALES CON FRANJA DUDOSA Y FRENO SOLO CUANDO HAY QUE GIRAR.
// SE LLAMA VERANO EN HOMENAJE A "VERANO PORTEÑO", DE PIAZZOLLA (VIENE DESPUÉS DE PRIMAVERA).
//==============================================================

#include "main.h"
#include <Arduino.h>
#include <Preferences.h>
#include "hardware/logger/logger.h"
#include "hardware/sensoresDistancia/sensoresDistancia.h"
#include "hardware/movimiento/PID.h"
#include "hardware/movimiento/puenteH.h"
#include "hardware/encoders/encoders.h"
#include "hardware/calibracion/calibracion.h"
#include "config.h"

//==============================================================
//                CREACIÓN DE VARIABLES GLOBALES
//==============================================================

sensado sensadoActual = {200, 50, 50}; // [FIX] El orden del struct es cent, der, izq (antes {50, 200, 50})
VELOCIDAD velocidadActual = {0, 0};
MAQUINA_NUEVA estado = LISTO;

// [180 OPUESTO] Último giro realizado (GIRANDO_DER / GIRANDO_IZQ). Se usa para elegir el lado del 180.
// AVANZANDO = todavía no giró.
MAQUINA_NUEVA movimientoAnterior = AVANZANDO;

uint32_t pulsosActuales = 0;

// Modo de navegación: Right Hand (por defecto) o Left Hand
MODO_SEGUIDOR modoActual = MODO_RIGHT_HAND;

// [DEBOUNCE] Último estado pedido por los sensores y cuántas lecturas SEGURAS seguidas lo pidieron
MAQUINA_NUEVA estadoDetectado = AVANZANDO;
uint8_t lecturasIguales = 0;

// [DIGITAL] Último estado SEGURO de cada sensor (true = hay pared). Las lecturas dudosas no lo cambian.
bool paredIzq = true;
bool paredCent = false;
bool paredDer = true;

// [SOSPECHA] true = va a VEL_SOSPECHA. Vuelve a crucero con LECTURAS_CONFIRMACION lecturas tranquilas seguidas
bool enSospecha = false;
uint8_t lecturasTranquilas = 0;

// [RAMPA] Velocidad base del momento (sin la corrección del PID)
VELOCIDAD velocidadRampa = {0, 0};
unsigned long ultimaRampa = 0;

// [VOTOS] Votos de cada sensor durante el pregiro, la aproximación o la decisión
VOTOS votosIzq = {0, 0};
VOTOS votosCent = {0, 0};
VOTOS votosDer = {0, 0};
uint8_t lecturasDecision = 0;

// [NO BLOQUEANTE] Estado FRENANDO (como en CITE): a qué estado pasa cuando paran las ruedas
MAQUINA_NUEVA estadoPostFreno = LISTO;
unsigned long tiempoInicioFreno = 0;
unsigned long ultimoChequeoFreno = 0;
int32_t pulsosChequeoFreno = 0;

// [WATCHDOG] Cuándo entró al estado actual
unsigned long tiempoInicioEstado = 0;

// [NO BLOQUEANTE] Botones
BOTON boton1 = {BOTON1, false, 0};
BOTON boton2 = {BOTON2, false, 0};
bool ignorarSoltarB1 = false;

//==============================================================
//                    FUNCIONES AUXILIARES
//==============================================================

// [LOG LECTURAS] Arma el texto " I:x C:y D:z" con las 3 distancias, igual que el log del 180
String textoLecturas(sensado s) {
  return " I:" + String(s.distanciaIzq) + " C:" + String(s.distanciaCent) + " D:" + String(s.distanciaDer);
}

// [LOG VOTOS] Arma el texto " (P:x L:y)" con los votos de pared y de libre
String textoVotos(VOTOS v) {
  return " (P:" + String(v.pared) + " L:" + String(v.libre) + ")";
}

// Pulsos promedio de las dos ruedas desde el último resetearEncoders()
int32_t leerPulsos() {
  return (abs(verPulsosEncoderA()) + abs(verPulsosEncoderB())) / 2;
}

// [DIGITAL] Pasa una lectura a pared / sin pared. Entre umbralPared y umbralLibre la lectura es
// dudosa: no cambia nada y queda lo último seguro (histéresis).
void actualizarDigital(int16_t lectura, bool &hayParedDigital) {
  if (lectura < configRobot.umbralPared) hayParedDigital = true;
  else if (lectura > configRobot.umbralLibre) hayParedDigital = false;
}

// [NO BLOQUEANTE] Una pasada de lectura por vuelta del loop, en todos los estados. Así los sensores
// nunca arrancan con la caché en cero y los estados digitales siempre están al día.
void leerSensores() {
  sensadoActual = actualizarSensado();
  if (!sensadoActual.nueva) return;
  actualizarDigital(sensadoActual.distanciaIzq, paredIzq);
  actualizarDigital(sensadoActual.distanciaCent, paredCent);
  actualizarDigital(sensadoActual.distanciaDer, paredDer);
}

// [VOTOS] Cuenta una lectura segura. Las dudosas no votan.
void votar(int16_t lectura, VOTOS &votos) {
  if (lectura < configRobot.umbralPared) {
    if (votos.pared < 255) votos.pared++;
  } else if (lectura > configRobot.umbralLibre) {
    if (votos.libre < 255) votos.libre++;
  }
}

// [VOTOS] Gana la mayoría. Si empatan (o no hubo lecturas seguras) se usa el último estado digital.
bool hayPared(VOTOS votos, bool estadoDigital) {
  if (votos.pared > votos.libre) return true;
  if (votos.libre > votos.pared) return false;
  return estadoDigital;
}

void reiniciarVotos() {
  votosIzq = {0, 0};
  votosCent = {0, 0};
  votosDer = {0, 0};
  lecturasDecision = 0;
}

// [NO BLOQUEANTE] Antirrebote por millis(): devuelve SE_APRETO o SE_SOLTO una sola vez por cambio
EVENTO_BOTON leerBoton(BOTON &b) {
  bool apretadoAhora = digitalRead(b.pin) == LOW;
  if (apretadoAhora == b.apretado || millis() - b.ultimoCambio < TIEMPO_ANTIRREBOTE_MS) return SIN_EVENTO;
  b.apretado = apretadoAhora;
  b.ultimoCambio = millis();
  return apretadoAhora ? SE_APRETO : SE_SOLTO;
}

int16_t menor(int16_t a, int16_t b) {
  return a < b ? a : b;
}

// [SOSPECHA] Las tres velocidades. Cada una nunca es más rápida que la anterior: si VS o VP
// quedaron más altas que la velocidad base, se usa la base.
VELOCIDAD velocidadCrucero() {
  VELOCIDAD v;
  v.izquierda = configRobot.velBaseIzq;
  v.derecha = configRobot.velBaseDer;
  return v;
}

VELOCIDAD velocidadSospecha() {
  VELOCIDAD v = velocidadCrucero();
  v.izquierda = menor(configRobot.velSospecha, v.izquierda);
  v.derecha = menor(configRobot.velSospecha, v.derecha);
  return v;
}

VELOCIDAD velocidadPregiro() {
  VELOCIDAD v = velocidadSospecha();
  v.izquierda = menor(configRobot.velPregiro, v.izquierda);
  v.derecha = menor(configRobot.velPregiro, v.derecha);
  return v;
}

// [RAMPA] Acerca una velocidad al objetivo un paso
int16_t acercarVelocidad(int16_t actual, int16_t objetivo) {
  if (actual < objetivo) {
    actual += configRobot.pasoAceleracion;
    return actual > objetivo ? objetivo : actual;
  }
  if (actual > objetivo) {
    actual -= configRobot.pasoDesaceleracion;
    return actual < objetivo ? objetivo : actual;
  }
  return actual;
}

// [RAMPA] Lleva velocidadRampa hacia el objetivo de a pasos, cada PERIODO_RAMPA_MS (no bloquea).
// Desde quieto arranca directo en la velocidad de pregiro: es baja (no patina) y por debajo de
// ella el motor casi no se mueve. Antes pasaba de 0 a la base de golpe.
void actualizarRampa(VELOCIDAD objetivo) {
  VELOCIDAD arranque = velocidadPregiro();
  if (velocidadRampa.izquierda < arranque.izquierda) velocidadRampa.izquierda = arranque.izquierda;
  if (velocidadRampa.derecha < arranque.derecha) velocidadRampa.derecha = arranque.derecha;

  if (millis() - ultimaRampa < PERIODO_RAMPA_MS) return;
  ultimaRampa = millis();
  velocidadRampa.izquierda = acercarVelocidad(velocidadRampa.izquierda, objetivo.izquierda);
  velocidadRampa.derecha = acercarVelocidad(velocidadRampa.derecha, objetivo.derecha);
}

// [NO BLOQUEANTE] Empieza a frenar. FRENANDO sostiene el freno hasta que paran las ruedas y
// después pasa a "siguiente" (antes: FRENO_F + delay(TIEMPO_FRENO_MS))
void iniciarFreno(MAQUINA_NUEVA siguiente) {
  movimiento(FRENO_F, {0, 0});
  velocidadRampa = {0, 0}; // [RAMPA] Después de frenar vuelve a arrancar desde VP
  estadoPostFreno = siguiente;
  tiempoInicioFreno = millis();
  ultimoChequeoFreno = tiempoInicioFreno;
  pulsosChequeoFreno = leerPulsos();
  estado = FRENANDO;
}

// Vuelve a AVANZANDO SIN FRENAR (lectura falsa, fin del postgiro, largada). Vuelve en sospecha:
// sube a crucero solo cuando ve LECTURAS_CONFIRMACION lecturas tranquilas seguidas.
void volverAAvanzar() {
  resetearErrorAnterior();
  estadoDetectado = AVANZANDO;
  lecturasIguales = 0;
  enSospecha = true;
  lecturasTranquilas = 0;
  estado = AVANZANDO;
  tiempoInicioEstado = millis();
}

// Deja todo como al principio para la próxima largada (parada por botón)
void reiniciarCorrida() {
  estadoDetectado = AVANZANDO;
  lecturasIguales = 0;
  movimientoAnterior = AVANZANDO; // [180 OPUESTO] La próxima largada arranca sin "último giro"
  enSospecha = false;
  lecturasTranquilas = 0;
  velocidadRampa = {0, 0};
  reiniciarVotos();
  resetearErrorAnterior();
}

//==============================================================
//                     VOID SETUP
//==============================================================

void setup (){
  pinMode(BOTON1, INPUT);
  pinMode(BOTON2, INPUT);

  inicializarCalibracion(); // Carga parámetros desde Flash (Preferences) o defaults
  inicializarLogger();
  inicializarMotores();
  inicializarEncoders();
  inicializacionSensoresDist();
  resetearEncoders();
}

//==============================================================
//                       VOID LOOP
//==============================================================

void loop(){
  leerSensores(); // [NO BLOQUEANTE] Una lectura por vuelta, en todos los estados

  EVENTO_BOTON eventoB1 = leerBoton(boton1);
  EVENTO_BOTON eventoB2 = leerBoton(boton2);

  // [NO BLOQUEANTE] El soltar del apretón que paró el robot no cuenta como "datos cargados" en LISTO
  if (ignorarSoltarB1 && eventoB1 == SE_SOLTO) {
    ignorarSoltarB1 = false;
    eventoB1 = SIN_EVENTO;
  }

  // Parar el robot apretando el botón 1. [NO BLOQUEANTE] Ahora en cualquier estado de la corrida
  // (antes solo en AVANZANDO, porque el while del antirrebote bloqueaba)
  bool enCorrida = estado != LISTO && estado != INFORMACION_RECIBIDA && estado != ESPERANDO_LARGADA;
  if (enCorrida && eventoB1 == SE_APRETO) {
    movimiento(FRENO_F, {0, 0}); // [PRECISION] Frena apenas se aprieta, sin esperar que se suelte
    reiniciarCorrida();
    ignorarSoltarB1 = true;
    enviarString(">>> PARADA POR BOTON 1. VOLVIENDO A LISTO <<<");
    estado = LISTO;
    return;
  }

  switch (estado) {
    case LISTO: {
      movimiento(FRENO_F, {0,0});

      #ifdef USAR_BLUETOOTH
      if (hayDatosBT()) {
        estado = INFORMACION_RECIBIDA;
        break;
      }
      #endif

      // Soltar el Botón 1 para avisar que se terminó de cargar datos
      if (eventoB1 == SE_SOLTO) {
        calibrarOffsetsLaterales(); // [OFFSET DINÁMICO] El robot tiene que estar centrado entre dos paredes (quieto, tarda ~0,6 s)
        enviarString(">>> DATOS CARGADOS. SELECCIONE: B1 (RIGHT HAND) o B2 (LEFT HAND) <<<");
        estado = ESPERANDO_LARGADA;
      }
      break;
    }

    case INFORMACION_RECIBIDA: {
      #ifdef USAR_BLUETOOTH
      procesarTramaBT();
      #endif
      estado = LISTO;
      break;
    }

    case ESPERANDO_LARGADA: {
      movimiento(FRENO_F, {0,0});
      // En este estado el robot ya NO revisa SerialBT.available

      // Botón 1: Inicia Right Hand
      if (eventoB1 == SE_SOLTO) {
        modoActual = MODO_RIGHT_HAND;
        enviarString(">>> LARGADA: MODO RIGHT HAND <<<");
        estado = LARGADA;
        tiempoInicioEstado = millis();
        break;
      }

      // Botón 2: Inicia Left Hand
      if (eventoB2 == SE_SOLTO) {
        modoActual = MODO_LEFT_HAND;
        enviarString(">>> LARGADA: MODO LEFT HAND <<<");
        estado = LARGADA;
        tiempoInicioEstado = millis();
        break;
      }
      break;
    }

    case LARGADA: {
      movimiento(FRENO_F, {0,0});
      // [NO BLOQUEANTE] 1 segundo de margen para sacar la mano (antes delay(1000)). Mientras tanto los
      // sensores se siguen leyendo arriba del switch: arranca con lecturas reales y no con ceros.
      if (millis() - tiempoInicioEstado >= TIEMPO_LARGADA_MS) {
        resetearEncoders();
        velocidadRampa = {0, 0};
        volverAAvanzar(); // Arranca en sospecha: sube a crucero recién con lecturas tranquilas
      }
      break;
    }

    case AVANZANDO: {
      pulsosActuales = leerPulsos();

      // [FRENTE] Seguridad: si la pared de enfrente ya está en el centro de la celda, frena y decide
      // quieto sin esperar confirmación (por si el sensor del frente la vio tarde)
      if (sensadoActual.distanciaCent < DISTANCIA_CORTE_FRENTE) {
        iniciarFreno(DECISION);
        enviarString(">>> FRENTE MUY CERCA <<<" + textoLecturas(sensadoActual)); // [PRECISION] Log después de frenar
        break;
      }

      // [SOSPECHA] Crucero si no sospecha nada; si sospecha, baja con rampa a VEL_SOSPECHA
      actualizarRampa(enSospecha ? velocidadSospecha() : velocidadCrucero());
      int16_t correccion = calcularCorreccion(sensadoActual, (velocidadRampa.izquierda + velocidadRampa.derecha) / 2);
      velocidadActual.izquierda = constrain(velocidadRampa.izquierda + correccion, 0, 255);
      velocidadActual.derecha = constrain(velocidadRampa.derecha - correccion, 0, 255);
      movimiento(AVANZAR, velocidadActual);

      // [DEBOUNCE] Solo se decide con lecturas nuevas de los 3 sensores (~33 ms), no con vueltas del loop
      if (!sensadoActual.nueva) break;

      // Lado prioritario: RIGHT HAND mira la derecha, LEFT HAND la izquierda
      bool rightHand = modoActual == MODO_RIGHT_HAND;
      int16_t lecturaPrioridad = rightHand ? sensadoActual.distanciaDer : sensadoActual.distanciaIzq;
      bool paredPrioridad = rightHand ? paredDer : paredIzq;

      // [SOSPECHA] Sospecha si la pared del lado prioritario no es segura (dudosa o ausente) o si hay
      // pared al frente a ~1 celda. Bajar la velocidad es barato: alcanza con una sola lectura.
      // Girar es caro: eso sí necesita confirmación (abajo).
      bool sospechaAhora = lecturaPrioridad >= configRobot.umbralPared
                        || sensadoActual.distanciaCent < UMBRAL_FRENTE_SOSPECHA;
      if (sospechaAhora) {
        if (!enSospecha) enviarString(">>> SOSPECHA <<<" + textoLecturas(sensadoActual));
        enSospecha = true;
        lecturasTranquilas = 0;
      } else if (enSospecha) {
        lecturasTranquilas++;
        if (lecturasTranquilas >= LECTURAS_CONFIRMACION) {
          enSospecha = false;
          enviarString(">>> FIN SOSPECHA: CRUCERO <<<");
        }
      }

      // [DEBOUNCE] Estado que piden los sensores digitales en esta lectura.
      // ORDEN DE PRIORIDADES: 1° lado prioritario libre (pregiro), 2° pared al frente (aproximación;
      // al final decide quieto entre el otro lado y el 180). Si el frente está libre, el otro lado
      // no importa: sigue derecho SIN FRENAR.
      MAQUINA_NUEVA detectado = AVANZANDO;
      bool lecturaSegura = false; // La lectura que pide el estado está fuera de la franja dudosa
      if (!paredPrioridad) {
        detectado = rightHand ? PREGIRO_DER : PREGIRO_IZQ;
        lecturaSegura = lecturaPrioridad > configRobot.umbralLibre;
      } else if (paredCent) {
        detectado = APROXIMANDO_FRENTE;
        lecturaSegura = sensadoActual.distanciaCent < configRobot.umbralPared;
      }

      if (detectado == estadoDetectado) {
        // [DIGITAL] Las lecturas dudosas no suman ni reinician: no dicen nada
        if (lecturaSegura && lecturasIguales < 255) lecturasIguales++;
      } else {
        estadoDetectado = detectado;
        lecturasIguales = lecturaSegura ? 1 : 0;
        // Los pulsos del pregiro se cuentan desde la PRIMERA detección (igual que antes),
        // así PRED/PREI no hay que recalibrarlos por la demora de confirmar.
        resetearEncoders();
      }

      if (detectado != AVANZANDO && lecturasIguales >= LECTURAS_CONFIRMACION) {
        reiniciarVotos();
        if (detectado == APROXIMANDO_FRENTE) {
          enviarString(">>> PARED AL FRENTE: APROXIMANDO <<<" + textoLecturas(sensadoActual)); // [LOG LECTURAS]
        } else {
          // [VOTOS] Las lecturas que confirmaron la abertura ya cuentan como votos de "libre"
          if (rightHand) votosDer.libre = lecturasIguales;
          else votosIzq.libre = lecturasIguales;
          enviarString(String(rightHand ? ">>> PREGIRO DERECHA <<<" : ">>> PREGIRO IZQUIERDA <<<") + textoLecturas(sensadoActual)); // [LOG LECTURAS]
        }
        estado = detectado;
        tiempoInicioEstado = millis();
        // [DEBOUNCE] El contador arranca de cero para la próxima decisión
        estadoDetectado = AVANZANDO;
        lecturasIguales = 0;
      }
      break;
    }

    case PREGIRO_DER: {
      pulsosActuales = leerPulsos();
      // [SOSPECHA] En el pregiro baja todavía más: llega lento al centro de la celda y frena corto.
      // Sin PID, igual que antes (el lado del giro está abierto).
      actualizarRampa(velocidadPregiro());
      movimiento(AVANZAR, velocidadRampa);
      // [VOTOS] Mientras avanza sigue mirando la abertura: si la pared vuelve, era una lectura falsa
      if (sensadoActual.nueva) votar(sensadoActual.distanciaDer, votosDer);

      bool finPregiro = pulsosActuales >= configRobot.pulsosPregiroDer
                     || sensadoActual.distanciaCent < DISTANCIA_CORTE_FRENTE;
      bool timeout = millis() - tiempoInicioEstado > TIEMPO_MAX_MANIOBRA_MS; // [WATCHDOG]
      if (finPregiro || timeout) {
        if (hayPared(votosDer, paredDer)) {
          // [NO FRENA] La abertura era falsa: vuelve a AVANZANDO sin frenar (antes frenaba y decidía quieto)
          enviarString(">>> PREGIRO DERECHA CANCELADO: era lectura falsa" + textoVotos(votosDer) + " <<<");
          volverAAvanzar();
        } else {
          iniciarFreno(GIRANDO_DER);
          enviarString(">>> FIN PREGIRO DERECHA -> GIRA" + textoVotos(votosDer) + (timeout ? " TIMEOUT" : "") + " <<<");
        }
      }
      break;
    }

    case PREGIRO_IZQ: {
      pulsosActuales = leerPulsos();
      // [SOSPECHA] En el pregiro baja todavía más: llega lento al centro de la celda y frena corto.
      // Sin PID, igual que antes (el lado del giro está abierto).
      actualizarRampa(velocidadPregiro());
      movimiento(AVANZAR, velocidadRampa);
      // [VOTOS] Mientras avanza sigue mirando la abertura: si la pared vuelve, era una lectura falsa
      if (sensadoActual.nueva) votar(sensadoActual.distanciaIzq, votosIzq);

      bool finPregiro = pulsosActuales >= configRobot.pulsosPregiroIzq
                     || sensadoActual.distanciaCent < DISTANCIA_CORTE_FRENTE;
      bool timeout = millis() - tiempoInicioEstado > TIEMPO_MAX_MANIOBRA_MS; // [WATCHDOG]
      if (finPregiro || timeout) {
        if (hayPared(votosIzq, paredIzq)) {
          // [NO FRENA] La abertura era falsa: vuelve a AVANZANDO sin frenar (antes frenaba y decidía quieto)
          enviarString(">>> PREGIRO IZQUIERDA CANCELADO: era lectura falsa" + textoVotos(votosIzq) + " <<<");
          volverAAvanzar();
        } else {
          iniciarFreno(GIRANDO_IZQ);
          enviarString(">>> FIN PREGIRO IZQUIERDA -> GIRA" + textoVotos(votosIzq) + (timeout ? " TIMEOUT" : "") + " <<<");
        }
      }
      break;
    }

    case APROXIMANDO_FRENTE: {
      pulsosActuales = leerPulsos();
      // [FRENTE] Igual que el pregiro: lento hasta el centro de la celda, sin PID (igual que antes)
      actualizarRampa(velocidadPregiro());
      movimiento(AVANZAR, velocidadRampa);
      if (sensadoActual.nueva) votar(sensadoActual.distanciaCent, votosCent);

      // [NO FRENA] Si el frente aparece libre con lecturas seguras, la pared era falsa: sigue sin frenar
      if (votosCent.libre >= LECTURAS_CONFIRMACION && votosCent.libre > votosCent.pared) {
        enviarString(">>> APROXIMACION CANCELADA: frente libre" + textoVotos(votosCent) + " <<<");
        volverAAvanzar();
        break;
      }

      bool finAproximacion = pulsosActuales >= PULSOS_APROX_FRENTE
                          || sensadoActual.distanciaCent < DISTANCIA_CORTE_FRENTE;
      bool timeout = millis() - tiempoInicioEstado > TIEMPO_MAX_MANIOBRA_MS; // [WATCHDOG]
      if (finAproximacion || timeout) {
        // Con pared al frente hay que parar sí o sí: frena y decide quieto (otro lado o 180)
        iniciarFreno(DECISION);
        enviarString(">>> FIN APROXIMACION <<<" + textoLecturas(sensadoActual)); // [PRECISION] Log después de frenar
      }
      break;
    }

    case DECISION: {
      movimiento(FRENO_F, {0,0});
      // [DECISION QUIETO] Junta LECTURAS_DECISION lecturas nuevas con el robot quieto en el centro de
      // la celda y decide por mayoría (antes leerMediana3, que bloqueaba ~100 ms)
      if (sensadoActual.nueva) {
        votar(sensadoActual.distanciaIzq, votosIzq);
        votar(sensadoActual.distanciaCent, votosCent);
        votar(sensadoActual.distanciaDer, votosDer);
        lecturasDecision++;
      }
      bool timeout = millis() - tiempoInicioEstado > TIEMPO_MAX_DECISION_MS;
      if (lecturasDecision < LECTURAS_DECISION && !timeout) break;

      bool derLibre    = !hayPared(votosDer, paredDer);
      bool izqLibre    = !hayPared(votosIzq, paredIzq);
      bool frenteLibre = !hayPared(votosCent, paredCent);

      MAQUINA_NUEVA siguiente;
      if (modoActual == MODO_RIGHT_HAND) {
        // ORDEN DE PRIORIDADES RIGHT HAND: 1° Derecha, 2° Frente, 3° Izquierda, 4° 180
        if (derLibre)         siguiente = GIRANDO_DER;
        else if (frenteLibre) siguiente = AVANZANDO;
        else if (izqLibre)    siguiente = GIRANDO_IZQ;
        else                  siguiente = GIRANDO_180;
      } else {
        // ORDEN DE PRIORIDADES LEFT HAND: 1° Izquierda, 2° Frente, 3° Derecha, 4° 180
        if (izqLibre)         siguiente = GIRANDO_IZQ;
        else if (frenteLibre) siguiente = AVANZANDO;
        else if (derLibre)    siguiente = GIRANDO_DER;
        else                  siguiente = GIRANDO_180;
      }

      String accion = (siguiente == GIRANDO_DER) ? " -> GIRA DERECHA <<<"
                    : (siguiente == GIRANDO_IZQ) ? " -> GIRA IZQUIERDA <<<"
                    : (siguiente == GIRANDO_180) ? " -> GIRA 180 <<<"
                    : " -> SIGUE DERECHO (era lectura falsa) <<<";
      enviarString(">>> DECISION" + accion + textoLecturas(sensadoActual));

      if (siguiente == AVANZANDO) {
        resetearEncoders();
        volverAAvanzar();
      } else {
        resetearEncoders();
        resetearErrorAnterior();
        estado = siguiente;
        tiempoInicioEstado = millis();
      }
      break;
    }

    case GIRANDO_DER: {
      pulsosActuales = leerPulsos();
      // Giro SOLO por encoders: los sensores no intervienen
      bool timeout = millis() - tiempoInicioEstado > TIEMPO_MAX_MANIOBRA_MS; // [WATCHDOG]
      if (pulsosActuales < configRobot.pulsosGiro90Der && !timeout) {
        movimiento(GIRAR_DER, {configRobot.velBaseIzqGiro, configRobot.velBaseDerGiro});
      } else {
        iniciarFreno(POSTGIRO); // [FIX 1] [NO BLOQUEANTE] La inercia del giro no entra al POSTGIRO
        enviarString(timeout ? ">>> TIMEOUT GIRO DER <<<" : ">>> GIRANDO DER <<<"); // [PRECISION] Log después de frenar
        movimientoAnterior = GIRANDO_DER;
      }
      break;
    }

    case GIRANDO_IZQ: {
      pulsosActuales = leerPulsos();
      // Giro SOLO por encoders: los sensores no intervienen
      bool timeout = millis() - tiempoInicioEstado > TIEMPO_MAX_MANIOBRA_MS; // [WATCHDOG]
      if (pulsosActuales < configRobot.pulsosGiro90Izq && !timeout) {
        movimiento(GIRAR_IZQ, {configRobot.velBaseIzqGiro, configRobot.velBaseDerGiro});
      } else {
        iniciarFreno(POSTGIRO); // [FIX 1] [NO BLOQUEANTE] La inercia del giro no entra al POSTGIRO
        enviarString(timeout ? ">>> TIMEOUT GIRO IZQ <<<" : ">>> GIRANDO IZQUIERDA <<<"); // [PRECISION] Log después de frenar
        movimientoAnterior = GIRANDO_IZQ;
      }
      break;
    }

    case POSTGIRO: {
      pulsosActuales = leerPulsos();
      // [SOSPECHA] Sale del giro a velocidad de sospecha (arranca en VP y sube con rampa). Sin PID,
      // igual que antes: recién está entrando al pasillo nuevo.
      actualizarRampa(velocidadSospecha());
      movimiento(AVANZAR, velocidadRampa);
      bool finPostgiro = pulsosActuales >= configRobot.pulsosPostgiro
                      || sensadoActual.distanciaCent < DISTANCIA_CORTE_FRENTE; // [PRECISION] Corte por pared al frente, igual que PREGIRO
      bool timeout = millis() - tiempoInicioEstado > TIEMPO_MAX_MANIOBRA_MS; // [WATCHDOG]
      if (finPostgiro || timeout) {
        enviarString(">>> FIN POSTGIRO <<<");
        resetearEncoders();
        volverAAvanzar(); // Sin frenar: sigue en sospecha hasta ver lecturas tranquilas
      }
      break;
    }

    case GIRANDO_180: {
      pulsosActuales = leerPulsos();
      // [180 OPUESTO] Gira hacia el lado contrario al último giro: si el último fue a la izquierda,
      // el 180 es a la derecha; si fue a la derecha (o todavía no giró), a la izquierda.
      // movimientoAnterior recién cambia al terminar el giro, así que el lado no cambia a mitad del 180.
      bool giro180Der = (movimientoAnterior == GIRANDO_IZQ);
      uint16_t pulsos180 = giro180Der ? configRobot.pulsosGiro180Der : configRobot.pulsosGiro180Izq;
      bool timeout = millis() - tiempoInicioEstado > TIEMPO_MAX_MANIOBRA_MS; // [WATCHDOG]
      if (pulsosActuales < pulsos180 && !timeout) {
        movimiento(giro180Der ? GIRAR_DER : GIRAR_IZQ, {configRobot.velBaseIzqGiro180, configRobot.velBaseDerGiro180});
      } else {
        iniciarFreno(POSTGIRO); // [FIX 1] [NO BLOQUEANTE] La inercia del giro no entra al POSTGIRO
        enviarString(giro180Der ? ">>> FIN GIRO 180 DER <<<" : ">>> FIN GIRO 180 IZQ <<<"); // [PRECISION] Log después de frenar
        if (timeout) enviarString(">>> TIMEOUT GIRO 180 <<<");
        // [180 OPUESTO] Se guarda el lado del 180, así el próximo 180 seguido sale para el otro lado
        movimientoAnterior = giro180Der ? GIRANDO_DER : GIRANDO_IZQ;
      }
      break;
    }

    case FRENANDO: {
      movimiento(FRENO_F, {0,0}); // Mantener el freno activo
      // [NO BLOQUEANTE] Cada PERIODO_CHEQUEO_FRENO_MS mira si las ruedas siguen girando. Sale apenas
      // paran (o a los TIEMPO_FRENO_MS como máximo): antes eran 450 ms fijos con delay()
      if (millis() - ultimoChequeoFreno < PERIODO_CHEQUEO_FRENO_MS) break;
      ultimoChequeoFreno = millis();
      int32_t pulsos = leerPulsos();
      bool ruedasQuietas = abs(pulsos - pulsosChequeoFreno) <= 1;
      pulsosChequeoFreno = pulsos;

      unsigned long tiempoFrenando = millis() - tiempoInicioFreno;
      if ((ruedasQuietas && tiempoFrenando >= TIEMPO_FRENO_MIN_MS) || tiempoFrenando >= TIEMPO_FRENO_MS) {
        resetearEncoders();
        resetearErrorAnterior();
        reiniciarVotos();
        estado = estadoPostFreno;
        tiempoInicioEstado = millis();
      }
      break;
    }
  }
}
