#pragma once

// Cabecera INTERNA del modulo de busqueda: solo la incluyen las estrategias
// (Busqueda.cpp). La interfaz y el resto del programa no la usan.
// Son las mismas funciones que antes estaban en el namespace anonimo de Busqueda.cpp.

#include <string>
#include <unordered_set>
#include <vector>
#include "comun/RawMovie.h"
#include "03_arbol_insercion/MovieTrie(arbol).h"
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

// Puntaje de la busqueda por texto (todos los campos, con pesos)
int puntajeTexto(const RawMovie& pelicula, const std::string& frase,
                 const std::vector<std::string>& terminos);
// Igual, pero con los campos ya normalizados (mismo resultado).
int puntajeTexto(const CamposNormalizados& campos, const std::string& frase,
                 const std::vector<std::string>& terminos);

// Busqueda por texto "y/o": una frase encuentra las peliculas que tienen CUALQUIERA de sus
// palabras. La prioridad se arma por niveles para que el orden sea:
//   1) la frase completa y contigua (con todas sus palabras)
//   2) todas las palabras, aunque esten separadas
//   3) menos palabras (cuantas mas, antes)
// nivel = palabras encontradas (+1 si la frase aparece contigua y tiene mas de una palabra).
// prioridad = nivel * PESO_NIVEL + puntajeTexto, asi puntajeTexto solo desempata dentro
// de un mismo nivel. Con una sola palabra el nivel es siempre el mismo y el orden queda
// igual que antes. puntajeTexto es menor que PESO_NIVEL mientras la consulta tenga menos
// de ~1 000 palabras distintas; el nivel se limita a NIVEL_MAXIMO para no desbordar int.
constexpr int PESO_NIVEL = 1000000;
constexpr int NIVEL_MAXIMO = 1000;

// Union de los candidatos del arbol para cada palabra (sin repetidos).
std::unordered_set<size_t> candidatosTexto(const MovieTrie& arbol, const std::vector<std::string>& terminos);

// 0 si la pelicula no tiene ninguna de las palabras.
int prioridadTexto(const CamposNormalizados& campos, const std::string& frase,
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
