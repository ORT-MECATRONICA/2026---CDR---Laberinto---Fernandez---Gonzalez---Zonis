//==============================================================
//                          OTOÑO
// VERANO SIMPLIFICADO: TRES VELOCIDADES (CRUCERO, SOSPECHA Y PREGIRO) CON RAMPA, SENSORES
// DIGITALES CON FRANJA DUDOSA Y FRENO SOLO CUANDO HAY QUE GIRAR O HAY PARED AL FRENTE.
// SIN VOTOS Y SIN EL ESTADO APROXIMANDO_FRENTE (AHORA LO HACE AVANZANDO CON PID).
// SE LLAMA OTOÑO EN HOMENAJE A "OTOÑO PORTEÑO", DE PIAZZOLLA (VIENE DESPUÉS DE VERANO).
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

sensado sensadoActual = {200, 50, 50}; // El orden del struct es cent, der, izq
MAQUINA_NUEVA estado = LISTO;

// Modo de navegación: Right Hand (por defecto) o Left Hand
MODO_SEGUIDOR modoActual = MODO_RIGHT_HAND;

// [180 OPUESTO] Último giro realizado (GIRANDO_DER / GIRANDO_IZQ). AVANZANDO = todavía no giró.
MAQUINA_NUEVA movimientoAnterior = AVANZANDO;

// [DIGITAL] Último estado SEGURO de cada sensor (true = hay pared). Las lecturas dudosas no lo cambian.
bool paredIzq = true;
bool paredCent = false;
bool paredDer = true;

// [DEBOUNCE] Lecturas SEGURAS de libre seguidas del lado prioritario (una de pared lo vuelve a cero)
uint8_t lecturasAbertura = 0;

// [RAMPA] Velocidad base del momento (sin la corrección del PID)
VELOCIDAD velocidadRampa = {0, 0};
unsigned long ultimaRampa = 0;

// [NO BLOQUEANTE] FRENANDO: a qué estado pasa cuando paran las ruedas
MAQUINA_NUEVA estadoPostFreno = LISTO;
unsigned long tiempoInicioFreno = 0;
unsigned long ultimoChequeoFreno = 0;
int32_t pulsosChequeoFreno = 0;

// [WATCHDOG] Cuándo entró al estado actual
unsigned long tiempoInicioEstado = 0;

//==============================================================
//                    FUNCIONES AUXILIARES
//==============================================================

// [LOG LECTURAS] Arma el texto " I:x C:y D:z" con las 3 distancias
String textoLecturas(sensado s) {
  return " I:" + String(s.distanciaIzq) + " C:" + String(s.distanciaCent) + " D:" + String(s.distanciaDer);
}

// [DIGITAL] Menos de UMBRAL_PARED = pared; más de UMBRAL_LIBRE = libre; en el medio no cambia nada
void actualizarDigital(int16_t lectura, bool &hayPared) {
  if (lectura < configRobot.umbralPared) hayPared = true;
  else if (lectura > configRobot.umbralLibre) hayPared = false;
}

// Una lectura por vuelta del loop, en todos los estados: los estados digitales siempre están al día
void leerSensores() {
  sensadoActual = actualizarSensado();
  if (!sensadoActual.nueva) return;
  actualizarDigital(sensadoActual.distanciaIzq, paredIzq);
  actualizarDigital(sensadoActual.distanciaCent, paredCent);
  actualizarDigital(sensadoActual.distanciaDer, paredDer);
}

void cambiarEstado(MAQUINA_NUEVA nuevo) {
  estado = nuevo;
  tiempoInicioEstado = millis();
}

// [WATCHDOG] La maniobra tarda demasiado (rueda trabada, VP muy baja): se da por terminada
bool tiempoAgotado() {
  return millis() - tiempoInicioEstado > TIEMPO_MAX_MANIOBRA_MS;
}

// [SOSPECHA] Velocidad de crucero limitada a "maxima": VS y VP nunca son más rápidas que la base
VELOCIDAD velocidadMaxima(int16_t maxima) {
  VELOCIDAD v = {configRobot.velBaseIzq, configRobot.velBaseDer};
  if (v.izquierda > maxima) v.izquierda = maxima;
  if (v.derecha > maxima) v.derecha = maxima;
  return v;
}

