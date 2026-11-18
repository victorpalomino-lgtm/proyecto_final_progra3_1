#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "03_arbol_insercion/MovieTrie(arbol).h"
#include "04_busqueda/BuscadorPeliculas.h"
#include "04_busqueda/Busqueda.h"
#include "04_busqueda/DecoradorBusqueda.h"
#include "05_interfaz_integracion/ListasUsuario.h"
#include "05_interfaz_integracion/ObserverLista.h"

// assert() desaparece con NDEBUG, y CMake compila en Release con -DNDEBUG: la prueba no
// verificaba nada. VERIFICAR comprueba siempre y, si falla, muestra donde y termina con 1.
#define VERIFICAR(condicion)                                                              \
    do {                                                                                   \
        if (!(condicion)) {                                                                \
            std::cerr << __FILE__ << ":" << __LINE__ << ": fallo VERIFICAR(" #condicion ")" \
                      << std::endl;                                                        \
            std::exit(1);                                                                  \
        }                                                                                  \
    } while (false)

namespace {

std::vector<RawMovie> crearCatalogoPrueba() {
    std::vector<RawMovie> peliculas(4);
    peliculas[0]= {"1999", "Matrix", "Sci-Fi", "Lana Wachowski", "Keanu Reeves", "Action", "http://1","Simulated reality"};
    peliculas[1]= {"2003", "Matrix Reloaded", "Sci-Fi", "Lana Wachowski", "Keanu Reeves", "Action", "http://2","Zion defense"};
    peliculas[2]= {"2021", "Matrix Resurrections", "Sci-Fi", "Lana Wachowski", "Keanu Reeves", "Action", "http://3","Return to Matrix"};
    peliculas[3]= {"2014", "John Wick", "Action", "Chad Stahelski", "Keanu Reeves", "Action", "http://4","Assassin vengeance"};
    return peliculas;
}

void probarDecorator() {
    auto peliculas = crearCatalogoPrueba();
    MovieTrie arbol;
    arbol.buildIndex(peliculas);

    BuscadorPeliculas buscador(arbol, peliculas);

    //Busqueda base sin decorar: matrix devuelve las 3 entregas
    buscador.setEstrategia(std::make_unique<BusquedaTexto>());
    auto base = buscador.buscar("matrix");
    VERIFICAR(base.size() == 3);

    //Decorador simple: filtrar por rango de anios [1995, 2005] -> quedan 2
    auto porAnio = std::make_unique<FiltroAnioBusqueda>(
        std::make_unique<BusquedaTexto>(), 1995, 2005);
    buscador.setEstrategia(std::move(porAnio));
    auto resAnio= buscador.buscar("matrix");
    VERIFICAR(resAnio.size()==2);

    //Decoradores apilados: BusquedaTexto -> FiltroAnio [1995, 2025] -> Excluir Matrix (1999) -> Top 1
    std::unordered_set<std::string> excluidas= {ListasUsuario::clave(peliculas[0])};
    auto decorada = std::make_unique<FiltroLimiteBusqueda>(
        std::make_unique<FiltroExcluirClavesBusqueda>(
            std::make_unique<FiltroAnioBusqueda>(
                std::make_unique<BusquedaTexto>(), 1995, 2025),
            excluidas, "vistas"),
        1);

    buscador.setEstrategia(std::move(decorada));
    auto resFinal = buscador.buscar("matrix");
    VERIFICAR(resFinal.size()==1);
    VERIFICAR(resFinal[0].pelicula->title!= "Matrix");

    std::string desc = buscador.descripcion("matrix");
    VERIFICAR(desc.find("[Anio: 1995-2025]")!= std::string::npos);
    VERIFICAR(desc.find("[Sin vistas]")!= std::string::npos);
    VERIFICAR(desc.find("[Top 1]")!= std::string::npos);
}

void probarObserver() {
    const std::string rutaTmp= "test_observer_listas.tmp";
    std::remove(rutaTmp.c_str());

    ListasUsuario listas(rutaTmp);
    CacheRecomendadorObserver cacheObs;
    AuditorListasObserver auditorObs;

    listas.agregarObservador(&cacheObs);
    listas.agregarObservador(&auditorObs);

    cacheObs.marcarActualizada();
    VERIFICAR(cacheObs.estaActualizada());

    RawMovie m1{"1999", "Matrix", "Sci-Fi", "Wachowski", "Keanu", "Action", "http://1", "Plot"};
    std::string k1= ListasUsuario::clave(m1);

    // Agregar Like invalida cache y registra en auditoria
    listas.alternarLike(k1);
    VERIFICAR(!cacheObs.estaActualizada());
    VERIFICAR(cacheObs.totalInvalidaciones()== 1);
    VERIFICAR(auditorObs.historialEventos().size()== 1);

    // Agregar Ver mas tarde NO invalida cache de recomendaciones, pero SI audita
    cacheObs.marcarActualizada();
    listas.alternarVerMasTarde(k1);
    VERIFICAR(cacheObs.estaActualizada());
    VERIFICAR(auditorObs.historialEventos().size()== 2);

    // Desuscribir observador
    listas.eliminarObservador(&auditorObs);
    listas.alternarLike(k1); // quita Like
    VERIFICAR(cacheObs.totalInvalidaciones()== 2);
    VERIFICAR(auditorObs.historialEventos().size()== 2);

    std::remove(rutaTmp.c_str());
}

} // namespace

int main() {
    probarDecorator();
    probarObserver();
    std::cout << "Patrones34Tests (Decorator y Observer): OK" << std::endl;
    return 0;
}