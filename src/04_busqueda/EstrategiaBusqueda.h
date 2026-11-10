#pragma once

#include <iostream>
#include <string>
#include <vector>
#include "comun/RawMovie.h"
#include "03_arbol_insercion/MovieTrie(arbol).h"

// pelicula encontrada, lista para mostrarse en la tabla de resultados
struct Resultado {
    size_t id = 0; // posicion de la pelicula en el vector
    int prioridad = 0; // las de mayor prioridad se muestran primero
    std::string tituloOrden; // titulo en minusculas, solo sirve para ordenar
    const RawMovie* pelicula = nullptr;
    bool operator<(const Resultado& otro) const;
};

std::ostream& operator<<(std::ostream& os, const Resultado& resultado);

// Patron Strategy: interfaz comun de los algoritmos de busqueda.
// Cada estrategia recibe la consulta por parametro y no guarda estado de la consulta,
// asi una misma estrategia se puede reutilizar para muchas busquedas.
// El arbol y las peliculas tambien llegan por parametro (inyeccion), para poder
// usar el Singleton IndiceGlobal en la aplicacion y Tries locales en las pruebas.
class EstrategiaBusqueda {
public:
    virtual ~EstrategiaBusqueda() = default;

    // Devuelve las peliculas encontradas, ya ordenadas
    virtual std::vector<Resultado> ejecutar(const std::string& consulta, const MovieTrie& arbol,
                                            const std::vector<RawMovie>& peliculas) const = 0;

    // Texto que se muestra en la pantalla de resultados
    virtual std::string descripcion(const std::string& consulta) const = 0;
};
