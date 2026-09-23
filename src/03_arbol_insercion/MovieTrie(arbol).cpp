#include "03_arbol_insercion/MovieTrie(arbol).h"
#include "02_limpieza_normalizacion/DataCleaner.h"

MovieTrie::MovieTrie() {
    root = new TrieNode();
}

MovieTrie::~MovieTrie() {
    delete root;
}

void MovieTrie::insertKey(const std::string& text, size_t movieId) {
    TrieNode* current = root;
    for (char c : text) {
        if (current->children.find(c) == current->children.end()) {
            current->children[c] = new TrieNode();
        }
        current = current->children[c];
        current->movieIds.insert(movieId);
    }
}

std::string MovieTrie::keyFromQuery(const std::string& text) {
    if (text.size() <= KEY_SIZE) {
        return text;
    }
    return text.substr(0, KEY_SIZE);
}

void MovieTrie::indexText(const std::string& text, size_t movieId) {
    std::string cleanText = DataCleaner::normalizeForSearch(text);
    if (cleanText.empty()) return;

    // Indice liviano de subcadenas.
    for (size_t i = 0; i < cleanText.size(); ++i) {
        insertKey(cleanText.substr(i, KEY_SIZE), movieId);
    }
}

void MovieTrie::buildIndex(const std::vector<RawMovie>& movies) {
    auto* replacement = new TrieNode();
    delete root;
    root = replacement;
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
    std::string cleanQuery = keyFromQuery(DataCleaner::normalizeForSearch(query));
    if (cleanQuery.empty()) return {};
    TrieNode* current = root;

    for (char c : cleanQuery) {
        auto it = current->children.find(c);
        if (it == current->children.end()) {
            return {};
        }
        current = it->second;
    }

    return current->movieIds;
}