#pragma once

#include <string>
#include <vector>
#include "comun/RawMovie.h"
#include "03_arbol_insercion/MovieTrie(arbol).h"
#include "04_busqueda/EstrategiaBusqueda.h"

// Estrategias concretas de busqueda (patron Strategy)
//
// Paralelismo: los candidatos que devuelve el arbol se verifican y puntuan por bloques en
// varios hilos (cada hilo toma bloques de un contador atomico y llena su propio vector de
// resultados). Al final se unen y se ordenan con std::sort + Resultado::operator<, que es
// un orden total (prioridad, titulo, id): el resultado es identico con cualquier numero
// de hilos.
//  - hilos:  0 = std::thread::hardware_concurrency(), 1 = un solo hilo.
//  - umbral: con menos candidatos que esto se trabaja en un solo hilo (crear hilos cuesta
//            mas que lo que se ahorra). Valores por defecto medidos con bench/Benchmark.cpp.
// El arbol y las peliculas solo se leen, por eso no hacen falta locks.

// Palabra, frase o parte de una palabra en cualquier campo que indexa el arbol.
// Una frase encuentra las peliculas con cualquiera de sus palabras ("y/o"); primero las
// que tienen la frase contigua, luego las que tienen todas las palabras y despues el resto
// (ver prioridadTexto en BusquedaComun.h).
class BusquedaTexto : public EstrategiaBusqueda {
private:
    size_t hilos;
    size_t umbral;

public:
    static constexpr size_t UMBRAL_POR_DEFECTO = 32;

    explicit BusquedaTexto(size_t hilos = 0, size_t umbral = UMBRAL_POR_DEFECTO);
    std::vector<Resultado> ejecutar(const std::string& consulta, const MovieTrie& arbol,
                                    const std::vector<RawMovie>& peliculas) const override;
    std::string descripcion(const std::string& consulta) const override;
};

// Busqueda por tag: el texto debe aparecer en un campo especifico (director, reparto o genero)
class BusquedaTag : public EstrategiaBusqueda {
private:
    FieldType campo; // enum definido en MovieTrie(arbol).h
    size_t hilos;
    size_t umbral;

    const std::string& valorCampo(const RawMovie& pelicula) const;

public:
    static constexpr size_t UMBRAL_POR_DEFECTO = 512;

    explicit BusquedaTag(FieldType campo, size_t hilos = 0, size_t umbral = UMBRAL_POR_DEFECTO);
    std::vector<Resultado> ejecutar(const std::string& consulta, const MovieTrie& arbol,
                                    const std::vector<RawMovie>& peliculas) const override;
    std::string descripcion(const std::string& consulta) const override;

    static std::string nombreCampo(FieldType campo);
};
