/* 
===========================================
    Máscaras para mapeo de bits  -- HEADER
===========================================
    Autor: Martín Zonis
    Fecha: 2026 - 05 - 04
    Descripcion: Este header es el prototipado de las funciones que permiten cargar información al mapa.
*/

#ifndef MASCARAS_MAPEO_
#define MASCARAS_MAPEO_
#include <stdint.h>
#include "config.h"

enum ORIENTACION{
    NADA,
    NORTE,
    SUR,
    ESTE,
    OESTE,
};

struct POSICION{
    uint8_t X;
    uint8_t Y;
    uint8_t orientacion;
};

//Carga que la celda está visitada
    void celdaVisitada(uint8_t X, uint8_t Y);
    //Carga que la celda está bloqueada (es decir que fue visitada más de una vez)
    void celdaBloqueada(uint8_t X, uint8_t Y);
    //Carga que la celda está desbloqueada
    void celdaDesbloqueada(uint8_t X, uint8_t Y);
    //Carga que la celda es la posición inicial
    void posicionInicial(uint8_t X, uint8_t Y);
    //Carga que la celda es la posición final
    void posicionFinal(uint8_t X, uint8_t Y);
    //Carga que la celda tiene una pared al norte
    void paredNorte(uint8_t X, uint8_t Y);
    //Carga que la celda tiene una pared al sur
    void paredSur(uint8_t X, uint8_t Y);
    //Carga que la celda tiene una pared al este
    void paredEste(uint8_t X, uint8_t Y);
    //Carga que la celda tiene una pared al oeste
    void paredOeste(uint8_t X, uint8_t Y);


    bool askParedNorte(uint8_t X, uint8_t Y);
    bool askParedSur(uint8_t X, uint8_t Y);
    bool askParedEste(uint8_t X, uint8_t Y);
    bool askParedOeste(uint8_t X, uint8_t Y);  
    bool askCeldaVisitada(uint8_t X, uint8_t Y);
    bool askCeldaBloqueada(uint8_t X, uint8_t Y);
    bool askPosicionInicial(uint8_t X, uint8_t Y);
    bool askPosicionFinal(uint8_t X, uint8_t Y);

#endif /* MASCARAS_MAPEO_ */