VELOCIDAD velocidadCrucero()  { return velocidadMaxima(255); }
VELOCIDAD velocidadSospecha() { return velocidadMaxima(configRobot.velSospecha); }
VELOCIDAD velocidadPregiro()  { return velocidadMaxima(min(configRobot.velPregiro, configRobot.velSospecha)); }

// [RAMPA] Acerca una velocidad al objetivo un paso: sube de a ACEL y baja de a DESAC
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
// ella el motor casi no se mueve.
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
// después pasa a "siguiente"
void iniciarFreno(MAQUINA_NUEVA siguiente) {
  movimiento(FRENO_F, {0, 0});
  velocidadRampa = {0, 0}; // [RAMPA] Después de frenar vuelve a arrancar desde VP
  estadoPostFreno = siguiente;
  tiempoInicioFreno = millis();
  ultimoChequeoFreno = tiempoInicioFreno;
  pulsosChequeoFreno = leerPulsos();
  estado = FRENANDO;
}

// Vuelve a AVANZANDO SIN FRENAR (largada, abertura falsa, fin del postgiro, decisión de seguir)
void volverAAvanzar() {
  resetearErrorAnterior();
  lecturasAbertura = 0;
  cambiarEstado(AVANZANDO);
}

// Avanza derecho SIN PID (pregiro y postgiro). Devuelve true cuando terminó: hizo los pulsos,
// la pared de enfrente quedó en el centro de la celda o se agotó el tiempo
bool avanzarHasta(uint16_t pulsos, VELOCIDAD objetivo) {
  actualizarRampa(objetivo);
  movimiento(AVANZAR, velocidadRampa);
  return leerPulsos() >= pulsos || sensadoActual.distanciaCent < DISTANCIA_CORTE_FRENTE || tiempoAgotado();
}

