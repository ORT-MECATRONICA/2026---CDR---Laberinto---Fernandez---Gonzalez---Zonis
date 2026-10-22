#include "calibracion.h"
#include "config.h"
#include <Preferences.h>

ParametrosRobot configRobot;
static Preferences pref;
static const char* NAMESPACE_NVS = "micromouse";

void inicializarCalibracion() {
    pref.begin(NAMESPACE_NVS, false);

    configRobot.pulsosPregiroDer  = pref.getUShort("pred", PULSOS_PREGIRO_90_DER);
    configRobot.pulsosPregiroIzq  = pref.getUShort("prei", PULSOS_PREGIRO_90_IZQ);
    configRobot.pulsosPostgiro    = pref.getUShort("post", PULSOS_POSTGIRO);
    configRobot.pulsosGiro90Der   = pref.getUShort("g90d", PULSOS_GIRO_90_DER);
    configRobot.pulsosGiro90Izq   = pref.getUShort("g90i", PULSOS_GIRO_90_IZQ);
    configRobot.pulsosGiro180Izq  = pref.getUShort("g180i", PULSOS_GIRO_180_IZQ);
    configRobot.pulsosGiro180Der  = pref.getUShort("g180d", PULSOS_GIRO_180_DER);

    configRobot.velBaseDer        = pref.getUChar("vd", VEL_BASE_DER);
    configRobot.velBaseIzq        = pref.getUChar("vi", VEL_BASE_IZQ);
    configRobot.velBaseDerGiro    = pref.getUChar("vgd", VEL_BASE_DER_GIRO);
    configRobot.velBaseIzqGiro    = pref.getUChar("vgi", VEL_BASE_IZQ_GIRO);
    configRobot.velBaseDerGiro180 = pref.getUChar("v180d", VEL_BASE_DER_GIRO180);
    configRobot.velBaseIzqGiro180 = pref.getUChar("v180i", VEL_BASE_IZQ_GIRO180);

    configRobot.kp                = pref.getFloat("kp", KP);

    pref.end();
}

void restablecerDefaults() {
    pref.begin(NAMESPACE_NVS, false);
    pref.clear();

    configRobot.pulsosPregiroDer  = PULSOS_PREGIRO_90_DER;
    configRobot.pulsosPregiroIzq  = PULSOS_PREGIRO_90_IZQ;
    configRobot.pulsosPostgiro    = PULSOS_POSTGIRO;
    configRobot.pulsosGiro90Der   = PULSOS_GIRO_90_DER;
    configRobot.pulsosGiro90Izq   = PULSOS_GIRO_90_IZQ;
    configRobot.pulsosGiro180Izq  = PULSOS_GIRO_180_IZQ;
    configRobot.pulsosGiro180Der  = PULSOS_GIRO_180_DER;

    configRobot.velBaseDer        = VEL_BASE_DER;
    configRobot.velBaseIzq        = VEL_BASE_IZQ;
    configRobot.velBaseDerGiro    = VEL_BASE_DER_GIRO;
    configRobot.velBaseIzqGiro    = VEL_BASE_IZQ_GIRO;
    configRobot.velBaseDerGiro180 = VEL_BASE_DER_GIRO180;
    configRobot.velBaseIzqGiro180 = VEL_BASE_IZQ_GIRO180;

    configRobot.kp                = KP;

    pref.end();
}

