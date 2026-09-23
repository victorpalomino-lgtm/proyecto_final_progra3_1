#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "comun/RawMovie.h"

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
    static constexpr size_t KEY_SIZE = 3;

    // Clave corta para candidatos.
    void insertKey(const std::string& text, size_t movieId);
    static std::string keyFromQuery(const std::string& text);

public:
    MovieTrie();
    ~MovieTrie();
    MovieTrie(const MovieTrie&) = delete;
    MovieTrie& operator=(const MovieTrie&) = delete;

    //Indexamos todo el dataset de peliculas
    void buildIndex(const std::vector<RawMovie>& movies);

    // Claves de subcadenas.
    void indexText(const std::string& text, size_t movieId);

    // IDs candidatos para la busqueda.
    std::unordered_set<size_t> searchSubstring(const std::string& query) const;
};