#pragma once

#include <atomic>
#include <mutex>
#include <vector>
#include "comun/RawMovie.h"
#include "03_arbol_insercion/MovieTrie(arbol).h"

// Patron Singleton (Meyers singleton): unico indice de peliculas de la aplicacion.
//
// Guarda juntos el MovieTrie y el vector de peliculas con el que se construyo, porque
// el arbol solo guarda posiciones del vector: si se separan, los IDs apuntarian a
// peliculas equivocadas. Por eso el vector se mueve aqui dentro y no se puede cambiar.
//
// Hilos:
//  - instancia() es segura para hilos: C++11 garantiza que una variable static local
//    se inicializa una sola vez aunque varios hilos llamen a la vez.
//  - construir() usa std::call_once: la indexacion ocurre una sola vez. Las llamadas
//    siguientes no hacen nada. Si la indexacion lanza una excepcion, el indice queda
//    vacio y se puede volver a intentar.
//  - Despues de construir, el estado es de solo lectura (arbol() y peliculas() devuelven
//    referencias const), asi que varios hilos pueden buscar a la vez sin locks.
//
// Pruebas: MovieTrie conserva su constructor publico. El Singleton es solo un contenedor
// y no impide crear Tries locales para inyectarlos (por ejemplo en Interfaz o en las
// estrategias de busqueda) con datos distintos en las pruebas.
class IndiceGlobal {
private:
    MovieTrie trie;
    std::vector<RawMovie> datos;
    std::once_flag unaVez;
    std::atomic<bool> construido{false};

    IndiceGlobal() = default;

public:
    IndiceGlobal(const IndiceGlobal&) = delete;
    IndiceGlobal& operator=(const IndiceGlobal&) = delete;
    IndiceGlobal(IndiceGlobal&&) = delete;
    IndiceGlobal& operator=(IndiceGlobal&&) = delete;

    static IndiceGlobal& instancia();

    // Indexa las peliculas solo la primera vez. Devuelve true si esta llamada
    // construyo el indice y false si ya estaba construido (no se reindexa).
    bool construir(std::vector<RawMovie> peliculas);

    // Lanzan std::logic_error si el indice todavia no se construyo.
    const MovieTrie& arbol() const;
    const std::vector<RawMovie>& peliculas() const;

    bool estaConstruido() const;
};
