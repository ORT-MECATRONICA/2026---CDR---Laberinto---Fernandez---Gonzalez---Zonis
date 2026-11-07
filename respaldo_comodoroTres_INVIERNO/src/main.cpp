//==============================================================
//                          INVIERNO
// OTOÑO CON LOS ARREGLOS DE LOS CHOQUES (VER CAMBIOS.md, ETIQUETAS [C1]..[C12]):
//  - FRENA CONTRA LA PARED DE ENFRENTE SEGÚN LA DISTANCIA (PERFIL), TAMBIÉN EN PREGIRO Y POSTGIRO
//  - LOS ENCODERS SE PONEN EN CERO SOLO EN EL CENTRO DE UNA CELDA: EL PREGIRO SABE DESDE DÓNDE
//    CONTAR AUNQUE NO HAYA VISTO EL BORDE DE UNA PARED (DOS GIROS SEGUIDOS)
//  - PID EN PREGIRO (CON LA PARED DEL OTRO LADO) Y EN POSTGIRO, SOLO CON PAREDES SEGURAS
//  - LOGS QUE NO TRABAN EL LOOP, SENSORES A 50 HZ Y WATCHDOG DE SENSORES
// SE LLAMA INVIERNO EN HOMENAJE A "INVIERNO PORTEÑO", DE PIAZZOLLA (VIENE DESPUÉS DE OTOÑO).
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

// [C4] [CENTRO DE CELDA] Los encoders se ponen en cero SOLO con el robot quieto en el centro de
// una celda (después de un giro, en DECISION y en la largada). Así leerPulsos() es siempre
// "cuánto avanzó desde el último centro". Antes también se ponían en cero en la primera lectura
// libre y se perdía esa referencia.
int32_t pulsosAbertura = 0;    // leerPulsos() en la primera lectura libre de AVANZANDO (como OTOÑO)
bool vioParedLado = false;     // desde el último centro, el lado prioritario leyó algo que NO es libre
bool bordePendiente = false;   // la última lectura del lado prioritario no fue libre
int32_t pulsosBorde = 0;       // leerPulsos() en la primera lectura libre después de una que no lo era
int32_t objetivoPregiro = 0;   // leerPulsos() en el que termina el pregiro
bool pregiroSinBorde = false;  // el pregiro cuenta desde el centro (PCELDA) y no desde el borde

// [C9] [VETO] Lecturas seguras de pared seguidas del lado del giro durante el pregiro
uint8_t lecturasParedPregiro = 0;

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

  // [C4] [BORDE] El borde de una pared es la PRIMERA lectura libre después de una que no era
  // libre (pared o dudosa). Se anota en cualquier estado, también en POSTGIRO: así, si la punta de
  // una pared pasa mientras sale de un giro, el pregiro igual cuenta desde ahí.
  int16_t lecturaLado = (modoActual == MODO_RIGHT_HAND) ? sensadoActual.distanciaDer : sensadoActual.distanciaIzq;
  if (lecturaLado <= configRobot.umbralLibre) {
    vioParedLado = true;
    bordePendiente = true;
  } else if (bordePendiente) {
    pulsosBorde = leerPulsos();
    bordePendiente = false;
  }
}

void cambiarEstado(MAQUINA_NUEVA nuevo) {
  estado = nuevo;
  tiempoInicioEstado = millis();
}

// [WATCHDOG] La maniobra tarda demasiado (rueda trabada, VP muy baja): se da por terminada
bool tiempoAgotado() {
  return millis() - tiempoInicioEstado > TIEMPO_MAX_MANIOBRA_MS;
}

// [SOSPECHA] Velocidad de crucero limitada a "maxima": VS y VP nunca son más rápidas que la base.
// [C10] Si hay que limitarla, se achican las DOS ruedas en la misma proporción: así se mantiene
// la diferencia entre VD y VI (antes las dos quedaban iguales y se perdía esa compensación)
VELOCIDAD velocidadMaxima(int16_t maxima) {
  VELOCIDAD v = {configRobot.velBaseIzq, configRobot.velBaseDer};
  int16_t mayor = max(v.izquierda, v.derecha);
  if (mayor > maxima && mayor > 0) {
    v.izquierda = (int32_t)v.izquierda * maxima / mayor;
    v.derecha = (int32_t)v.derecha * maxima / mayor;
  }
  return v;
}

