//==============================================================
//                          PRIMAVERA
// VERSIÓN DEL CÓDIGO CON CALIBRACIÓN DE PULSOS, VELOCIDADES Y KP
// SE LLAMA BACHIN EN HOMENAJE A "PRIMAVERA PORTEÑA", DE PIAZOLLA.
//
// [ESTABLE] Cambios para que ande estable a 100/255 (buscá cada etiqueta en el código):
//  [E1]  Velocidad 100. OJO: la que usa es la de la Flash -> mandar VD=100 y VI=100 por BT
//  [E2]  PID también en PREGIRO y POSTGIRO (antes iban a ciegas y salían torcidos del giro)
//  [E3]  Con pared al frente llega CON PID, frena por el sensor de enfrente y recién ahí decide
//  [E4]  Al terminar el pregiro la lectura quieta solo puede CANCELAR el giro, no elegir otro lado
//  [E5]  Nada de Bluetooth con los motores andando (un println podía trabar el loop hasta 1 s)
//  [E6]  Debounce con franja dudosa: una lectura a medias del borde no reinicia la cuenta
//  [E7]  Sensores leídos en todas las vueltas + watchdog: si uno deja de contestar, se reinician
//  [E8]  Timeout de maniobras: si un pregiro, giro o postgiro no llega a sus pulsos, se da por terminado y sigue (nunca se traba)
//  [E9]  Botones con antirrebote (un pico de ruido de los motores no para el robot)
//  [E10] POST cuenta desde el centro de la celda del giro (dos giros seguidos)
//  [E11] (calibracion.cpp) Valores de BT fuera de rango
//  [E12] VEL_LENTA cerca de la pared de enfrente, al confirmar una abertura y en el pregiro
//  [E13] (sensoresDistancia.cpp) Filtro del frente: una lectura falsa suelta no frena ni corta
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

sensado sensadoActual = {50, 200, 50};
VELOCIDAD velocidadActual = {0, 0};
MAQUINA_NUEVA estado = LISTO;

// [180 OPUESTO] Último giro realizado (GIRANDO_DER / GIRANDO_IZQ). Ahora se usa para elegir el lado del 180.
// AVANZANDO = todavía no giró.
MAQUINA_NUEVA movimientoAnterior = AVANZANDO;

uint32_t pulsosActuales = 0;
// uint32_t tiempoInicioGiro = 0; // [NO UTILIZADA ACTUALMENTE - MANTENIDA POR REQUISITO]

// Modo de navegación: Right Hand (por defecto) o Left Hand
MODO_SEGUIDOR modoActual = MODO_RIGHT_HAND;

// [E6] [DEBOUNCE] Lecturas nuevas seguidas con el lado prioritario libre
uint8_t lecturasLibres = 0;

// [E5] Lo que vieron los sensores cuando se confirmó la abertura. Se manda recién con el robot frenado.
String textoDeteccion = "";

// [E8] Estado que se está cronometrando y desde cuándo
MAQUINA_NUEVA estadoCronometrado = LISTO;
unsigned long inicioEstado = 0;

//==============================================================
//                    FUNCIONES AUXILIARES
//==============================================================

// [LOG LECTURAS] Arma el texto " I:x C:y D:z" con las 3 distancias, igual que el log del 180
String textoLecturas(sensado s) {
  return " I:" + String(s.distanciaIzq) + " C:" + String(s.distanciaCent) + " D:" + String(s.distanciaDer);
}

// [E9] Botón apretado de verdad: tiene que leer LOW dos veces, separadas 5 ms. Los pines 34 y 35
// no tienen pull-up interno y un pico de ruido de los motores podía parar el robot en plena corrida.
bool botonApretado(uint8_t pin) {
  if (digitalRead(pin) != LOW) return false;
  delay(5);
  return digitalRead(pin) == LOW;
}

// [E2] Avanza con el PID de paredes. Lo usan AVANZANDO, PREGIRO y POSTGIRO. Antes PREGIRO y
// POSTGIRO iban con las dos ruedas iguales: si el giro dejaba el robot torcido o corrido, seguía
// así hasta la pared. El PID ya ignora solo el lado abierto (lectura >= UMBRAL_PARED_PID).
// [E12] Va a VEL_LENTA si "lento" es true (abertura confirmándose o pregiro) o si la pared de
// enfrente está cerca. Si VD y VI son distintos, baja los dos en la misma proporción para no
// perder la compensación entre motores.
void avanzarConPID(bool lento) {
  int16_t baseIzq = configRobot.velBaseIzq;
  int16_t baseDer = configRobot.velBaseDer;
  int16_t mayor = max(baseIzq, baseDer);
  if ((lento || sensadoActual.distanciaCent < DISTANCIA_LENTA) && mayor > VEL_LENTA) {
    baseIzq = (int32_t)baseIzq * VEL_LENTA / mayor;
    baseDer = (int32_t)baseDer * VEL_LENTA / mayor;
  }
  int16_t correccion = calcularCorreccion(sensadoActual);
  velocidadActual.izquierda = constrain(baseIzq + correccion, 0, 255);
  velocidadActual.derecha = constrain(baseDer - correccion, 0, 255);
  movimiento(AVANZAR, {velocidadActual.izquierda, velocidadActual.derecha});
}

