#include "04_busqueda/Busqueda.h"
#include "04_busqueda/BusquedaComun.h"
#include "02_limpieza_normalizacion/DataCleaner.h"
#include "comun/Utilidades.h"

#include "comun/Paralelismo.h"

#include <algorithm>
#include <atomic>
#include <iterator>
#include <unordered_set>
#include <utility>

using namespace busqueda_interna;

namespace {

// Candidatos que toma un hilo de una vez. Bloques chicos con reparto dinamico equilibran
// la carga aunque los candidatos tengan sinopsis de largos distintos y aunque el CPU tenga
// nucleos de distinta velocidad (P-cores y E-cores).
constexpr size_t BLOQUE_CANDIDATOS = 32;

// Verifica y puntua los candidatos, en uno o varios hilos, y devuelve los resultados ordenados.
// procesar(id, salida) agrega a "salida" el resultado del candidato id (si corresponde).
//
// Datos compartidos: "lista" y "siguiente" (atomic). procesar() solo lee el arbol y las
// peliculas. Cada hilo h escribe unicamente en partes[h]; la union se hace despues del
// join, en el hilo que llamo.
template <typename Procesar>
std::vector<Resultado> procesarCandidatos(const std::unordered_set<size_t>& ids, size_t hilos,
                                          size_t umbral, const Procesar& procesar) {
    std::vector<Resultado> resultados;
    hilos = paralelo::resolverHilos(hilos);
    // No lanzar mas hilos que bloques de trabajo.
    hilos = std::min(hilos, (ids.size() + BLOQUE_CANDIDATOS - 1) / BLOQUE_CANDIDATOS);

    if (hilos <= 1 || ids.size() < umbral) {
        for (size_t id : ids) procesar(id, resultados);
    } else {
        const std::vector<size_t> lista(ids.begin(), ids.end());
        std::vector<std::vector<Resultado>> partes(hilos);
        std::atomic<size_t> siguiente{0};
        paralelo::ejecutarEnHilos(hilos, [&](size_t h) {
            for (;;) {
                const size_t inicio = siguiente.fetch_add(BLOQUE_CANDIDATOS, std::memory_order_relaxed);
                if (inicio >= lista.size()) break;
                const size_t fin = std::min(lista.size(), inicio + BLOQUE_CANDIDATOS);
                for (size_t i = inicio; i < fin; ++i) procesar(lista[i], partes[h]);
            }
        });
        size_t total = 0;
        for (const auto& parte : partes) total += parte.size();
        resultados.reserve(total);
        for (auto& parte : partes) {
            std::move(parte.begin(), parte.end(), std::back_inserter(resultados));
        }
    }

    std::sort(resultados.begin(), resultados.end()); // usa Resultado::operator<
    return resultados;
}

} // namespace

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


BusquedaTexto::BusquedaTexto(size_t hilos, size_t umbral) : hilos(hilos), umbral(umbral) {
}

// Trie: O(min(m,3) + candidatos), ademas de normalizacion, confirmacion y ordenamiento.
std::vector<Resultado> BusquedaTexto::ejecutar(const std::string& consulta, const MovieTrie& arbol,
                                               const std::vector<RawMovie>& peliculas) const {
    // Se usa la misma funcion que el arbol para que ambos comparen igual
    const std::string textoNormalizado = DataCleaner::normalizeForSearch(consulta);

    const std::vector<std::string> terminos = separarTerminos(textoNormalizado);
    if (terminos.empty()) {
        return {};
    }

    // La consulta completa debe aparecer de forma contigua en un campo.
    const auto ids = arbol.searchSubstring(textoNormalizado);

    return procesarCandidatos(ids, hilos, umbral, [&](size_t id, std::vector<Resultado>& salida) {
        if (id >= peliculas.size()) {
            return;
        }
        const RawMovie& pelicula = peliculas[id];
        // Cada campo se normaliza una sola vez y se reutiliza para verificar y puntuar.
        CamposNormalizados campos = normalizarCampos(pelicula);
        if (!algunCampoContiene(campos, textoNormalizado)) return;
        const int prioridad = puntajeTexto(campos, textoNormalizado, terminos);
        if (prioridad > 0) {
            Resultado resultado = crearResultado(id, pelicula, std::move(campos.titulo));
            resultado.prioridad = prioridad;
            salida.push_back(std::move(resultado));
        }
    });
}

std::string BusquedaTexto::descripcion(const std::string& consulta) const {
    return "\"" + consulta + "\"";
}


BusquedaTag::BusquedaTag(FieldType campo, size_t hilos, size_t umbral)
    : campo(campo), hilos(hilos), umbral(umbral) {
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

    const std::vector<std::string> terminos = separarTerminos(textoNormalizado);
    if (terminos.empty()) {
        return {};
    }

    const auto candidatos = arbol.searchSubstring(textoNormalizado);

    return procesarCandidatos(candidatos, hilos, umbral, [&](size_t id, std::vector<Resultado>& salida) {
        if (id >= peliculas.size()) {
            return;
        }
        const RawMovie& pelicula = peliculas[id];
        // El campo se normaliza una sola vez para verificar y puntuar.
        const std::string campoNormalizado = DataCleaner::normalizeForSearch(valorCampo(pelicula));
        if (!contiene(campoNormalizado, textoNormalizado)) return;
        const int prioridad = puntajeCampoNormalizado(campoNormalizado, textoNormalizado, terminos);
        if (prioridad > 0) {
            Resultado resultado = crearResultado(id, pelicula);
            resultado.prioridad = prioridad;
            salida.push_back(std::move(resultado));
        }
    });
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