String procesarComandoCalibracion(String comando) {
    comando.trim();
    if (comando.length() == 0) return "";

    String cmdUpper = comando;
    cmdUpper.toUpperCase();

    if (cmdUpper == "VER" || cmdUpper == "STATUS") {
        return obtenerReporteCalibracion();
    }

    if (cmdUpper == "DEFAULT" || cmdUpper == "RESET") {
        restablecerDefaults();
        return ">>> VALORES POR DEFECTO RESTABLECIDOS EN FLASH <<<";
    }

    int idx = comando.indexOf('=');
    if (idx == -1) idx = comando.indexOf(':');

    if (idx == -1) {
        return "ERROR: Formato incorrecto. Use CLAVE=VALOR (ej: KP=0.25, P90D=320, VER)";
    }

    String clave = comando.substring(0, idx);
    clave.trim();
    clave.toUpperCase();

    String valorStr = comando.substring(idx + 1);
    valorStr.trim();

    pref.begin(NAMESPACE_NVS, false);
    String resp = "";

    if (clave == "KP") {
        float val = valorStr.toFloat();
        configRobot.kp = val;
        pref.putFloat("kp", val);
        resp = "ACK: KP = " + String(val, 3);
    } else if (clave == "PRED" || clave == "PRE_DER") {
        uint16_t val = valorStr.toInt();
        configRobot.pulsosPregiroDer = val;
        pref.putUShort("pred", val);
        resp = "ACK: pulsosPregiroDer = " + String(val);
    } else if (clave == "PREI" || clave == "PRE_IZQ") {
        uint16_t val = valorStr.toInt();
        configRobot.pulsosPregiroIzq = val;
        pref.putUShort("prei", val);
        resp = "ACK: pulsosPregiroIzq = " + String(val);
    } else if (clave == "POST") {
        uint16_t val = valorStr.toInt();
        configRobot.pulsosPostgiro = val;
        pref.putUShort("post", val);
        resp = "ACK: pulsosPostgiro = " + String(val);
    } else if (clave == "G90D" || clave == "P90D" || clave == "GIRO_DER") {
        uint16_t val = valorStr.toInt();
        configRobot.pulsosGiro90Der = val;
        pref.putUShort("g90d", val);
        resp = "ACK: pulsosGiro90Der = " + String(val);
    } else if (clave == "G90I" || clave == "P90I" || clave == "GIRO_IZQ") {
        uint16_t val = valorStr.toInt();
        configRobot.pulsosGiro90Izq = val;
        pref.putUShort("g90i", val);
        resp = "ACK: pulsosGiro90Izq = " + String(val);
    } else if (clave == "G180" || clave == "G180I" || clave == "P180I" || clave == "GIRO_180") {
        uint16_t val = valorStr.toInt();
        configRobot.pulsosGiro180Izq = val;
        pref.putUShort("g180i", val);
        resp = "ACK: pulsosGiro180Izq = " + String(val);
    } else if (clave == "G180D" || clave == "P180D") {
        uint16_t val = valorStr.toInt();
        configRobot.pulsosGiro180Der = val;
        pref.putUShort("g180d", val);
        resp = "ACK: pulsosGiro180Der = " + String(val);
    } else if (clave == "VD" || clave == "VEL_DER") {
        uint8_t val = valorStr.toInt();
        configRobot.velBaseDer = val;
        pref.putUChar("vd", val);
        resp = "ACK: velBaseDer = " + String(val);
    } else if (clave == "VI" || clave == "VEL_IZQ") {
        uint8_t val = valorStr.toInt();
        configRobot.velBaseIzq = val;
        pref.putUChar("vi", val);
        resp = "ACK: velBaseIzq = " + String(val);
    } else if (clave == "VGD" || clave == "VEL_G_DER") {
        uint8_t val = valorStr.toInt();
        configRobot.velBaseDerGiro = val;
        pref.putUChar("vgd", val);
        resp = "ACK: velBaseDerGiro = " + String(val);
    } else if (clave == "VGI" || clave == "VEL_G_IZQ") {
        uint8_t val = valorStr.toInt();
        configRobot.velBaseIzqGiro = val;
        pref.putUChar("vgi", val);
        resp = "ACK: velBaseIzqGiro = " + String(val);
    } else if (clave == "V180D" || clave == "VEL_180_DER") {
        uint8_t val = valorStr.toInt();
        configRobot.velBaseDerGiro180 = val;
        pref.putUChar("v180d", val);
        resp = "ACK: velBaseDerGiro180 = " + String(val);
    } else if (clave == "V180I" || clave == "VEL_180_IZQ") {
        uint8_t val = valorStr.toInt();
        configRobot.velBaseIzqGiro180 = val;
        pref.putUChar("v180i", val);
        resp = "ACK: velBaseIzqGiro180 = " + String(val);
    } else {
        resp = "ERROR: Clave desconocida '" + clave + "'. Envie VER para listar parametros.";
    }

    pref.end();
    return resp;
}

String obtenerReporteCalibracion() {
    String r = "=== CALIBRACION ACTUAL ===\n";
    r += "KP: " + String(configRobot.kp, 3) + "\n";
    r += "Pulsos Pregiro -> DER: " + String(configRobot.pulsosPregiroDer) + " | IZQ: " + String(configRobot.pulsosPregiroIzq) + "\n";
    r += "Pulsos Giro 90 -> DER: " + String(configRobot.pulsosGiro90Der) + " | IZQ: " + String(configRobot.pulsosGiro90Izq) + "\n";
    r += "Pulsos Giro 180 -> IZQ: " + String(configRobot.pulsosGiro180Izq) + " | DER: " + String(configRobot.pulsosGiro180Der) + "\n";
    r += "Pulsos Postgiro: " + String(configRobot.pulsosPostgiro) + "\n";
    r += "Velocidad Avance -> DER: " + String(configRobot.velBaseDer) + " | IZQ: " + String(configRobot.velBaseIzq) + "\n";
    r += "Velocidad Giro 90 -> DER: " + String(configRobot.velBaseDerGiro) + " | IZQ: " + String(configRobot.velBaseIzqGiro) + "\n";
    r += "Velocidad Giro 180 -> DER: " + String(configRobot.velBaseDerGiro180) + " | IZQ: " + String(configRobot.velBaseIzqGiro180) + "\n";
    r += "==========================";
    return r;
}
