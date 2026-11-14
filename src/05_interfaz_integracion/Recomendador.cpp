#include "05_interfaz_integracion/Recomendador.h"
#include "02_limpieza_normalizacion/DataCleaner.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>
#include <unordered_set>

namespace {

//datos de una pelicula ya normalizados para compararlos rapido
struct Perfil {
    std::string director;
    std::string origen;
    std::unordered_set<std::string> actores;
    std::unordered_set<std::string> generos;
    int anio = 0; //0 si no tiene anio
};

//el reparto viene separado por comas
std::unordered_set<std::string> separarActores(const std::string& reparto) {
    std::unordered_set<std::string> actores;
    size_t inicio = 0;
    while (inicio <= reparto.size()) {
        size_t coma = reparto.find(',', inicio);
        if (coma == std::string::npos) {
            coma = reparto.size();
        }
        std::string actor = DataCleaner::normalizeForSearch(reparto.substr(inicio, coma - inicio));
        if (!actor.empty()) {
            actores.insert(actor);
        }
        inicio = coma + 1;
    }
    return actores;
}

//el genero puede ser "comedy, drama" o "romantic comedy", se toma cada palabra
std::unordered_set<std::string> separarGeneros(const std::string& genero) {
    std::unordered_set<std::string> generos;
    std::string palabra;
    for (char c : DataCleaner::normalizeForSearch(genero) + ' ') {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            palabra += c;
        } else if (!palabra.empty()) {
            generos.insert(palabra);
            palabra.clear();
        }
    }
    return generos;
}

Perfil crearPerfil(const RawMovie& pelicula) {
    Perfil perfil;
    perfil.director = DataCleaner::normalizeForSearch(pelicula.director);
    perfil.origen = DataCleaner::normalizeForSearch(pelicula.origin);
    perfil.actores = separarActores(pelicula.cast);
    perfil.generos = separarGeneros(pelicula.genre);
    perfil.anio = std::atoi(pelicula.releaseYear.c_str());
    return perfil;
}

int contarEnComun(const std::unordered_set<std::string>& a, const std::unordered_set<std::string>& b) {
    int comunes = 0;
    for (const std::string& valor : a) {
        if (b.count(valor) > 0) {
            comunes++;
        }
    }
    return comunes;
}

int puntaje(const Perfil& candidata, const Perfil& conLike) {
    int puntos = 0;
    if (!candidata.director.empty() && candidata.director == conLike.director) {
        puntos += Recomendador::PUNTOS_DIRECTOR;
    }
    puntos += Recomendador::PUNTOS_ACTOR * contarEnComun(candidata.actores, conLike.actores);
    puntos += Recomendador::PUNTOS_GENERO * contarEnComun(candidata.generos, conLike.generos);
    if (puntos == 0) {
        return 0; //origen y epoca solos no bastan para recomendar
    }
    if (!candidata.origen.empty() && candidata.origen == conLike.origen) {
        puntos += Recomendador::PUNTOS_ORIGEN;
    }
    if (candidata.anio > 0 && conLike.anio > 0 && std::abs(candidata.anio - conLike.anio) <= 10) {
        puntos += Recomendador::PUNTOS_EPOCA;
    }
    return puntos;
}

}

std::vector<Resultado> Recomendador::recomendar(const std::vector<RawMovie>& peliculas,
                                                const std::vector<size_t>& idsConLike) {
    std::vector<Resultado> recomendadas;
    std::vector<Perfil> perfilesConLike;
    std::unordered_set<size_t> excluir;
    for (size_t id : idsConLike) {
        if (id < peliculas.size()) {
            perfilesConLike.push_back(crearPerfil(peliculas[id]));
            excluir.insert(id);
        }
    }
    if (perfilesConLike.empty()) {
        return recomendadas;
    }

    for (size_t id = 0; id < peliculas.size(); id++) {
        if (excluir.count(id) > 0) {
            continue;
        }
        Perfil candidata = crearPerfil(peliculas[id]);
        int total = 0;
        for (const Perfil& conLike : perfilesConLike) {
            total += puntaje(candidata, conLike);
        }
        if (total > 0) {
            Resultado resultado;
            resultado.id = id;
            resultado.prioridad = total;
            resultado.tituloOrden = DataCleaner::normalizeForSearch(peliculas[id].title);
            resultado.pelicula = &peliculas[id];
            recomendadas.push_back(resultado);
        }
    }
    std::sort(recomendadas.begin(), recomendadas.end()); //usa el orden de Resultado
    return recomendadas;
}