// Giro SOLO por encoders: los sensores no intervienen. Devuelve true cuando terminó
bool girarHasta(MOVIMIENTOS giro, uint16_t pulsos, VELOCIDAD velocidad) {
  if (leerPulsos() >= pulsos || tiempoAgotado()) return true;
  movimiento(giro, velocidad);
  return false;
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
  leerSensores(); // Una lectura por vuelta, en todos los estados

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
      if (digitalRead(BOTON1) == LOW) {
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
      // Botón 1: Right Hand. Botón 2: Left Hand
      bool boton1 = digitalRead(BOTON1) == LOW;
      bool boton2 = digitalRead(BOTON2) == LOW;
      if (!boton1 && !boton2) break;

      while(digitalRead(BOTON1) == LOW || digitalRead(BOTON2) == LOW) { delay(10); } // Antirrebote al soltar
      modoActual = boton1 ? MODO_RIGHT_HAND : MODO_LEFT_HAND;
      enviarString(boton1 ? ">>> LARGADA: MODO RIGHT HAND <<<" : ">>> LARGADA: MODO LEFT HAND <<<");
      delay(1000); // 1 segundo de margen para sacar la mano
      volverAAvanzar();
      break;
    }

    case AVANZANDO: {
      // [FRENTE] Pared de enfrente en el centro de la celda: frena y decide quieto
      if (sensadoActual.distanciaCent < DISTANCIA_CORTE_FRENTE) {
        iniciarFreno(DECISION);
        enviarString(">>> PARED AL FRENTE <<<" + textoLecturas(sensadoActual)); // Log después de frenar
        break;
      }

      // Lado prioritario: RIGHT HAND mira la derecha, LEFT HAND la izquierda
      bool rightHand = modoActual == MODO_RIGHT_HAND;
      int16_t lecturaLado = rightHand ? sensadoActual.distanciaDer : sensadoActual.distanciaIzq;

      // [DEBOUNCE] Girar es caro: hacen falta LECTURAS_CONFIRMACION lecturas nuevas SEGURAS de libre
      // del lado prioritario. Las dudosas no suman ni restan; una de pared vuelve a cero.
      // Si el lado prioritario tiene pared, el otro lado no importa: sigue derecho SIN FRENAR.
      if (sensadoActual.nueva) {
        if (lecturaLado < configRobot.umbralPared) {
          lecturasAbertura = 0;
        } else if (lecturaLado > configRobot.umbralLibre) {
          lecturasAbertura++;
          // Los pulsos del pregiro se cuentan desde la PRIMERA lectura libre: así PRED/PREI no
          // dependen de cuánto tarda en confirmar
          if (lecturasAbertura == 1) resetearEncoders();
        }

        if (lecturasAbertura >= LECTURAS_CONFIRMACION) {
          // [ALTA VELOCIDAD] P = pulsos del pregiro que ya se usaron para confirmar. Si se acerca
          // a PRED/PREI, el robot va demasiado rápido para llegar a VP antes de frenar
          enviarString(String(rightHand ? ">>> PREGIRO DERECHA" : ">>> PREGIRO IZQUIERDA") +
                       " P:" + String(leerPulsos()) + " <<<" + textoLecturas(sensadoActual));
          cambiarEstado(rightHand ? PREGIRO_DER : PREGIRO_IZQ);
          break;
        }
      }

      // [SOSPECHA] Bajar la velocidad es barato: alcanza con una sola lectura.
      //  - VP: abertura del lado prioritario (se está confirmando un giro) o pared cerca al frente
      //    (frente no libre seguro). En los dos casos viene un freno: llega lento y frena corto.
      //  - VS: pared del lado prioritario no segura (dudosa) o pared al frente a ~1 celda.
      //  - Si no: crucero.
      VELOCIDAD objetivo = velocidadCrucero();
      if (lecturasAbertura > 0 || sensadoActual.distanciaCent <= configRobot.umbralLibre) {
        objetivo = velocidadPregiro();
      } else if (lecturaLado >= configRobot.umbralPared || sensadoActual.distanciaCent < UMBRAL_FRENTE_SOSPECHA) {
        objetivo = velocidadSospecha();
      }
      actualizarRampa(objetivo);

      // PID sobre la velocidad de la rampa (el tope del PID es un % de esa velocidad)
      int16_t correccion = calcularCorreccion(sensadoActual, (velocidadRampa.izquierda + velocidadRampa.derecha) / 2);
      VELOCIDAD velocidad;
      velocidad.izquierda = constrain(velocidadRampa.izquierda + correccion, 0, 255);
      velocidad.derecha = constrain(velocidadRampa.derecha - correccion, 0, 255);
      movimiento(AVANZAR, velocidad);
      break;
    }

    case PREGIRO_DER: {
      // [SOSPECHA] A VP hasta el centro de la celda, sin PID (el lado del giro está abierto)
      if (!avanzarHasta(configRobot.pulsosPregiroDer, velocidadPregiro())) break;
      if (paredDer) {
        // [NO FRENA] La última lectura segura de la derecha fue de pared: la abertura era falsa
        enviarString(">>> PREGIRO DERECHA CANCELADO: era lectura falsa <<<");
        volverAAvanzar();
      } else {
        iniciarFreno(GIRANDO_DER);
        enviarString(">>> FIN PREGIRO DERECHA -> GIRA <<<");
      }
      break;
    }

    case PREGIRO_IZQ: {
      // [SOSPECHA] A VP hasta el centro de la celda, sin PID (el lado del giro está abierto)
      if (!avanzarHasta(configRobot.pulsosPregiroIzq, velocidadPregiro())) break;
      if (paredIzq) {
        // [NO FRENA] La última lectura segura de la izquierda fue de pared: la abertura era falsa
        enviarString(">>> PREGIRO IZQUIERDA CANCELADO: era lectura falsa <<<");
        volverAAvanzar();
      } else {
        iniciarFreno(GIRANDO_IZQ);
        enviarString(">>> FIN PREGIRO IZQUIERDA -> GIRA <<<");
      }
      break;
    }

    case DECISION: {
      movimiento(FRENO_F, {0,0});
      // [DECISION QUIETO] Robot quieto en el centro de la celda: mediana de 3 lecturas (~100 ms),
      // pasada a digital con los mismos umbrales (si sale dudosa, queda lo último seguro)
      sensado s = leerMediana3();
      actualizarDigital(s.distanciaIzq, paredIzq);
      actualizarDigital(s.distanciaCent, paredCent);
      actualizarDigital(s.distanciaDer, paredDer);

      MAQUINA_NUEVA siguiente;
      if (modoActual == MODO_RIGHT_HAND) {
        // ORDEN DE PRIORIDADES RIGHT HAND: 1° Derecha, 2° Frente, 3° Izquierda, 4° 180
        if (!paredDer)       siguiente = GIRANDO_DER;
        else if (!paredCent) siguiente = AVANZANDO;
        else if (!paredIzq)  siguiente = GIRANDO_IZQ;
        else                 siguiente = GIRANDO_180;
      } else {
        // ORDEN DE PRIORIDADES LEFT HAND: 1° Izquierda, 2° Frente, 3° Derecha, 4° 180
        if (!paredIzq)       siguiente = GIRANDO_IZQ;
        else if (!paredCent) siguiente = AVANZANDO;
        else if (!paredDer)  siguiente = GIRANDO_DER;
        else                 siguiente = GIRANDO_180;
      }

      String accion = (siguiente == GIRANDO_DER) ? " -> GIRA DERECHA <<<"
                    : (siguiente == GIRANDO_IZQ) ? " -> GIRA IZQUIERDA <<<"
                    : (siguiente == GIRANDO_180) ? " -> GIRA 180 <<<"
                    : " -> SIGUE DERECHO (era lectura falsa) <<<";
      enviarString(">>> DECISION" + accion + textoLecturas(s));

      resetearEncoders();
      if (siguiente == AVANZANDO) volverAAvanzar();
      else cambiarEstado(siguiente);
      break;
    }

    case GIRANDO_DER: {
      if (!girarHasta(GIRAR_DER, configRobot.pulsosGiro90Der, {configRobot.velBaseIzqGiro, configRobot.velBaseDerGiro})) break;
      iniciarFreno(POSTGIRO); // [NO BLOQUEANTE] La inercia del giro no entra al POSTGIRO
      movimientoAnterior = GIRANDO_DER;
      enviarString(">>> FIN GIRO DERECHA <<<");
      break;
    }

    case GIRANDO_IZQ: {
      if (!girarHasta(GIRAR_IZQ, configRobot.pulsosGiro90Izq, {configRobot.velBaseIzqGiro, configRobot.velBaseDerGiro})) break;
      iniciarFreno(POSTGIRO); // [NO BLOQUEANTE] La inercia del giro no entra al POSTGIRO
      movimientoAnterior = GIRANDO_IZQ;
      enviarString(">>> FIN GIRO IZQUIERDA <<<");
      break;
    }

    case GIRANDO_180: {
      // [180 OPUESTO] Gira hacia el lado contrario al último giro: si el último fue a la izquierda,
      // el 180 es a la derecha; si fue a la derecha (o todavía no giró), a la izquierda.
      // movimientoAnterior recién cambia al terminar, así que el lado no cambia a mitad del 180.
      bool giro180Der = movimientoAnterior == GIRANDO_IZQ;
      uint16_t pulsos180 = giro180Der ? configRobot.pulsosGiro180Der : configRobot.pulsosGiro180Izq;
      if (!girarHasta(giro180Der ? GIRAR_DER : GIRAR_IZQ, pulsos180, {configRobot.velBaseIzqGiro180, configRobot.velBaseDerGiro180})) break;
      iniciarFreno(POSTGIRO); // [NO BLOQUEANTE] La inercia del giro no entra al POSTGIRO
      // [180 OPUESTO] Se guarda el lado del 180, así el próximo 180 seguido sale para el otro lado
      movimientoAnterior = giro180Der ? GIRANDO_DER : GIRANDO_IZQ;
      enviarString(giro180Der ? ">>> FIN GIRO 180 DER <<<" : ">>> FIN GIRO 180 IZQ <<<");
      break;
    }

    case POSTGIRO: {
      // [SOSPECHA] Sale del giro a VS (arranca en VP y sube con rampa), sin PID: recién entra al
      // pasillo nuevo
      if (!avanzarHasta(configRobot.pulsosPostgiro, velocidadSospecha())) break;
      enviarString(">>> FIN POSTGIRO <<<");
      volverAAvanzar(); // Sin frenar
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
        cambiarEstado(estadoPostFreno);
      }
      break;
    }
  }
}
