#pragma once

#include <vector>
#include "comun/RawMovie.h"
#include "04_busqueda/EstrategiaBusqueda.h"

//busca peliculas parecidas a las que tienen like
//cada pelicula del catalogo suma puntos por lo que comparte con cada pelicula con like:
//mismo director +5, cada actor en comun +3, cada genero en comun +2,
//mismo origen +1 y estreno con 10 anios o menos de diferencia +1
class Recomendador {
public:
    static const int PUNTOS_DIRECTOR = 5;
    static const int PUNTOS_ACTOR = 3;
    static const int PUNTOS_GENERO = 2;
    static const int PUNTOS_ORIGEN = 1;
    static const int PUNTOS_EPOCA = 1;

    //devuelve las recomendadas de mayor a menor puntaje, sin las que ya tienen like
    static std::vector<Resultado> recomendar(const std::vector<RawMovie>& peliculas,
                                             const std::vector<size_t>& idsConLike);
};