// Frena y sostiene el freno: la inercia no se cuenta como pulsos de la maniobra siguiente [FIX 1]
void frenar() {
  movimiento(FRENO_F, {0, 0});
  delay(TIEMPO_FRENO_MS);
}

// Arranca la corrida en el modo elegido
void largar(MODO_SEGUIDOR modo) {
  modoActual = modo;
  // [E1] Muestra la velocidad que va a usar DE VERDAD (la de la Flash, no la de config.h)
  enviarString(String(modo == MODO_RIGHT_HAND ? ">>> LARGADA: MODO RIGHT HAND" : ">>> LARGADA: MODO LEFT HAND") +
               " (VI=" + String(configRobot.velBaseIzq) + " VD=" + String(configRobot.velBaseDer) + ") <<<");
  delay(1000); // 1 segundo de margen para sacar la mano
  resetearEncoders();
  resetearErrorAnterior(); // el PID no arrastra nada de la corrida anterior
  lecturasLibres = 0;
  estado = AVANZANDO;
}

// [DECISION QUIETO] Se llama con el robot FRENADO contra la pared de enfrente (centro de la celda).
// [E3] Es el ÚNICO lugar donde se decide quieto: con pared al frente el robot sabe dónde está,
// porque lo frenó el sensor de enfrente. Vuelve a leer los sensores (mediana de 3 lecturas) y
// decide con las prioridades de siempre. Si la pared de enfrente era una lectura falsa, sigue.
MAQUINA_NUEVA decidirQuieto(String origen) {
  sensado s = leerMediana3();
  // [E7] Si un sensor no contestó, la mediana tiene lecturas viejas: no decide con eso. Vuelve a
  // AVANZANDO, el watchdog reinicia los sensores en la vuelta siguiente y, con lecturas nuevas,
  // AVANZANDO frena otra vez contra la pared y decide bien.
  if (sensorSinRespuesta() != nullptr) return AVANZANDO;
  bool derLibre    = s.distanciaDer  >= UMBRAL_PARED_ESTADO_NORMAL;
  bool izqLibre    = s.distanciaIzq  >= UMBRAL_PARED_ESTADO_NORMAL;
  bool frenteLibre = s.distanciaCent >= UMBRAL_FRENTE;

  // [E3] Primero el frente: si quieto ve el frente libre, la lectura que lo hizo frenar era falsa
  // y el robot NO está en el centro de una celda, así que no gira acá (antes, si además veía libre
  // la derecha, giraba en ese lugar cualquiera). Sigue y, si hay abertura, el PREGIRO la ubica.
  // Con pared al frente el orden de siempre: RIGHT HAND 1° derecha, 2° izquierda, 3° 180;
  // LEFT HAND 1° izquierda, 2° derecha, 3° 180.
  MAQUINA_NUEVA siguiente;
  if (frenteLibre) {
    siguiente = AVANZANDO;
  } else if (modoActual == MODO_RIGHT_HAND) {
    if (derLibre)         siguiente = GIRANDO_DER;
    else if (izqLibre)    siguiente = GIRANDO_IZQ;
    else                  siguiente = GIRANDO_180;
  } else {
    if (izqLibre)         siguiente = GIRANDO_IZQ;
    else if (derLibre)    siguiente = GIRANDO_DER;
    else                  siguiente = GIRANDO_180;
  }

  // [LOG LECTURAS] Lectura con el robot quieto y lo que decidió
  String accion = (siguiente == GIRANDO_DER) ? " -> GIRA DERECHA <<<"
                : (siguiente == GIRANDO_IZQ) ? " -> GIRA IZQUIERDA <<<"
                : (siguiente == GIRANDO_180) ? " -> GIRA 180 <<<"
                : " -> SIGUE DERECHO (era lectura falsa) <<<";
  enviarString(origen + accion + textoLecturas(s));
  return siguiente;
}

