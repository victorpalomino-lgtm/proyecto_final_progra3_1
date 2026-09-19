#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "RawMovie.h"

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

    // Inserta una cadena exacta comenzando desde un nodo raíz específico
    void insertSuffix(const std::string& text, size_t movieId);

public:
    MovieTrie();
    ~MovieTrie();

    //Indexamos todo el dataset de peliculas
    void buildIndex(const std::vector<RawMovie>& movies);

    //Inserta todos los sufijos
    void indexText(const std::string& text, size_t movieId);

    //Búsqueda rápida que retorna los IDs
    std::unordered_set<size_t> searchSubstring(const std::string& query) const;
};