#pragma once

#include <string>
#include <vector>
#include "comun/RawMovie.h"
#include "03_arbol_insercion/MovieTrie(arbol).h"
#include "04_busqueda/EstrategiaBusqueda.h"

// Estrategias concretas de busqueda (patron Strategy)

// Palabra, frase o parte de una palabra en cualquier campo que indexa el arbol
class BusquedaTexto : public EstrategiaBusqueda {
public:
    std::vector<Resultado> ejecutar(const std::string& consulta, const MovieTrie& arbol,
                                    const std::vector<RawMovie>& peliculas) const override;
    std::string descripcion(const std::string& consulta) const override;
};

// Busqueda por tag: el texto debe aparecer en un campo especifico (director, reparto o genero)
class BusquedaTag : public EstrategiaBusqueda {
private:
    FieldType campo; // enum definido en MovieTrie(arbol).h

    const std::string& valorCampo(const RawMovie& pelicula) const;

public:
    explicit BusquedaTag(FieldType campo);
    std::vector<Resultado> ejecutar(const std::string& consulta, const MovieTrie& arbol,
                                    const std::vector<RawMovie>& peliculas) const override;
    std::string descripcion(const std::string& consulta) const override;

    static std::string nombreCampo(FieldType campo);
};
