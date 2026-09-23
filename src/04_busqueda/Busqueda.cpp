#include "04_busqueda/Busqueda.h"
#include "02_limpieza_normalizacion/DataCleaner.h"
#include "comun/Utilidades.h"

#include <algorithm>
#include <sstream>
#include <unordered_set>

const size_t ANCHO_TITULO = 40;
const size_t ANCHO_DIRECTOR = 18;
const size_t ANCHO_GENERO = 12;

namespace {
std::vector<std::string> separarTerminos(const std::string& textoNormalizado) {
    std::vector<std::string> terminos;
    std::istringstream entrada(textoNormalizado);
    std::string termino;

    while (entrada >> termino) {
        bool repetido = false;
        for (const std::string& guardado : terminos) {
            if (guardado == termino) {
                repetido = true;
                break;
            }
        }
        if (!repetido) {
            terminos.push_back(termino);
        }
    }
    return terminos;
}

bool contiene(const std::string& campo, const std::string& texto) {
    return !texto.empty() && campo.find(texto) != std::string::npos;
}

int sumarCampo(const std::string& campo, const std::string& frase,
               const std::vector<std::string>& terminos, int peso) {
    int puntaje = 0;
    if (contiene(campo, frase)) {
        puntaje += peso * 4;
    }
    for (const std::string& termino : terminos) {
        if (contiene(campo, termino)) {
            puntaje += peso;
        }
    }
    return puntaje;
}

int puntajeTexto(const RawMovie& pelicula, const std::string& frase,
                 const std::vector<std::string>& terminos) {
    std::string titulo = DataCleaner::normalizeForSearch(pelicula.title);
    std::string director = DataCleaner::normalizeForSearch(pelicula.director);
    std::string reparto = DataCleaner::normalizeForSearch(pelicula.cast);
    std::string genero = DataCleaner::normalizeForSearch(pelicula.genre);
    std::string sinopsis = DataCleaner::normalizeForSearch(pelicula.plot);

    int puntaje = 0;
    if (titulo == frase) {
        puntaje += 200;
    }
    puntaje += sumarCampo(titulo, frase, terminos, 40);
    puntaje += sumarCampo(director, frase, terminos, 18);
    puntaje += sumarCampo(genero, frase, terminos, 14);
    puntaje += sumarCampo(reparto, frase, terminos, 12);
    puntaje += sumarCampo(sinopsis, frase, terminos, 5);

    bool encontroTodos = !terminos.empty();
    for (const std::string& termino : terminos) {
        bool aparece = contiene(titulo, termino) || contiene(director, termino) ||
                       contiene(genero, termino) || contiene(reparto, termino) ||
                       contiene(sinopsis, termino);
        if (!aparece) {
            encontroTodos = false;
            break;
        }
    }
    if (encontroTodos) {
        puntaje += 20;
    }
    return puntaje;
}

int puntajeCampo(const std::string& valor, const std::string& frase,
                 const std::vector<std::string>& terminos) {
    std::string campo = DataCleaner::normalizeForSearch(valor);
    int puntaje = 0;
    if (contiene(campo, frase)) {
        puntaje += 40;
    }
    for (const std::string& termino : terminos) {
        if (contiene(campo, termino)) {
            puntaje += 10;
        }
    }
    return puntaje;
}
}

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

// Trie: O(min(m,3) + candidatos), ademas de normalizacion, confirmacion y ordenamiento.
std::vector<Resultado> BusquedaTexto::ejecutar(const MovieTrie& arbol, const std::vector<RawMovie>& peliculas) const {
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