VELOCIDAD velocidadCrucero()  { return velocidadMaxima(255); }
VELOCIDAD velocidadSospecha() { return velocidadMaxima(configRobot.velSospecha); }
VELOCIDAD velocidadPregiro()  { return velocidadMaxima(min(configRobot.velPregiro, configRobot.velSospecha)); }

// [C12] La pared de enfrente ya está en el centro de la celda
bool frenteEnCorte() {
  return sensadoActual.distanciaCent < (int16_t)configRobot.distanciaCorte;
}

// [C6] [FRENADO POR FRENTE] Tope de velocidad según la distancia a la pared de enfrente:
// a DFRENO mm (o más) del punto de corte no limita; desde ahí baja en línea recta hasta VP justo
// en el corte. Así llega siempre lento al corte, venga a la velocidad que venga, y no frena antes
// de lo necesario. Se usa en AVANZANDO, PREGIRO y POSTGIRO (antes POSTGIRO no miraba el frente)
VELOCIDAD limitarPorFrente(VELOCIDAD objetivo) {
  int32_t falta = sensadoActual.distanciaCent - (int32_t)configRobot.distanciaCorte;
  if (falta >= configRobot.distanciaFrenado) return objetivo;
  if (falta < 0) falta = 0;
  VELOCIDAD vp = velocidadPregiro();
  VELOCIDAD crucero = velocidadCrucero();
  VELOCIDAD tope;
  tope.izquierda = vp.izquierda + (int32_t)(crucero.izquierda - vp.izquierda) * falta / configRobot.distanciaFrenado;
  tope.derecha = vp.derecha + (int32_t)(crucero.derecha - vp.derecha) * falta / configRobot.distanciaFrenado;
  if (objetivo.izquierda > tope.izquierda) objetivo.izquierda = tope.izquierda;
  if (objetivo.derecha > tope.derecha) objetivo.derecha = tope.derecha;
  return objetivo;
}

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

