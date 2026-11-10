#pragma once

#include <memory>
#include <string>
#include <vector>
#include "comun/RawMovie.h"
#include "03_arbol_insercion/MovieTrie(arbol).h"
#include "04_busqueda/EstrategiaBusqueda.h"

// Patron Strategy: Contexto.
// Guarda la estrategia activa y delega en ella la busqueda. Quien lo usa (la Interfaz)
// solo conoce EstrategiaBusqueda; la estrategia se puede cambiar en tiempo de ejecucion.
// El arbol y las peliculas se inyectan por referencia: en la aplicacion vienen del
// Singleton IndiceGlobal y en las pruebas pueden ser un Trie local.
class BuscadorPeliculas {
private:
    const MovieTrie& arbol;
    const std::vector<RawMovie>& peliculas;
    std::unique_ptr<EstrategiaBusqueda> estrategia;

public:
    BuscadorPeliculas(const MovieTrie& arbol, const std::vector<RawMovie>& peliculas,
                      std::unique_ptr<EstrategiaBusqueda> inicial = nullptr);

    void setEstrategia(std::unique_ptr<EstrategiaBusqueda> nueva);
    bool tieneEstrategia() const;

    // Lanzan std::logic_error si no hay estrategia activa
    std::vector<Resultado> buscar(const std::string& consulta) const;
    std::string descripcion(const std::string& consulta) const;
};
