// Prueba de integracion de la Interfaz con entrada guionada (std::cin) y salida capturada
// (std::cout). Comprueba que los patrones se usan de verdad en la aplicacion:
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

        // Buscar "ship", ver la primera (Ghost Ship), darle Like y volver al menu.
        const std::string salida = ejecutar(peliculas, arbol, ruta, "1\nship\n1\nL\nR\nM\n0\n");

        const size_t like = salida.find("Le dio Like a esta pelicula.");
        require(like != std::string::npos, "Dar Like desde el detalle");

        // Observer: antes del Like no hay recomendaciones; despues, al volver al menu, si.
        require(salida.find("De Like a una pelicula para recibir recomendaciones.") < like,
                "Sin Likes no hay recomendaciones");
        require(salida.find(" - Silent", like) != std::string::npos,
                "Observer: el Like invalida la cache y se recalculan las recomendaciones");

        std::remove(ruta.c_str());
        std::cout << "Pruebas de flujo de la interfaz (Observer) correctas." << std::endl;
    } catch (const std::exception& error) {
        std::remove(ruta.c_str());
        std::cerr << error.what() << std::endl;
        return 1;
    }
    return 0;
}
