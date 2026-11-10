#include "03_arbol_insercion/IndiceGlobal.h"

#include <stdexcept>

IndiceGlobal& IndiceGlobal::instancia() {
    static IndiceGlobal unica; // inicializacion thread-safe garantizada desde C++11
    return unica;
}

bool IndiceGlobal::construir(std::vector<RawMovie> peliculas) {
    bool estaLlamadaConstruyo = false;
    std::call_once(unaVez, [&]() {
        try {
            datos = std::move(peliculas);
            trie.buildIndex(datos); // los IDs del arbol son posiciones de "datos"
        } catch (...) {
            datos.clear(); // no dejar un vector sin su arbol; call_once permite reintentar
            throw;
        }
        construido.store(true, std::memory_order_release);
        estaLlamadaConstruyo = true;
    });
    return estaLlamadaConstruyo;
}

const MovieTrie& IndiceGlobal::arbol() const {
    if (!estaConstruido()) throw std::logic_error("IndiceGlobal: el arbol aun no se construyo");
    return trie;
}

const std::vector<RawMovie>& IndiceGlobal::peliculas() const {
    if (!estaConstruido()) throw std::logic_error("IndiceGlobal: el indice aun no se construyo");
    return datos;
}

bool IndiceGlobal::estaConstruido() const {
    return construido.load(std::memory_order_acquire);
}
