#pragma once

#include <string>
#include <vector>
#include "comun/RawMovie.h"
#include "03_arbol_insercion/MovieTrie(arbol).h"
#include "04_busqueda/Busqueda.h"

// Interfaz de la plataforma en la terminal
//solo usa las funciones publicas de los demas modulos
class Interfaz {
private:
    const std::vector<RawMovie>& peliculas;
    const MovieTrie& arbol;

    void pantallaBusquedaTexto();
    void pantallaBusquedaTag();
    void mostrarResultados(const Busqueda& busqueda);
    void mostrarDetalle(const RawMovie& pelicula);
    static void mostrarCampo(const std::string& etiqueta, const std::string& valor);

public:
    Interfaz(const std::vector<RawMovie>& peliculas, const MovieTrie& arbol);

    // Menu principal
    void ejecutar();

    // funciones se usan en la pantalla de carga, cuando el arbol aun no existe
    static void prepararConsola();
    static void limpiarPantalla();
    static void mostrarLinea(char simbolo);
    static void mostrarCabecera();
    static void mostrarEtapa(const std::string& nombre, const std::string& detalle);
    static bool construirArbol(MovieTrie& arbol, const std::vector<RawMovie>& peliculas);
    static bool leerLinea(const std::string& mensaje, std::string& linea);
    static void pausa(const std::string& mensaje);
};
