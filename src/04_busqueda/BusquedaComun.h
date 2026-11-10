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

// Puntaje de la busqueda por texto (todos los campos, con pesos)
int puntajeTexto(const RawMovie& pelicula, const std::string& frase,
                 const std::vector<std::string>& terminos);

// Puntaje de la busqueda por tag (un solo campo)
int puntajeCampo(const std::string& valor, const std::string& frase,
                 const std::vector<std::string>& terminos);

// Arma el resultado de una pelicula con su titulo listo para ordenar
Resultado crearResultado(size_t id, const RawMovie& pelicula);

}
