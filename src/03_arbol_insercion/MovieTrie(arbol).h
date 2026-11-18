#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "comun/RawMovie.h"
#include "03_arbol_insercion/ObservadorIndexacion.h"

enum class FieldType {
    TITLE,
    PLOT,
    DIRECTOR,
    CAST,
    GENRE
};

struct TrieNode {
    std::unordered_map<char, TrieNode*> children;

    // Lista de IDs de películas.
    std::unordered_set<size_t> movieIds;  //unordered_set evita IDs duplicados si la palabra se repite.

    ~TrieNode() {
        for (auto& pair : children) {
            delete pair.second;
        }
    }
};

class MovieTrie {
private:
    TrieNode* root;
    static constexpr size_t KEY_SIZE = 8;

    // Clave corta para candidatos.
    void insertKey(const std::string& text, size_t movieId);
    static std::string keyFromQuery(const std::string& text);

    void buildSecuencial(const std::vector<RawMovie>& movies, IObservadorIndexacion* observador);
    void buildParalelo(TrieNode* nuevaRaiz, const std::vector<RawMovie>& movies, size_t hilos,
                       IObservadorIndexacion* observador);

public:
    // Longitud maxima de clave indexada directamente en el Trie (8-gramas).
    static constexpr size_t maxKeySize() { return KEY_SIZE; }

    // Por debajo de esta cantidad de peliculas se indexa en un solo hilo: crear hilos y
    // normalizar por adelantado no compensa (medido con bench/Benchmark.cpp).
    static constexpr size_t MIN_PELICULAS_PARALELO = 64;

    MovieTrie();
    ~MovieTrie();
    MovieTrie(const MovieTrie&) = delete;
    MovieTrie& operator=(const MovieTrie&) = delete;

    // Indexamos todo el dataset de peliculas.
    // hilos: 0 = std::thread::hardware_concurrency(), 1 = algoritmo secuencial original.
    // observador (opcional): recibe el progreso, siempre desde el hilo que llama.
    // El arbol resultante es el mismo con cualquier numero de hilos.
    void buildIndex(const std::vector<RawMovie>& movies, size_t hilos = 0,
                    IObservadorIndexacion* observador = nullptr);

    // Claves de subcadenas.
    void indexText(const std::string& text, size_t movieId);

    // IDs candidatos para la busqueda.
    std::unordered_set<size_t> searchSubstring(const std::string& query) const;

    // Compara los dos arboles nodo por nodo (mismos hijos y mismos IDs en cada nodo).
    // Se usa en pruebas y en el benchmark para verificar que el indice paralelo es
    // identico al secuencial.
    bool mismoContenido(const MovieTrie& otro) const;
};
