#include "main.h"
#include <Arduino.h>
#include "hardware/logger/logger.h"
#include "hardware/sensoresDistancia/sensoresDistancia.h"
#include "hardware/sensorPiso/sensorPiso.h"
#include "config.h"
#include "hardware/movimiento/puenteH.h"
#include "memoria/funcionesMapeo.h"

//====================
//  FUNCIONES DEL MAIN
//====================
void actualizarMapaConLaLecturaDeLosSensores();


//====================
// VARIABLES GLOBALES
//====================

sensado lecturaActual = {0,0,0};
ORIENTACION orientacionActual = NORTE;
POSICION posicionActual = {X_START,Y_START, NORTE};
POSICION posicionContigua = {(X_START), (Y_START - 1), NORTE};
//Las posiciones de MOVIMIENTOS son ABSOLUTAS, es realmente lo que están haciendo los motores
MOVIMIENTOS movimientoAnterior = FRENO_F;
ESTADOS_ETAPA_1 estadoActual = PARTIDA;

void setup(){
  inicializacionSensoresDist();
  inicializarLogger();
}

void loop(){
  switch (estadoActual){
    case PARTIDA: {
      actualizarSensado();
      actualizarMapaConLaLecturaDeLosSensores();
      estadoActual = ENTRE_CELDAS;
      break;
    };
    case ENTRE_CELDAS: {
      if(cambioDeCelda()){
        estadoActual = FIN_CELDA;
      } else {
        estadoActual = ENTRE_CELDAS;
      }
      break;
    };
    case FIN_CELDA: {
        movimientoAnterior = leerAccion();
        actualizarPosicion();
        actualizarMapaConLaLecturaDeLosSensores();
      break;
    };
    case FIN:{
      break;
    };
  }
}

void actualizarMapaConLaLecturaDeLosSensores(){

  //NECESITO CONSULTAR ESTA FUNCIÓN DEBIDO A QUE ME RESULTA UNA CONCATENACIÓN HORRIBLE DE IFS
  //ADEMÁS TENGO QUE HACER VARIAS DE ESTAS, PORQUE PARA CAMBIAR LA ORIENTACIÓN VA A HABER UNA IG
  //Y PARA CAMBIAR LA POSICIÓN OTRA MÁS. 
  //NO SE QUE HACER
  //Hacerlo con Switch cases sería raro tb porque tengo que hacer mil quinientos casos (en realidad 32, son finitos, JAJASJAJAJAJA)
  actualizarSensado();
  if (posicionActual.orientacion == NORTE){
      if(lecturaActual.distanciaCent < UMBRAL_LECTURA){
        paredNorte(posicionActual.X, posicionActual.Y); 
      }
      if(lecturaActual.distanciaDer < UMBRAL_LECTURA){
        paredEste(posicionActual.X, posicionActual.Y);
      }
      if(lecturaActual.distanciaIzq < UMBRAL_LECTURA){
        paredOeste(posicionActual.X, posicionActual.Y);
    }
  } else if(posicionActual.orientacion == SUR){
      if(lecturaActual.distanciaCent < UMBRAL_LECTURA){
        paredSur(posicionActual.X, posicionActual.Y); 
      }
      if(lecturaActual.distanciaDer < UMBRAL_LECTURA){
        paredOeste(posicionActual.X, posicionActual.Y);
      }
      if(lecturaActual.distanciaIzq < UMBRAL_LECTURA){
        paredEste(posicionActual.X, posicionActual.Y);
      }
  } else if(posicionActual.orientacion == ESTE){
      if(lecturaActual.distanciaCent < UMBRAL_LECTURA){
        paredEste(posicionActual.X, posicionActual.Y); 
      }
     if(lecturaActual.distanciaDer < UMBRAL_LECTURA){
        paredSur(posicionActual.X, posicionActual.Y);
      }
      if(lecturaActual.distanciaIzq < UMBRAL_LECTURA){
        paredNorte(posicionActual.X, posicionActual.Y);
      }
  } else if(posicionActual.orientacion == OESTE){
     if(lecturaActual.distanciaCent < UMBRAL_LECTURA){
        paredOeste(posicionActual.X, posicionActual.Y); 
      }
      if(lecturaActual.distanciaDer < UMBRAL_LECTURA){
        paredNorte(posicionActual.X, posicionActual.Y);
      }
      if(lecturaActual.distanciaIzq < UMBRAL_LECTURA){
        paredSur(posicionActual.X, posicionActual.Y);
     }
  }
} /*Fin de la función*/