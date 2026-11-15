#include "04_busqueda/DecoradorBusqueda.h"

#include <stdexcept>
#include <utility>

DecoradorBusqueda::DecoradorBusqueda(std::unique_ptr<EstrategiaBusqueda> estrategia): envuelta(std::move(estrategia)) {
    if (!envuelta) {
        throw std::invalid_argument("DecoradorBusqueda requiere una estrategia valida");
    }
}

std::vector<Resultado> DecoradorBusqueda::ejecutar(const std::string& consulta, const MovieTrie& arbol,
                                                   const std::vector<RawMovie>& peliculas) const {
    return envuelta->ejecutar(consulta, arbol, peliculas);
}

std::string DecoradorBusqueda::descripcion(const std::string& consulta) const {
    return envuelta->descripcion(consulta);
}

//FiltroAnioBusqueda

FiltroAnioBusqueda::FiltroAnioBusqueda(std::unique_ptr<EstrategiaBusqueda> estrategia,int anioMin, int anioMax)
    :DecoradorBusqueda(std::move(estrategia)), anioMin(anioMin), anioMax(anioMax) {}

std::vector<Resultado> FiltroAnioBusqueda::ejecutar(const std::string& consulta, const MovieTrie& arbol,
                                                    const std::vector<RawMovie>& peliculas) const {
    std::vector<Resultado> base= DecoradorBusqueda::ejecutar(consulta, arbol, peliculas);
    std::vector<Resultado> filtrados;
    filtrados.reserve(base.size());

    for (const Resultado& r: base) {
        if (!r.pelicula) continue;
        try {
            int anio= std::stoi(r.pelicula->releaseYear);
            if (anio>= anioMin && anio<= anioMax) {
                filtrados.push_back(r);
            }
        } catch (...) {
            //si releaseYear no es numerico valido, se omite del filtro por anio
        }
    }
    return filtrados;
}

std::string FiltroAnioBusqueda::descripcion(const std::string& consulta) const {
    return DecoradorBusqueda::descripcion(consulta) +" [Anio: " + std::to_string(anioMin) +"-"+
        std::to_string(anioMax) +"]";
}

//FiltroExcluirClavesBusqueda

FiltroExcluirClavesBusqueda::FiltroExcluirClavesBusqueda(
    std::unique_ptr<EstrategiaBusqueda> estrategia,
    std::unordered_set<std::string> clavesExcluidas,
    std::string etiqueta): DecoradorBusqueda(std::move(estrategia)), clavesExcluidas(std::move(clavesExcluidas)),
      etiqueta(std::move(etiqueta)) {}

std::vector<Resultado> FiltroExcluirClavesBusqueda::ejecutar(
    const std::string& consulta,
    const MovieTrie& arbol,
    const std::vector<RawMovie>& peliculas) const {
    std::vector<Resultado> base= DecoradorBusqueda::ejecutar(consulta, arbol, peliculas);
    std::vector<Resultado> filtrados;
    filtrados.reserve(base.size());

    for (const Resultado& r: base) {
        if (!r.pelicula) continue;
        std::string k= r.pelicula->title +'\t'+ r.pelicula->releaseYear +'\t'+ r.pelicula->wikiPage;
        if (clavesExcluidas.count(k)== 0) {
            filtrados.push_back(r);
        }
    }
    return filtrados;
}

std::string FiltroExcluirClavesBusqueda::descripcion(const std::string& consulta) const {
    return DecoradorBusqueda::descripcion(consulta) +" [Sin "+ etiqueta +"]";
}

//FiltroLimiteBusqueda

FiltroLimiteBusqueda::FiltroLimiteBusqueda(std::unique_ptr<EstrategiaBusqueda> estrategia, size_t limite):
DecoradorBusqueda(std::move(estrategia)), limite(limite) {}

std::vector<Resultado> FiltroLimiteBusqueda::ejecutar(const std::string& consulta, const MovieTrie& arbol,
                                                      const std::vector<RawMovie>& peliculas) const {
    std::vector<Resultado> base= DecoradorBusqueda::ejecutar(consulta, arbol, peliculas);
    if (base.size()> limite) {
        base.resize(limite);
    }
    return base;
}

std::string FiltroLimiteBusqueda::descripcion(const std::string& consulta) const {
    return DecoradorBusqueda::descripcion(consulta) +" [Top "+ std::to_string(limite)+"]";
}