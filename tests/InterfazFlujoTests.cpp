// Prueba de integracion de la Interfaz con entrada guionada (std::cin) y salida capturada
// (std::cout). Comprueba que los patrones se usan de verdad en la aplicacion:
// - Decorator: [X] en los resultados envuelve la busqueda en FiltroExcluirClavesBusqueda y
//   oculta las peliculas guardadas; sin peliculas guardadas la opcion no aparece.
// - Observer: dar Like notifica a la cache de recomendaciones de la Interfaz y la pantalla
//   de inicio pasa a mostrar recomendaciones.

#include "03_arbol_insercion/MovieTrie(arbol).h"
#include "05_interfaz_integracion/Interfaz.h"

#include <cstdio>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

RawMovie crearPelicula(const std::string& titulo, const std::string& director, const std::string& genero,
                       const std::string& sinopsis) {
    RawMovie pelicula;
    pelicula.title = titulo;
    pelicula.releaseYear = "2000";
    pelicula.director = director;
    pelicula.genre = genero;
    pelicula.plot = sinopsis;
    pelicula.wikiPage = "https://example.org/" + titulo;
    return pelicula;
}

size_t contar(const std::string& texto, const std::string& buscado) {
    size_t veces = 0;
    for (size_t pos = texto.find(buscado); pos != std::string::npos; pos = texto.find(buscado, pos + 1)) {
        veces++;
    }
    return veces;
}

// Ejecuta la Interfaz con la entrada dada y devuelve todo lo que imprimio.
std::string ejecutar(const std::vector<RawMovie>& peliculas, const MovieTrie& arbol, const std::string& ruta,
                     const std::string& entrada) {
    std::istringstream in(entrada);
    std::ostringstream out;
    std::streambuf* cinOriginal = std::cin.rdbuf(in.rdbuf());
    std::streambuf* coutOriginal = std::cout.rdbuf(out.rdbuf());
    try {
        Interfaz interfaz(peliculas, arbol, ruta);
        interfaz.ejecutar();
    } catch (...) {
        std::cin.rdbuf(cinOriginal);
        std::cout.rdbuf(coutOriginal);
        throw;
    }
    std::cin.rdbuf(cinOriginal);
    std::cout.rdbuf(coutOriginal);
    return out.str();
}

} // namespace

int main() {
    const std::string ruta = (std::filesystem::temp_directory_path() / "interfaz_flujo_prueba.txt").string();
    std::remove(ruta.c_str());
    try {
        std::vector<RawMovie> peliculas;
        peliculas.push_back(crearPelicula("The Ship", "Ana Lopez", "drama", "A ship at sea."));
        peliculas.push_back(crearPelicula("Ghost Ship", "Ana Lopez", "drama", "A ghost ship."));
        peliculas.push_back(crearPelicula("Harbor", "Juan Perez", "comedy", "Ships everywhere."));
        peliculas.push_back(crearPelicula("Silent", "Ana Lopez", "drama", "Nothing."));
        MovieTrie arbol;
        arbol.buildIndex(peliculas);

        // 1) Buscar "ship" sin nada guardado: 3 resultados y sin opcion [X].
        //    Ver la primera (Ghost Ship), darle Like y volver: ahora si aparece [X].
        //    [X] oculta la guardada (2 resultados); [X] otra vez vuelve a mostrar las 3.
        // 2) Buscar "ghost": su unico resultado ya esta guardado; con [X] se avisa y se
        //    vuelve a la lista completa.
        const std::string entrada =
            "1\nship\n"
            "1\nL\nR\n"
            "X\n"
            "X\n"
            "M\n"
            "1\nghost\nX\n\nM\n"
            "0\n";
        const std::string salida = ejecutar(peliculas, arbol, ruta, entrada);

        const size_t like = salida.find("Le dio Like a esta pelicula.");
        const size_t primeraX = salida.find("[X] Ocultar");
        require(like != std::string::npos, "Dar Like desde el detalle");
        require(primeraX != std::string::npos && primeraX > like,
                "La opcion [X] solo aparece cuando hay peliculas guardadas");

        require(contar(salida, "RESULTADOS PARA \"ship\" [Sin guardadas]") == 1,
                "Decorator: la busqueda filtrada se describe con [Sin guardadas]");
        require(salida.find("2 peliculas encontradas") != std::string::npos,
                "Decorator: el filtro oculta la pelicula con Like");
        require(salida.find("[X] Mostrar tambien las guardadas") != std::string::npos,
                "Con el filtro activo se ofrece volver a mostrar todas");
        require(contar(salida, "3 peliculas encontradas") >= 2, "Quitar el filtro vuelve a mostrar las 3");
        require(salida.find("Todas las peliculas de \"ghost\" [Sin guardadas] ya estan guardadas.") !=
                    std::string::npos,
                "Si el filtro deja la lista vacia se avisa");

        // Observer: antes del Like no hay recomendaciones; despues, al volver al menu, si.
        require(salida.find("De Like a una pelicula para recibir recomendaciones.") < like,
                "Sin Likes no hay recomendaciones");
        require(salida.find(" - Silent", like) != std::string::npos,
                "Observer: el Like invalida la cache y se recalculan las recomendaciones");

        // Sin nada guardado, la pantalla de resultados es la de siempre (sin [X]).
        std::remove(ruta.c_str());
        const std::string limpia = ejecutar(peliculas, arbol, ruta, "1\nship\nM\n0\n");
        require(limpia.find("[X]") == std::string::npos, "Por defecto la pantalla de resultados no cambia");
        require(limpia.find("3 peliculas encontradas") != std::string::npos, "Por defecto se muestran todas");

        std::remove(ruta.c_str());
        std::cout << "Pruebas de flujo de la interfaz (Decorator y Observer) correctas." << std::endl;
    } catch (const std::exception& error) {
        std::remove(ruta.c_str());
        std::cerr << error.what() << std::endl;
        return 1;
    }
    return 0;
}
