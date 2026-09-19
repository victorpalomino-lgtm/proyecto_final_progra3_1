#include "MovieTrie(arbol).h"
#include "DataCleaner.h"

MovieTrie::MovieTrie() {
    root = new TrieNode();
}

MovieTrie::~MovieTrie() {
    delete root;
}

void MovieTrie::insertSuffix(const std::string& text, size_t movieId) {
    TrieNode* current = root;
    for (char c : text) {
        if (current->children.find(c) == current->children.end()) {
            current->children[c] = new TrieNode();
        }
        current = current->children[c];
        current->movieIds.insert(movieId); // Registra que esta película pasa por este nodo
    }
}

void MovieTrie::indexText(const std::string& text, size_t movieId) {
    std::string cleanText = DataCleaner::normalizeForSearch(text);
    if (cleanText.empty()) return;

    // Para buscar las subcadenas, insertamoslos sufijos.
    for (size_t i = 0; i < cleanText.size(); ++i) {
        // Insertamos la subcadena desde la posición 'i' en adelante
        insertSuffix(cleanText.substr(i), movieId);
    }
}

void MovieTrie::buildIndex(const std::vector<RawMovie>& movies) {
    for (size_t id = 0; id < movies.size(); ++id) {
        const auto& movie = movies[id];

        indexText(movie.title, id);
        indexText(movie.plot, id);

        indexText(movie.director, id);
        indexText(movie.cast, id);
        indexText(movie.genre, id);
    }
}

std::unordered_set<size_t> MovieTrie::searchSubstring(const std::string& query) const {
    std::string cleanQuery = DataCleaner::normalizeForSearch(query);
    TrieNode* current = root;

    for (char c : cleanQuery) {
        auto it = current->children.find(c);
        if (it == current->children.end()) {
            return {}; // No existe la subcadena en ninguna película
        }
        current = it->second;
    }

    //Todos los IDs de películas que tienen la secuencia
    return current->movieIds;
}