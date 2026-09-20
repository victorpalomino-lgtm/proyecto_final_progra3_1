#pragma once

#include <iostream>
#include <string>
#include <vector>
#include "RawMovie.h"
#include "MovieTrie(arbol).h"

// pelicula encontrada, lista para mostrarse en la tabla de resultados
struct Resultado {
    size_t id = 0; // posicion de la pelicula en el vector
    int prioridad = 0; // las de mayor prioridad se muestran primero
    std::string tituloOrden; // titulo en minusculas, solo sirve para ordenar
    const RawMovie* pelicula = nullptr;
    bool operator<(const Resultado& otro) const;
};

std::ostream& operator<<(std::ostream& os, const Resultado& resultado);


class Busqueda {
protected:
    std::string texto; // lo que escribio el usuario
    std::string textoNormalizado; // el mismo texto limpio y en minusculas

    // arma el resultado de una pelicula con su titulo listo para ordenar
    Resultado crearResultado(size_t id, const RawMovie& pelicula) const;

public:
    explicit Busqueda(const std::string& texto);
    virtual ~Busqueda() = default;

    // Devuelve las peliculas encontradas, ya ordenadas
    virtual std::vector<Resultado> ejecutar(const MovieTrie& arbol, const std::vector<RawMovie>& peliculas) const = 0;
    // Texto que se muestra en la pantalla de resultados
    virtual std::string descripcion() const = 0;

    const std::string& getTexto() const;
};

// Palabra, frase o parte de una palabra en cualquier campo que indexa el arbol
class BusquedaTexto : public Busqueda {
public:
    explicit BusquedaTexto(const std::string& texto);
    std::vector<Resultado> ejecutar(const MovieTrie& arbol, const std::vector<RawMovie>& peliculas) const override;
    std::string descripcion() const override;
};

// Busqueda por tag: el texto debe aparecer en un campo especifico (director, reparto o genero)
class BusquedaTag : public Busqueda {
private:
    FieldType campo; // enum definido en MovieTrie(arbol).h

    const std::string& valorCampo(const RawMovie& pelicula) const;

public:
    BusquedaTag(FieldType campo, const std::string& texto);
    std::vector<Resultado> ejecutar(const MovieTrie& arbol, const std::vector<RawMovie>& peliculas) const override;
    std::string descripcion() const override;

    static std::string nombreCampo(FieldType campo);
};
