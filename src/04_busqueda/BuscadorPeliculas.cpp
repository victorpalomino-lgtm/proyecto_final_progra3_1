#include "04_busqueda/BuscadorPeliculas.h"

#include <stdexcept>

BuscadorPeliculas::BuscadorPeliculas(const MovieTrie& arbol, const std::vector<RawMovie>& peliculas,
                                     std::unique_ptr<EstrategiaBusqueda> inicial)
    : arbol(arbol), peliculas(peliculas), estrategia(std::move(inicial)) {
}

void BuscadorPeliculas::setEstrategia(std::unique_ptr<EstrategiaBusqueda> nueva) {
    estrategia = std::move(nueva);
}

bool BuscadorPeliculas::tieneEstrategia() const {
    return estrategia != nullptr;
}

std::vector<Resultado> BuscadorPeliculas::buscar(const std::string& consulta) const {
    if (!estrategia) throw std::logic_error("BuscadorPeliculas: no hay estrategia de busqueda");
    return estrategia->ejecutar(consulta, arbol, peliculas);
}

std::string BuscadorPeliculas::descripcion(const std::string& consulta) const {
    if (!estrategia) throw std::logic_error("BuscadorPeliculas: no hay estrategia de busqueda");
    return estrategia->descripcion(consulta);
}
