#include "Busqueda.h"
#include "DataCleaner.h"
#include "Utilidades.h"

#include <algorithm>
#include <unordered_set>

const size_t ANCHO_TITULO = 40;
const size_t ANCHO_DIRECTOR = 18;
const size_t ANCHO_GENERO = 12;


bool Resultado::operator<(const Resultado& otro) const {
    if (prioridad != otro.prioridad) {
        return prioridad > otro.prioridad; // mayor prioridad va primero
    }
    if (tituloOrden != otro.tituloOrden) {
        return tituloOrden < otro.tituloOrden;
    }
    return id < otro.id;
}

std::ostream& operator<<(std::ostream& os, const Resultado& resultado) {
    if (resultado.pelicula == nullptr) {
        return os;
    }
    const RawMovie& pelicula = *resultado.pelicula;

    // Si el titulo es muy largo se recorta, pero el anio siempre se ve
    std::string anio;
    if (!pelicula.releaseYear.empty()) {
        anio = " (" + pelicula.releaseYear + ")";
    }
    std::string titulo = recortar(pelicula.title, ANCHO_TITULO - contarCaracteres(anio)) + anio;

    std::string director = pelicula.director.empty() ? "-" : pelicula.director;
    std::string genero = pelicula.genre.empty() ? "-" : pelicula.genre;

    os << rellenar(titulo, ANCHO_TITULO) << " | " << rellenar(recortar(director, ANCHO_DIRECTOR), ANCHO_DIRECTOR) << " | " << recortar(genero, ANCHO_GENERO);
    return os;
}


Busqueda::Busqueda(const std::string& texto) : texto(texto) {
    // Se usa la misma funcion que el arbol para que ambos comparen igual
    textoNormalizado = DataCleaner::normalizeForSearch(texto);
}

Resultado Busqueda::crearResultado(size_t id, const RawMovie& pelicula) const {
    Resultado resultado;
    resultado.id = id;
    resultado.pelicula = &pelicula;
    resultado.tituloOrden = DataCleaner::normalizeForSearch(pelicula.title);
    return resultado;
}

const std::string& Busqueda::getTexto() const {
    return texto;
}


BusquedaTexto::BusquedaTexto(const std::string& texto) : Busqueda(texto) {
}

// Complejidad aproximada: O(m) para bajar por el arbol
std::vector<Resultado> BusquedaTexto::ejecutar(const MovieTrie& arbol, const std::vector<RawMovie>& peliculas) const {
    std::vector<Resultado> resultados;

    // El arbol devuelve los ids de las peliculas que contienen el texto
    std::unordered_set<size_t> ids = arbol.searchSubstring(texto);
    resultados.reserve(ids.size());

    for (size_t id : ids) {
        if (id >= peliculas.size()) {
            continue; // por seguridad, no deberia pasar
        }
        Resultado resultado = crearResultado(id, peliculas[id]);
        if (resultado.tituloOrden.find(textoNormalizado) != std::string::npos) {
            resultado.prioridad = 1;
        }
        resultados.push_back(resultado);
    }

    std::sort(resultados.begin(), resultados.end()); // usa Resultado::operator<
    return resultados;
}

std::string BusquedaTexto::descripcion() const {
    return "\"" + texto + "\"";
}


BusquedaTag::BusquedaTag(FieldType campo, const std::string& texto) : Busqueda(texto), campo(campo) {
}

const std::string& BusquedaTag::valorCampo(const RawMovie& pelicula) const {
    switch (campo) {
        case FieldType::TITLE:
            return pelicula.title;
        case FieldType::PLOT:
            return pelicula.plot;
        case FieldType::DIRECTOR:
            return pelicula.director;
        case FieldType::CAST:
            return pelicula.cast;
        case FieldType::GENRE:
            return pelicula.genre;
    }
    return pelicula.title;
}


std::vector<Resultado> BusquedaTag::ejecutar(const MovieTrie& arbol, const std::vector<RawMovie>& peliculas) const {
    std::vector<Resultado> resultados;
    std::unordered_set<size_t> candidatos = arbol.searchSubstring(texto);

    for (size_t id : candidatos) {
        if (id >= peliculas.size()) {
            continue;
        }
        const RawMovie& pelicula = peliculas[id];
        std::string valor = DataCleaner::normalizeForSearch(valorCampo(pelicula));
        if (valor.find(textoNormalizado) != std::string::npos) {
            resultados.push_back(crearResultado(id, pelicula));
        }
    }

    std::sort(resultados.begin(), resultados.end());
    return resultados;
}

std::string BusquedaTag::descripcion() const {
    return nombreCampo(campo) + " \"" + texto + "\"";
}

std::string BusquedaTag::nombreCampo(FieldType campo) {
    switch (campo) {
        case FieldType::TITLE:
            return "titulo";
        case FieldType::PLOT:
            return "sinopsis";
        case FieldType::DIRECTOR:
            return "director";
        case FieldType::CAST:
            return "reparto";
        case FieldType::GENRE:
            return "genero";
    }
    return "campo";
}
