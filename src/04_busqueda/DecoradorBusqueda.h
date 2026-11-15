#ifndef PROYECTO_PROGRA3_1_DECORADORBUSQUEDA_H
#define PROYECTO_PROGRA3_1_DECORADORBUSQUEDA_H
#pragma once

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>
#include "04_busqueda/EstrategiaBusqueda.h"

//Patron Decorator: Clase base abstracta para envolver cualquier EstrategiaBusqueda
//Permite apilar filtros o transformaciones sobre los resultados sin modificar
//BusquedaTexto, BusquedaTag ni BuscadorPeliculas.
class DecoradorBusqueda: public EstrategiaBusqueda {
protected:
    std::unique_ptr<EstrategiaBusqueda> envuelta;

public:
    explicit DecoradorBusqueda(std::unique_ptr<EstrategiaBusqueda> estrategia);

    std::vector<Resultado> ejecutar(const std::string& consulta, const MovieTrie& arbol, const std::vector<RawMovie>& peliculas) const override;

    std::string descripcion(const std::string& consulta) const override;
};

//Decorador concreto 1: filtra peliculas cuyo anio de estreno este en [anioMin, anioMax]
class FiltroAnioBusqueda: public DecoradorBusqueda {
private:
    int anioMin;
    int anioMax;

public:
    FiltroAnioBusqueda(std::unique_ptr<EstrategiaBusqueda> estrategia, int anioMin, int anioMax);

    std::vector<Resultado> ejecutar(const std::string& consulta, const MovieTrie& arbol, const std::vector<RawMovie>& peliculas) const override;

    std::string descripcion(const std::string& consulta) const override;
};

//Decorador concreto 2: excluye peliculas cuyas claves esten en un conjunto (ej. ya vistas o en Ver mas tarde)
class FiltroExcluirClavesBusqueda: public DecoradorBusqueda {
private:
    std::unordered_set<std::string> clavesExcluidas;
    std::string etiqueta;

public:
    FiltroExcluirClavesBusqueda(std::unique_ptr<EstrategiaBusqueda> estrategia, std::unordered_set<std::string> clavesExcluidas, std::string etiqueta= "excluidas");

    std::vector<Resultado> ejecutar(const std::string& consulta, const MovieTrie& arbol, const std::vector<RawMovie>& peliculas) const override;

    std::string descripcion(const std::string& consulta) const override;
};

//Decorador concreto 3: limita el numero maximo de resultados devueltos
class FiltroLimiteBusqueda: public DecoradorBusqueda {
private:
    size_t limite;

public:
    FiltroLimiteBusqueda(std::unique_ptr<EstrategiaBusqueda> estrategia, size_t limite);

    std::vector<Resultado> ejecutar(const std::string& consulta, const MovieTrie& arbol, const std::vector<RawMovie>& peliculas) const override;

    std::string descripcion(const std::string& consulta) const override;
};

#endif //PROYECTO_PROGRA3_1_DECORADORBUSQUEDA_H