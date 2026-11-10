#include "04_busqueda/Busqueda.h"
#include "04_busqueda/BusquedaComun.h"
#include "02_limpieza_normalizacion/DataCleaner.h"
#include "comun/Utilidades.h"

#include <algorithm>

using namespace busqueda_interna;

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


// Trie: O(min(m,3) + candidatos), ademas de normalizacion, confirmacion y ordenamiento.
std::vector<Resultado> BusquedaTexto::ejecutar(const std::string& consulta, const MovieTrie& arbol,
                                               const std::vector<RawMovie>& peliculas) const {
    // Se usa la misma funcion que el arbol para que ambos comparen igual
    const std::string textoNormalizado = DataCleaner::normalizeForSearch(consulta);

    std::vector<Resultado> resultados;
    std::vector<std::string> terminos = separarTerminos(textoNormalizado);
    if (terminos.empty()) {
        return resultados;
    }

    // La consulta completa debe aparecer de forma contigua en un campo.
    const auto ids = arbol.searchSubstring(textoNormalizado);

    for (size_t id : ids) {
        if (id >= peliculas.size()) {
            continue;
        }
        const auto& pelicula = peliculas[id];
        bool coincide = false;
        for (const auto* campo : {&pelicula.title, &pelicula.plot, &pelicula.director,
                                  &pelicula.cast, &pelicula.genre}) {
            if (contiene(DataCleaner::normalizeForSearch(*campo), textoNormalizado)) {
                coincide = true;
                break;
            }
        }
        if (!coincide) continue;
        Resultado resultado = crearResultado(id, peliculas[id]);
        resultado.prioridad = puntajeTexto(peliculas[id], textoNormalizado, terminos);
        if (resultado.prioridad > 0) {
            resultados.push_back(resultado);
        }
    }

    std::sort(resultados.begin(), resultados.end()); // usa Resultado::operator<
    return resultados;
}

std::string BusquedaTexto::descripcion(const std::string& consulta) const {
    return "\"" + consulta + "\"";
}


BusquedaTag::BusquedaTag(FieldType campo) : campo(campo) {
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


std::vector<Resultado> BusquedaTag::ejecutar(const std::string& consulta, const MovieTrie& arbol,
                                             const std::vector<RawMovie>& peliculas) const {
    const std::string textoNormalizado = DataCleaner::normalizeForSearch(consulta);

    std::vector<Resultado> resultados;
    std::vector<std::string> terminos = separarTerminos(textoNormalizado);
    if (terminos.empty()) {
        return resultados;
    }

    const auto candidatos = arbol.searchSubstring(textoNormalizado);

    for (size_t id : candidatos) {
        if (id >= peliculas.size()) {
            continue;
        }
        const RawMovie& pelicula = peliculas[id];
        if (!contiene(DataCleaner::normalizeForSearch(valorCampo(pelicula)), textoNormalizado)) continue;
        Resultado resultado = crearResultado(id, pelicula);
        resultado.prioridad = puntajeCampo(valorCampo(pelicula), textoNormalizado, terminos);
        if (resultado.prioridad > 0) {
            resultados.push_back(resultado);
        }
    }

    std::sort(resultados.begin(), resultados.end());
    return resultados;
}

std::string BusquedaTag::descripcion(const std::string& consulta) const {
    return nombreCampo(campo) + " \"" + consulta + "\"";
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
