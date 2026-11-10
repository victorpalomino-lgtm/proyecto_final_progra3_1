#include "04_busqueda/BusquedaComun.h"
#include "02_limpieza_normalizacion/DataCleaner.h"

#include <sstream>

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

Resultado crearResultado(size_t id, const RawMovie& pelicula) {
    Resultado resultado;
    resultado.id = id;
    resultado.pelicula = &pelicula;
    resultado.tituloOrden = DataCleaner::normalizeForSearch(pelicula.title);
    return resultado;
}

}
