#include "04_busqueda/BusquedaComun.h"
#include "02_limpieza_normalizacion/DataCleaner.h"

#include <algorithm>
#include <sstream>
#include <utility>

namespace busqueda_interna {

namespace {
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
}

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

CamposNormalizados normalizarCampos(const RawMovie& pelicula) {
    CamposNormalizados campos;
    campos.titulo = DataCleaner::normalizeForSearch(pelicula.title);
    campos.sinopsis = DataCleaner::normalizeForSearch(pelicula.plot);
    campos.director = DataCleaner::normalizeForSearch(pelicula.director);
    campos.reparto = DataCleaner::normalizeForSearch(pelicula.cast);
    campos.genero = DataCleaner::normalizeForSearch(pelicula.genre);
    return campos;
}

int puntajeTexto(const RawMovie& pelicula, const std::string& frase,
                 const std::vector<std::string>& terminos) {
    return puntajeTexto(normalizarCampos(pelicula), frase, terminos);
}

int puntajeTexto(const CamposNormalizados& campos, const std::string& frase,
                 const std::vector<std::string>& terminos) {
    const std::string& titulo = campos.titulo;
    const std::string& director = campos.director;
    const std::string& reparto = campos.reparto;
    const std::string& genero = campos.genero;
    const std::string& sinopsis = campos.sinopsis;

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

std::unordered_set<size_t> candidatosTexto(const MovieTrie& arbol, const std::vector<std::string>& terminos) {
    std::unordered_set<size_t> candidatos;
    for (const std::string& termino : terminos) {
        const std::unordered_set<size_t> ids = arbol.searchSubstring(termino);
        candidatos.insert(ids.begin(), ids.end());
    }
    return candidatos;
}

int prioridadTexto(const CamposNormalizados& campos, const std::string& frase,
                   const std::vector<std::string>& terminos) {
    auto aparece = [&](const std::string& texto) {
        return contiene(campos.titulo, texto) || contiene(campos.sinopsis, texto) ||
               contiene(campos.director, texto) || contiene(campos.reparto, texto) ||
               contiene(campos.genero, texto);
    };

    int encontrados = 0;
    for (const std::string& termino : terminos) {
        if (aparece(termino)) {
            encontrados++;
        }
    }
    if (encontrados == 0) {
        return 0;
    }
    int nivel = encontrados;
    if (terminos.size() > 1 && aparece(frase)) {
        nivel++; // frase completa y contigua
    }
    nivel = std::min(nivel, NIVEL_MAXIMO);
    return nivel * PESO_NIVEL + puntajeTexto(campos, frase, terminos);
}

int puntajeCampo(const std::string& valor, const std::string& frase,
                 const std::vector<std::string>& terminos) {
    return puntajeCampoNormalizado(DataCleaner::normalizeForSearch(valor), frase, terminos);
}

int puntajeCampoNormalizado(const std::string& campo, const std::string& frase,
                            const std::vector<std::string>& terminos) {
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

Resultado crearResultado(size_t id, const RawMovie& pelicula) {
    return crearResultado(id, pelicula, DataCleaner::normalizeForSearch(pelicula.title));
}

Resultado crearResultado(size_t id, const RawMovie& pelicula, std::string tituloNormalizado) {
    Resultado resultado;
    resultado.id = id;
    resultado.pelicula = &pelicula;
    resultado.tituloOrden = std::move(tituloNormalizado);
    return resultado;
}

}
