#include "05_interfaz_integracion/ListasUsuario.h"
#include "05_interfaz_integracion/Recomendador.h"

#include <cstdio>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

RawMovie crearPelicula(const std::string& titulo, const std::string& anio, const std::string& origen,
                       const std::string& director, const std::string& reparto, const std::string& genero) {
    RawMovie pelicula;
    pelicula.title = titulo;
    pelicula.releaseYear = anio;
    pelicula.origin = origen;
    pelicula.director = director;
    pelicula.cast = reparto;
    pelicula.genre = genero;
    pelicula.wikiPage = "https://en.wikipedia.org/wiki/" + titulo;
    return pelicula;
}

int main() {
    const std::string ruta = (std::filesystem::temp_directory_path() / "listas_usuario_prueba.txt").string();
    std::remove(ruta.c_str());

    try {
        std::vector<RawMovie> peliculas;
        peliculas.push_back(crearPelicula("Jaws", "1975", "American", "Steven Spielberg", "Roy Scheider, Robert Shaw", "thriller"));
        peliculas.push_back(crearPelicula("Duel", "1971", "American", "Steven Spielberg", "Dennis Weaver", "thriller"));
        peliculas.push_back(crearPelicula("Psycho", "1960", "American", "Alfred Hitchcock", "Anthony Perkins", "thriller"));
        peliculas.push_back(crearPelicula("Grease", "1978", "American", "Randal Kleiser", "John Travolta", "musical"));
        peliculas.push_back(crearPelicula("Jaws 2", "1978", "American", "Jeannot Szwarc", "Roy Scheider", "horror"));

        //listas
        const std::string jaws = ListasUsuario::clave(peliculas[0]);
        const std::string duel = ListasUsuario::clave(peliculas[1]);
        {
            ListasUsuario listas(ruta);
            require(listas.likes().empty() && listas.verMasTarde().empty(), "Empieza sin listas");
            require(listas.alternarLike(jaws), "Agregar like");
            require(listas.tieneLike(jaws) && !listas.estaEnVerMasTarde(jaws), "Like no toca ver mas tarde");
            require(listas.alternarVerMasTarde(duel) && listas.alternarVerMasTarde(jaws), "Agregar a ver mas tarde");
            require(listas.verMasTarde().size() == 2 && listas.verMasTarde()[0] == duel, "Guarda el orden");
            require(!listas.alternarVerMasTarde(duel), "Alternar otra vez la quita");
            require(listas.verMasTarde().size() == 1 && listas.verMasTarde()[0] == jaws, "Quitar de la lista");
        }
        {
            ListasUsuario cargadas(ruta);
            require(cargadas.likes().size() == 1 && cargadas.tieneLike(jaws), "Like se guarda en el archivo");
            require(cargadas.verMasTarde().size() == 1 && cargadas.estaEnVerMasTarde(jaws),
                    "Ver mas tarde se guarda en el archivo");
            require(!cargadas.estaEnVerMasTarde(duel), "Lo quitado no vuelve al cargar");
        }

        //recomendaciones
        require(Recomendador::recomendar(peliculas, {}).empty(), "Sin likes no hay recomendadas");

        std::vector<Resultado> recomendadas = Recomendador::recomendar(peliculas, {0});
        require(!recomendadas.empty(), "Con un like hay recomendadas");
        for (const Resultado& resultado : recomendadas) {
            require(resultado.id != 0, "No recomienda la pelicula que ya tiene like");
            require(resultado.id != 3, "No recomienda si no comparte director, reparto ni genero");
        }
        require(recomendadas[0].pelicula->title == "Duel", "Mismo director y genero va primero");
        require(recomendadas[1].pelicula->title == "Jaws 2", "Actor en comun gana a solo genero");
        require(recomendadas[2].pelicula->title == "Psycho", "Solo genero va al final");
        require(recomendadas[0].prioridad == Recomendador::PUNTOS_DIRECTOR + Recomendador::PUNTOS_GENERO
                + Recomendador::PUNTOS_ORIGEN + Recomendador::PUNTOS_EPOCA, "Puntaje de director y genero");

        std::vector<Resultado> conDos = Recomendador::recomendar(peliculas, {0, 3});
        bool aparecePsycho = false;
        for (const Resultado& resultado : conDos) {
            require(resultado.id != 0 && resultado.id != 3, "Excluye todas las que tienen like");
            aparecePsycho = aparecePsycho || resultado.id == 2;
        }
        require(aparecePsycho, "Suma lo de cada like");

        std::remove(ruta.c_str());
        std::cout << "Pruebas de listas y recomendaciones correctas." << std::endl;
    } catch (const std::exception& error) {
        std::remove(ruta.c_str());
        std::cerr << error.what() << std::endl;
        return 1;
    }
}
