#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "comun/RawMovie.h"
#include "05_interfaz_integracion/ListasUsuario.h"
#include "05_interfaz_integracion/ObserverLista.h"
#include "03_arbol_insercion/MovieTrie(arbol).h"
#include "03_arbol_insercion/IndiceGlobal.h"
#include "04_busqueda/Busqueda.h"
#include "04_busqueda/BuscadorPeliculas.h"

// Interfaz de la plataforma en la terminal
//solo usa las funciones publicas de los demas modulos
class Interfaz {
private:
    const std::vector<RawMovie>& peliculas;
    BuscadorPeliculas buscador; // contexto del patron Strategy
    ListasUsuario listas;
    std::unordered_map<std::string, size_t> idPorClave; //para ubicar en el vector las peliculas guardadas
    std::vector<Resultado> recomendadas;
    // Patron Observer: ListasUsuario le avisa cada cambio y la cache se invalida cuando
    // cambian los Likes, asi las recomendaciones se recalculan solo cuando hace falta.
    CacheRecomendadorObserver cacheRecomendadas;

    using FabricaEstrategia = std::function<std::unique_ptr<EstrategiaBusqueda>()>;

    void pantallaBusquedaTexto();
    void pantallaBusquedaTag();
    // Busca con la estrategia que crea "fabrica"; si el usuario lo pide, la envuelve en el
    // Decorator FiltroExcluirClavesBusqueda para ocultar las peliculas ya guardadas.
    void mostrarResultados(const std::string& consulta, const FabricaEstrategia& fabrica);
    // Devuelve true si el usuario pidio alternar el filtro de guardadas ([X]).
    bool mostrarLista(const std::string& titulo, const std::vector<Resultado>& resultados,
                      const std::string& tiempo, bool esBusqueda = false, bool filtroActivo = false);
    std::unordered_set<std::string> clavesGuardadas() const;
    void pantallaLista(const std::string& titulo, const std::vector<Resultado>& resultados,
                       const std::string& vacio);
    void mostrarResumen(const std::string& titulo, const std::vector<Resultado>& resultados,
                        const std::string& vacio);
    std::vector<Resultado> resultadosDe(const std::vector<std::string>& claves) const;
    const std::vector<Resultado>& obtenerRecomendadas();
    void mostrarDetalle(const RawMovie& pelicula);
    static void mostrarCampo(const std::string& etiqueta, const std::string& valor);
    static std::string rutaListas();

public:
    // Inyeccion explicita: permite usar un Trie y un vector propios (por ejemplo en pruebas)
    //si no se da la ruta de las listas se usa data/listas_usuario.txt
    Interfaz(const std::vector<RawMovie>& peliculas, const MovieTrie& arbol,
             const std::string& rutaListas = "");
    // Usa el Singleton IndiceGlobal, que ya debe estar construido
    Interfaz();
    ~Interfaz();
    // ListasUsuario guarda un puntero al observador de esta instancia: no se copia.
    Interfaz(const Interfaz&) = delete;
    Interfaz& operator=(const Interfaz&) = delete;

    // Menu principal
    void ejecutar();

    // funciones se usan en la pantalla de carga, cuando el arbol aun no existe
    static void prepararConsola();
    static void limpiarPantalla();
    static void mostrarLinea(char simbolo);
    static void mostrarCabecera();
    static void mostrarEtapa(const std::string& nombre, const std::string& detalle);
    static bool construirIndice(IndiceGlobal& indice, std::vector<RawMovie> peliculas);
    static bool leerLinea(const std::string& mensaje, std::string& linea);
    static void pausa(const std::string& mensaje);
};