// [E4] Fin del pregiro, con el robot ya FRENADO en el centro de la celda. La dirección se decidió
// en movimiento (LECTURAS_CONFIRMACION lecturas libres). La lectura quieta (mediana de 3) solo
// puede CANCELAR el giro si del lado del giro ahora ve pared; nunca elige otro lado. Antes acá se
// llamaba a decidirQuieto, que con el frente a menos de 100 podía girar al otro lado o hacer un
// 180 antes de llegar al centro. Si cancela y hay pared al frente, AVANZANDO frena enseguida y
// decide quieto ahí.
void finDePregiro(bool derecha) {
  uint32_t pulsosFinales = leerPulsos(); // incluye lo que siguió andando mientras frenaba
  sensado s = leerMediana3();
  // [E7] Si un sensor no contestó, no decide con lecturas viejas: se queda en el PREGIRO (que ya
  // hizo sus pulsos), el watchdog reinicia los sensores en la vuelta siguiente y después vuelve
  // a pasar por acá con lecturas nuevas, sin moverse del lugar.
  if (sensorSinRespuesta() != nullptr) return;
  int16_t ladoGiro = derecha ? s.distanciaDer : s.distanciaIzq;
  bool gira = ladoGiro >= UMBRAL_PARED_ESTADO_NORMAL;

  // [E5] [LOG LECTURAS] P = pulsos desde la primera lectura libre (al confirmar y al frenar).
  // Si el P de "confirmo" ya es mayor que PRED/PREI, el robot llega tarde: el debounce tarda más
  // que el pregiro y PRED/PREI no tienen efecto.
  enviarString(String(derecha ? ">>> FIN PREGIRO DERECHA" : ">>> FIN PREGIRO IZQUIERDA") +
               (gira ? " -> GIRA <<<" : " -> NO GIRA: ahora ve pared <<<") +
               " confirmo" + textoDeteccion + " | freno P:" + String(pulsosFinales) +
               " | quieto" + textoLecturas(s));

  estado = gira ? (derecha ? GIRANDO_DER : GIRANDO_IZQ) : AVANZANDO;
  resetearErrorAnterior();
  resetearEncoders();
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
  // [E7] Una lectura de sensores y de encoders por vuelta, en TODOS los estados. Antes los giros
  // no leían los sensores y el POSTGIRO podía arrancar con lecturas de antes del giro.
  sensadoActual = actualizarSensado();
  pulsosActuales = leerPulsos();

  // [E8] Cuánto hace que está en el estado actual
  if (estado != estadoCronometrado) {
    estadoCronometrado = estado;
    inicioEstado = millis();
  }
  bool tiempoAgotado = millis() - inicioEstado > TIEMPO_MAX_MANIOBRA_MS;

  // [E7] Watchdog: si un sensor deja de contestar mientras el robot se guía por los sensores,
  // frena, reinicia los 3 (como al arrancar) y sigue en el mismo estado. Antes quedaba la última
  // lectura para siempre: con el de enfrente colgado, no veía nunca la pared.
  bool usaSensores = estado == AVANZANDO || estado == PREGIRO_DER || estado == PREGIRO_IZQ || estado == POSTGIRO;
  const char* sensorColgado = usaSensores ? sensorSinRespuesta() : nullptr;
  if (sensorColgado != nullptr) {
    movimiento(FRENO_F, {0, 0});
    enviarString(String(">>> SENSOR ") + sensorColgado + " SIN RESPUESTA: REINICIANDO SENSORES <<<");
    inicializacionSensoresDist(); // si sigue colgado, reintenta en la vuelta siguiente
    resetearErrorAnterior(); // las lecturas viejas no entran al PID
    return; // en la vuelta siguiente sigue en el mismo estado
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

      // Presionar Botón 1 para avisar que se terminó de cargar datos
      if (botonApretado(BOTON1)) { // [E9]
        while(digitalRead(BOTON1) == LOW) { delay(10); } // Antirrebote al soltar
        calibrarOffsetsLaterales(); // [OFFSET DINÁMICO] El robot tiene que estar centrado entre dos paredes
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
      if (botonApretado(BOTON1)) { // [E9]
        while(digitalRead(BOTON1) == LOW) { delay(10); } // Antirrebote al soltar
        largar(MODO_RIGHT_HAND);
        break;
      }

      // Botón 2: Inicia Left Hand
      if (botonApretado(BOTON2)) { // [E9]
        while(digitalRead(BOTON2) == LOW) { delay(10); } // Antirrebote al soltar
        largar(MODO_LEFT_HAND);
        break;
      }
      break;
    }

    case AVANZANDO: {
      // Parar el robot presionando el botón 1 (ÚNICAMENTE CUANDO ESTÁ EN EL ESTADO AVANZANDO)
      if (botonApretado(BOTON1)) { // [E9]
        movimiento(FRENO_F, {0, 0}); // [PRECISION] Frena ANTES de esperar que se suelte el botón
        while(digitalRead(BOTON1) == LOW) { delay(10); } // Antirrebote al soltar
        lecturasLibres = 0; // [DEBOUNCE] Arranca de cero en la próxima largada
        movimientoAnterior = AVANZANDO; // [180 OPUESTO] La próxima largada arranca sin "último giro"
        enviarString(">>> PARADA POR BOTON 1. VOLVIENDO A LISTO <<<");
        estado = LISTO;
        break;
      }

      // [E3] Pared de enfrente en el centro de la celda: frena y decide quieto (derecha, frente,
      // izquierda o 180). Antes, con pared al frente a menos de 100, entraba a un PREGIRO sin PID
      // que podía cortar por pulsos ANTES de llegar al centro, y giraba corrido.
      if (sensadoActual.distanciaCent < DISTANCIA_CORTE_FRENTE) {
        frenar();
        // [E10] P = pulsos desde el último giro, la largada o la última abertura (sirve para medir una celda)
        estado = decidirQuieto(">>> PARED AL FRENTE P:" + String(leerPulsos()));
        resetearErrorAnterior();
        resetearEncoders();
        lecturasLibres = 0;
        break;
      }

      // [E6] [DEBOUNCE] Solo se mira el lado prioritario (RIGHT HAND la derecha, LEFT HAND la
      // izquierda). El otro lado y el 180 se deciden quieto contra la pared de enfrente ([E3]).
      //  - Libre (>= UMBRAL_PARED_ESTADO_NORMAL): suma. La PRIMERA lectura libre pone los encoders
      //    en cero: PRED/PREI se cuentan desde el borde de la pared, no desde que confirma.
      //  - Pared segura (< UMBRAL_PARED_PID): era una lectura falsa, vuelve a cero.
      //  - En el medio: lectura a medias del borde de la pared, no suma ni vuelve a cero. Antes
      //    cualquier lectura distinta reiniciaba la cuenta Y los encoders, y el giro salía corrido.
      if (sensadoActual.nueva) {
        int16_t ladoPrioritario = (modoActual == MODO_RIGHT_HAND) ? sensadoActual.distanciaDer : sensadoActual.distanciaIzq;
        if (ladoPrioritario >= UMBRAL_PARED_ESTADO_NORMAL) {
          lecturasLibres++;
          if (lecturasLibres == 1) resetearEncoders();
        } else if (ladoPrioritario < UMBRAL_PARED_PID) {
          lecturasLibres = 0;
        }
      }

      if (lecturasLibres >= LECTURAS_CONFIRMACION) {
        // [E5] Antes acá se mandaba ">>> GIRANDO DERECHA <<<" con los motores andando. Ahora se
        // guarda y finDePregiro() lo manda con el robot frenado.
        textoDeteccion = " P:" + String(leerPulsos()) + textoLecturas(sensadoActual);
        lecturasLibres = 0;
        estado = (modoActual == MODO_RIGHT_HAND) ? PREGIRO_DER : PREGIRO_IZQ;
        break;
      }

      // [E2] Con PID. [E12] Lento desde la primera lectura libre: confirma y frena siempre igual
      avanzarConPID(lecturasLibres > 0);
      break;
    }

    case PREGIRO_DER: {
      // [E2] Avanza CON PID hasta el centro de la celda: PRED pulsos desde la primera lectura
      // libre, o antes si la pared de enfrente llega al corte. [E12] A VEL_LENTA.
      if (!tiempoAgotado && pulsosActuales < configRobot.pulsosPregiroDer && sensadoActual.distanciaCent >= DISTANCIA_CORTE_FRENTE) {
        avanzarConPID(true);
        break;
      }
      frenar();
      finDePregiro(true); // [E4]
      break;
    }

    case PREGIRO_IZQ: {
      // [E2] Avanza CON PID hasta el centro de la celda: PREI pulsos desde la primera lectura
      // libre, o antes si la pared de enfrente llega al corte. [E12] A VEL_LENTA.
      if (!tiempoAgotado && pulsosActuales < configRobot.pulsosPregiroIzq && sensadoActual.distanciaCent >= DISTANCIA_CORTE_FRENTE) {
        avanzarConPID(true);
        break;
      }
      frenar();
      finDePregiro(false); // [E4]
      break;
    }

    case GIRANDO_DER: {
      bool cortePorPulsosDer = !tiempoAgotado && pulsosActuales < configRobot.pulsosGiro90Der;
      if (cortePorPulsosDer) {
        movimiento(GIRAR_DER, {configRobot.velBaseIzqGiro, configRobot.velBaseDerGiro});
      } else {
        movimiento(FRENO_F, {0,0});
        enviarString(">>> GIRANDO DER <<<"); // [PRECISION] Log después de frenar (antes se mandaba girando)
        delay(TIEMPO_FRENO_MS); // [FIX 1] Sostener el freno: la inercia del giro ya no entra al POSTGIRO
        movimientoAnterior = GIRANDO_DER;
        estado = POSTGIRO;
        resetearErrorAnterior();
        resetearEncoders(); // [E10] Desde acá (centro de la celda) se cuentan POST y el P de los logs
      }
      break;
    }

    case GIRANDO_IZQ: {
      bool cortePorPulsosIzq = !tiempoAgotado && pulsosActuales < configRobot.pulsosGiro90Izq;
      if (cortePorPulsosIzq) {
        movimiento(GIRAR_IZQ, {configRobot.velBaseIzqGiro, configRobot.velBaseDerGiro});
      } else {
        movimiento(FRENO_F, {0,0});
        enviarString(">>> GIRANDO IZQUIERDA <<<"); // [PRECISION] Log después de frenar (antes se mandaba girando)
        delay(TIEMPO_FRENO_MS); // [FIX 1] Sostener el freno: la inercia del giro ya no entra al POSTGIRO
        movimientoAnterior = GIRANDO_IZQ;
        estado = POSTGIRO;
        resetearErrorAnterior();
        resetearEncoders(); // [E10] Desde acá (centro de la celda) se cuentan POST y el P de los logs
      }
      break;
    }

    case POSTGIRO: {
      // [E2] Sale del giro CON PID: si el giro lo dejó torcido o corrido, lo corrige enseguida.
      // [E10] Mientras hace los POST pulsos no busca aberturas: después de un giro a la derecha,
      // la derecha ve el pasillo por el que vino. POST tiene que alcanzar para que los sensores
      // laterales entren en la celda siguiente (ver PULSOS_POSTGIRO en config.h).
      if (!tiempoAgotado && pulsosActuales < configRobot.pulsosPostgiro && sensadoActual.distanciaCent >= DISTANCIA_CORTE_FRENTE) { // [PRECISION] Corte por pared al frente, igual que PREGIRO
        avanzarConPID(false); // [E12] a VD/VI, salvo que la pared de enfrente esté cerca
        break;
      }
      // [E5] Antes mandaba ">>> FIN POSTGIRO <<<" acá, con los motores andando.
      // Pasa a AVANZANDO sin frenar y sin resetear el PID (venía usándolo). [E10] Tampoco se
      // resetean los encoders: el P de PARED AL FRENTE sigue contando desde el centro del giro.
      lecturasLibres = 0;
      estado = AVANZANDO;
      break;
    }

    case GIRANDO_180: {
      // [180 OPUESTO] Gira hacia el lado contrario al último giro: si el último fue a la izquierda,
      // el 180 es a la derecha; si fue a la derecha (o todavía no giró), a la izquierda como antes.
      // movimientoAnterior recién cambia al terminar el giro, así que el lado no cambia a mitad del 180.
      bool giro180Der = (movimientoAnterior == GIRANDO_IZQ);
      uint16_t pulsos180 = giro180Der ? configRobot.pulsosGiro180Der : configRobot.pulsosGiro180Izq;
      if (!tiempoAgotado && pulsosActuales < pulsos180) {
        movimiento(giro180Der ? GIRAR_DER : GIRAR_IZQ, {configRobot.velBaseIzqGiro180, configRobot.velBaseDerGiro180});
      } else {
        movimiento(FRENO_F, {0,0});
        enviarString(giro180Der ? ">>> FIN GIRO 180 DER <<<" : ">>> FIN GIRO 180 IZQ <<<"); // [PRECISION] Log después de frenar
        delay(TIEMPO_FRENO_MS); // [FIX 1] Sostener el freno: la inercia del giro ya no entra al POSTGIRO
        // [180 OPUESTO] Se guarda el lado del 180, así el próximo 180 seguido sale para el otro lado
        movimientoAnterior = giro180Der ? GIRANDO_DER : GIRANDO_IZQ;
        estado = POSTGIRO;
        resetearErrorAnterior();
        resetearEncoders(); // [E10]
      }
      break;
    }

    default: {
      // DECISION no se usa: si por algún error se llega acá, el robot queda frenado en LISTO
      movimiento(FRENO_F, {0, 0});
      estado = LISTO;
      break;
    }
  }
}