// [C8] Avanza con rampa y PID usando solo los lados permitidos. [C6] Nunca más rápido de lo que
// permite la pared de enfrente. Lo usan AVANZANDO, PREGIRO y POSTGIRO (antes los dos últimos
// iban sin PID: si salían torcidos de un giro, seguían torcidos hasta la pared)
void avanzarConPID(VELOCIDAD objetivo, bool usarIzq, bool usarDer) {
  actualizarRampa(limitarPorFrente(objetivo));
  // PID sobre la velocidad de la rampa (el tope del PID es un % de esa velocidad)
  int16_t correccion = calcularCorreccion(sensadoActual, (velocidadRampa.izquierda + velocidadRampa.derecha) / 2, usarIzq, usarDer);
  VELOCIDAD velocidad;
  velocidad.izquierda = constrain(velocidadRampa.izquierda + correccion, 0, 255);
  velocidad.derecha = constrain(velocidadRampa.derecha - correccion, 0, 255);
  movimiento(AVANZAR, velocidad);
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

// [C4] El robot está QUIETO en el centro de una celda: desde acá se cuentan los pulsos
void marcarCentroDeCelda() {
  resetearEncoders();
  vioParedLado = false;
  bordePendiente = false;
  lecturasAbertura = 0;
}

// [C7] En los últimos "zona" pulsos del giro la velocidad baja en línea recta hasta "minimo"
int16_t bajarEnZona(int16_t velocidad, int16_t minimo, int32_t falta, uint16_t zona) {
  if (zona == 0 || falta >= zona || velocidad <= minimo) return velocidad;
  return minimo + (int32_t)(velocidad - minimo) * falta / zona;
}

// Giro SOLO por encoders: los sensores no intervienen. Devuelve true cuando terminó
// [C7] Al final del giro baja la velocidad hasta VP: frena siempre desde la misma velocidad
bool girarHasta(MOVIMIENTOS giro, uint16_t pulsos, VELOCIDAD velocidad) {
  int32_t hechos = leerPulsos();
  if (hechos >= pulsos || tiempoAgotado()) return true;
  int32_t falta = pulsos - hechos;
  velocidad.izquierda = bajarEnZona(velocidad.izquierda, configRobot.velPregiro, falta, configRobot.zonaFrenoGiro);
  velocidad.derecha = bajarEnZona(velocidad.derecha, configRobot.velPregiro, falta, configRobot.zonaFrenoGiro);
  movimiento(giro, velocidad);
  return false;
}

// [C4] [C8] [C9] Una vuelta del pregiro hacia "derecha" (true) o izquierda (false)
void pasoPregiro(bool derecha) {
  int16_t lecturaGiro = derecha ? sensadoActual.distanciaDer : sensadoActual.distanciaIzq;
  uint16_t pre = derecha ? configRobot.pulsosPregiroDer : configRobot.pulsosPregiroIzq;
  const char* lado = derecha ? "DERECHA" : "IZQUIERDA";

  // [C9] [VETO] Pared segura = suma; libre seguro = vuelve a cero; dudosa = no dice nada
  if (sensadoActual.nueva) {
    if (lecturaGiro < configRobot.umbralPared) lecturasParedPregiro++;
    else if (lecturaGiro > configRobot.umbralLibre) lecturasParedPregiro = 0;
  }
  if (lecturasParedPregiro >= LECTURAS_PARED_VETO) {
    // [NO FRENA] La pared volvió (2 lecturas seguras seguidas): la abertura era falsa, o era el
    // pasillo por el que vino (abertura sin borde). Sigue sin frenar.
    guardarLog(String(">>> PREGIRO ") + lado + " CANCELADO: volvio la pared P:" + String(leerPulsos()) + " <<<");
    volverAAvanzar();
    return;
  }

  int32_t falta = objetivoPregiro - leerPulsos();
  if (falta > 0 && !frenteEnCorte() && !tiempoAgotado()) {
    // [C4] Si falta más que un pregiro normal (abertura sin borde) va a VS; cerca del final, a VP
    VELOCIDAD objetivo = (falta > pre) ? velocidadSospecha() : velocidadPregiro();
    // [C8] PID solo con la pared del lado CONTRARIO al giro (el lado del giro está abierto)
    avanzarConPID(objetivo, derecha, !derecha);
    return;
  }

  iniciarFreno(derecha ? GIRANDO_DER : GIRANDO_IZQ);
  guardarLog(String(">>> FIN PREGIRO ") + lado + " -> GIRA P:" + String(leerPulsos()) +
             (frenteEnCorte() ? " (corto por frente)" : "") + (tiempoAgotado() ? " TIMEOUT" : "") + " <<<");
}

// [C3] [WATCHDOG SENSORES] Estados en los que el robot anda guiado por los sensores
bool estadoUsaSensores() {
  return estado == AVANZANDO || estado == PREGIRO_DER || estado == PREGIRO_IZQ || estado == POSTGIRO;
}

String textoSensores(uint8_t bits) {
  String t = "";
  if (bits & 0x01) t += " IZQ";
  if (bits & 0x02) t += " CENT";
  if (bits & 0x04) t += " DER";
  return t;
}

// [C3] Si un sensor dejó de responder con el robot andando: frena, reinicia los sensores y sigue
// en el mismo estado. Antes el robot seguía con la última lectura (por ejemplo, sin ver nunca la
// pared de enfrente) y, como "nueva" no llegaba más, dejaba de decidir.
void revisarSensores() {
  if (!estadoUsaSensores()) return;
  uint8_t colgados = sensoresColgados();
  if (colgados == 0) return;

  movimiento(FRENO_F, {0, 0}); // queda frenado mientras reinicia
  enviarLogsGuardados();
  enviarString(">>> SENSOR COLGADO:" + textoSensores(colgados) + " -> REINICIANDO <<<");
  for (uint8_t intento = 1; intento <= INTENTOS_REINICIO_SENSORES; intento++) {
    // leerMediana3() espera lecturas frescas de los 3 (hasta 500 ms). No decide nada: solo sirve
    // para saber si volvieron. Si no llegan, sensoresColgados() sigue dando distinto de 0.
    if (!reiniciarSensores()) continue;
    leerMediana3();
    if (sensoresColgados() != 0) continue;

    enviarString(">>> SENSORES OK (intento " + String(intento) + "): SIGUE <<<");
    velocidadRampa = {0, 0};
    resetearErrorAnterior();
    cambiarEstado(estado); // sigue en el mismo estado (reinicia el timeout de la maniobra)
    return;
  }
  enviarString(">>> NO SE PUDIERON REINICIAR LOS SENSORES: ROBOT DETENIDO <<<");
  estado = LISTO;
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
  leerSensores();    // Una lectura por vuelta, en todos los estados
  revisarSensores(); // [C3]

  switch (estado) {
    case LISTO: {
      movimiento(FRENO_F, {0,0});
      enviarLogsGuardados(); // [C1]

      #ifdef USAR_BLUETOOTH
      if (hayDatosBT()) {
        estado = INFORMACION_RECIBIDA;
        break;
      }
      #endif

      // Presionar Botón 1 para avisar que se terminó de cargar datos
      if (digitalRead(BOTON1) == LOW) {
        while(digitalRead(BOTON1) == LOW) { delay(10); } // Antirrebote al soltar
        // [C3] No larga con un sensor muerto: lo intenta reiniciar y, si no anda, avisa
        if (!sensoresIniciadosOK() && !reiniciarSensores()) {
          enviarString(">>> NO ARRANCA: hay un sensor que no responde (revisar cables) <<<");
          break;
        }
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
      if (configRobot.pulsosCelda == 0) enviarString(">>> AVISO: PCELDA sin calibrar (dos giros seguidos como OTOÑO) <<<"); // [C4]
      delay(1000); // 1 segundo de margen para sacar la mano
      marcarCentroDeCelda(); // [C4] La largada es en el centro de una celda
      volverAAvanzar();
      break;
    }

    case AVANZANDO: {
      // [FRENTE] Pared de enfrente en el centro de la celda: frena y decide quieto
      if (frenteEnCorte()) {
        iniciarFreno(DECISION);
        // [C4] P = pulsos desde el último centro de celda: sirve para calibrar PCELDA
        guardarLog(">>> PARED AL FRENTE" + textoLecturas(sensadoActual) + " P:" + String(leerPulsos()) + " <<<");
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
          // [C4] Antes acá se ponían los encoders en cero. Ahora se ANOTA dónde fue la primera
          // lectura libre y los encoders siguen contando desde el último centro de celda
          if (lecturasAbertura == 1) pulsosAbertura = leerPulsos();
        }

        if (lecturasAbertura >= LECTURAS_CONFIRMACION) {
          // [C4] Dónde termina el pregiro:
          //  - Con borde (vio pared de ese lado desde el último centro): PRED/PREI desde el
          //    borde, como siempre.
          //  - Sin borde (desde el último centro el lado prioritario siempre leyó libre: es el
          //    pasillo por el que vino, como en dos giros seguidos): en el centro de la celda
          //    SIGUIENTE = PCELDA desde el centro. Nunca antes que PRED/PREI desde la primera
          //    lectura libre. Si PCELDA = 0 (sin calibrar), como OTOÑO.
          uint16_t pre = rightHand ? configRobot.pulsosPregiroDer : configRobot.pulsosPregiroIzq;
          pregiroSinBorde = !vioParedLado;
          if (!pregiroSinBorde)                objetivoPregiro = pulsosBorde + pre;
          else if (configRobot.pulsosCelda > 0) objetivoPregiro = max((int32_t)configRobot.pulsosCelda, pulsosAbertura + pre);
          else                                 objetivoPregiro = pulsosAbertura + pre;
          lecturasParedPregiro = 0; // [C9]
          // [ALTA VELOCIDAD] P = pulsos desde el centro; O = donde termina el pregiro.
          // Si P ya está cerca de O, venía demasiado rápido para llegar a VP antes de frenar
          guardarLog(String(rightHand ? ">>> PREGIRO DERECHA" : ">>> PREGIRO IZQUIERDA") +
                     (pregiroSinBorde ? (configRobot.pulsosCelda > 0 ? " SIN BORDE" : " SIN BORDE (PCELDA=0)") : "") +
                     " P:" + String(leerPulsos()) + " O:" + String(objetivoPregiro) + " <<<" +
                     textoLecturas(sensadoActual));
          cambiarEstado(rightHand ? PREGIRO_DER : PREGIRO_IZQ);
          break;
        }
      }

      // [SOSPECHA] Bajar la velocidad es barato: alcanza con una sola lectura.
      //  - VP: abertura del lado prioritario (se está confirmando un giro).
      //  - VS: pared del lado prioritario no segura (dudosa).
      //  - Si no: crucero.
      // [C6] La pared de enfrente ya no usa escalones (VS a 250, VP a 130): la limita
      //      limitarPorFrente() dentro de avanzarConPID()
      VELOCIDAD objetivo = velocidadCrucero();
      if (lecturasAbertura > 0) {
        objetivo = velocidadPregiro();
      } else if (lecturaLado >= configRobot.umbralPared) {
        objetivo = velocidadSospecha();
      }
      // [C8] Desde la primera lectura libre el lado prioritario no entra al PID (se está abriendo)
      bool usarLado = lecturasAbertura == 0;
      avanzarConPID(objetivo, rightHand ? true : usarLado, rightHand ? usarLado : true);
      break;
    }

    case PREGIRO_DER: {
      pasoPregiro(true);
      break;
    }

    case PREGIRO_IZQ: {
      pasoPregiro(false);
      break;
    }

    case DECISION: {
      movimiento(FRENO_F, {0,0});
      enviarLogsGuardados(); // [C1] Robot quieto: manda lo que guardó mientras andaba
      // [DECISION QUIETO] Robot quieto en el centro de la celda: mediana de 3 lecturas (~60 ms),
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

      marcarCentroDeCelda(); // [C4] Frenó en el centro de la celda (incluye resetearEncoders)
      if (siguiente == AVANZANDO) volverAAvanzar();
      else cambiarEstado(siguiente);
      break;
    }

    case GIRANDO_DER: {
      if (!girarHasta(GIRAR_DER, configRobot.pulsosGiro90Der, {configRobot.velBaseIzqGiro, configRobot.velBaseDerGiro})) break;
      iniciarFreno(POSTGIRO); // [NO BLOQUEANTE] La inercia del giro no entra al POSTGIRO
      movimientoAnterior = GIRANDO_DER;
      guardarLog(">>> FIN GIRO DERECHA <<<");
      break;
    }

    case GIRANDO_IZQ: {
      if (!girarHasta(GIRAR_IZQ, configRobot.pulsosGiro90Izq, {configRobot.velBaseIzqGiro, configRobot.velBaseDerGiro})) break;
      iniciarFreno(POSTGIRO); // [NO BLOQUEANTE] La inercia del giro no entra al POSTGIRO
      movimientoAnterior = GIRANDO_IZQ;
      guardarLog(">>> FIN GIRO IZQUIERDA <<<");
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
      guardarLog(giro180Der ? ">>> FIN GIRO 180 DER <<<" : ">>> FIN GIRO 180 IZQ <<<");
      break;
    }

    case POSTGIRO: {
      // [SOSPECHA] Sale del giro a VS (arranca en VP y sube con rampa).
      // [C8] Ahora CON PID (con las paredes seguras que haya): si el giro lo dejó torcido o corrido,
      // lo corrige enseguida en vez de seguir derecho hasta la pared.
      // [C6] Y frenando si tiene una pared adelante (antes iba a VS sin mirar el frente)
      if (frenteEnCorte() || leerPulsos() >= configRobot.pulsosPostgiro || tiempoAgotado()) {
        guardarLog(">>> FIN POSTGIRO <<<");
        volverAAvanzar(); // Sin frenar (si cortó por frente, AVANZANDO frena en la vuelta siguiente)
        break;
      }
      avanzarConPID(velocidadSospecha(), true, true);
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
        // [C4] Terminó un giro: el robot está quieto en el centro de la celda
        if (estadoPostFreno == POSTGIRO) marcarCentroDeCelda();
        enviarLogsGuardados(); // [C1] Ruedas quietas: ahora sí manda los mensajes
        cambiarEstado(estadoPostFreno);
      }
      break;
    }
  }
}
