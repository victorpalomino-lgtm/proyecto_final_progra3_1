#pragma once

// Cabecera INTERNA del modulo de busqueda: solo la incluyen las estrategias
// (Busqueda.cpp). La interfaz y el resto del programa no la usan.
// Son las mismas funciones que antes estaban en el namespace anonimo de Busqueda.cpp.

#include <string>
#include <vector>
#include "comun/RawMovie.h"
#include "04_busqueda/EstrategiaBusqueda.h"

namespace busqueda_interna {

// Separa el texto normalizado en palabras, sin repetidas
std::vector<std::string> separarTerminos(const std::string& textoNormalizado);

bool contiene(const std::string& campo, const std::string& texto);

// Los 5 campos de una pelicula normalizados UNA sola vez, para reutilizarlos entre la
// verificacion y el puntaje (antes cada etapa volvia a normalizar los mismos textos).
struct CamposNormalizados {
    std::string titulo;
    std::string sinopsis;
    std::string director;
    std::string reparto;
    std::string genero;
};

CamposNormalizados normalizarCampos(const RawMovie& pelicula);

// true si la frase aparece en alguno de los campos (mismo orden que antes:
// titulo, sinopsis, director, reparto, genero).
bool algunCampoContiene(const CamposNormalizados& campos, const std::string& frase);

// Puntaje de la busqueda por texto (todos los campos, con pesos)
int puntajeTexto(const RawMovie& pelicula, const std::string& frase,
                 const std::vector<std::string>& terminos);
// Igual, pero con los campos ya normalizados (mismo resultado).
int puntajeTexto(const CamposNormalizados& campos, const std::string& frase,
                 const std::vector<std::string>& terminos);

// Puntaje de la busqueda por tag (un solo campo)
int puntajeCampo(const std::string& valor, const std::string& frase,
                 const std::vector<std::string>& terminos);
// Igual, pero con el campo ya normalizado (mismo resultado).
int puntajeCampoNormalizado(const std::string& campo, const std::string& frase,
                            const std::vector<std::string>& terminos);

// Arma el resultado de una pelicula con su titulo listo para ordenar
Resultado crearResultado(size_t id, const RawMovie& pelicula);
// Igual, reutilizando el titulo ya normalizado.
Resultado crearResultado(size_t id, const RawMovie& pelicula, std::string tituloNormalizado);

}
