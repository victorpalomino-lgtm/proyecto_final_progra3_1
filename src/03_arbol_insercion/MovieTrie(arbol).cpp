#include "03_arbol_insercion/MovieTrie(arbol).h"
#include "02_limpieza_normalizacion/DataCleaner.h"
#include "comun/Paralelismo.h"

#include <algorithm>
#include <atomic>
#include <memory>
#include <numeric>

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

namespace {

constexpr size_t NUM_CAMPOS = 5;
constexpr size_t NUM_BYTES = 256;
// Peliculas que toma un hilo de una vez al normalizar (reparto dinamico con un atomic).
constexpr size_t BLOQUE_NORMALIZAR = 64;

// Mismo orden de campos que el algoritmo secuencial.
std::array<const std::string*, NUM_CAMPOS> camposIndexados(const RawMovie& m) {
    return {&m.title, &m.plot, &m.director, &m.cast, &m.genre};
}

size_t byteDe(char c) {
    return static_cast<unsigned char>(c);
}

// Inserta la clave text[i, i+longitud) empezando en "nodo", que ya es el hijo de la raiz
// correspondiente a text[i]. Equivale a insertKey(text.substr(i, KEY_SIZE), id).
void insertarDesde(TrieNode* nodo, const std::string& text, size_t i, size_t longitud, size_t id) {
    nodo->movieIds.insert(id);
    const size_t fin = std::min(text.size(), i + longitud);
    for (size_t j = i + 1; j < fin; ++j) {
        auto it = nodo->children.find(text[j]);
        if (it == nodo->children.end()) {
            auto nuevo = std::make_unique<TrieNode>();
            it = nodo->children.emplace(text[j], nuevo.get()).first;
            nuevo.release(); // ahora lo posee el mapa de hijos
        }
        nodo = it->second;
        nodo->movieIds.insert(id);
    }
}

// LPT (Longest Processing Time): los caracteres mas pesados primero, cada uno al hilo con
// menos carga acumulada. Devuelve el hilo duenio de cada byte.
std::array<size_t, NUM_BYTES> repartirLPT(const std::array<size_t, NUM_BYTES>& peso, size_t hilos) {
    std::array<size_t, NUM_BYTES> orden{};
    std::iota(orden.begin(), orden.end(), size_t{0});
    std::stable_sort(orden.begin(), orden.end(), [&](size_t a, size_t b) { return peso[a] > peso[b]; });

    std::array<size_t, NUM_BYTES> duenio{};
    std::vector<size_t> carga(hilos, 0);
    for (size_t byte : orden) {
        if (peso[byte] == 0) break;
        const size_t menor = static_cast<size_t>(
            std::min_element(carga.begin(), carga.end()) - carga.begin());
        duenio[byte] = menor;
        carga[menor] += peso[byte];
    }
    return duenio;
}

} // namespace

void MovieTrie::buildIndex(const std::vector<RawMovie>& movies, size_t hilos,
                           IObservadorIndexacion* observador) {
    auto* replacement = new TrieNode();
    delete root;
    root = replacement;

    hilos = paralelo::resolverHilos(hilos);
    if (hilos <= 1 || movies.size() < MIN_PELICULAS_PARALELO) {
        buildSecuencial(movies, observador);
    } else {
        buildParalelo(root, movies, hilos, observador);
    }
}

// Algoritmo original, un solo hilo. Con observador se hace una pasada previa para conocer
// el total de claves (solo cuesta normalizar, ~4% del tiempo de indexacion).
void MovieTrie::buildSecuencial(const std::vector<RawMovie>& movies, IObservadorIndexacion* observador) {
    size_t total = 0;
    if (observador) {
        for (const auto& movie : movies) {
            for (const std::string* campo : camposIndexados(movie)) {
                total += DataCleaner::normalizeForSearch(*campo).size();
            }
        }
    }

    size_t procesadas = 0;
    for (size_t id = 0; id < movies.size(); ++id) {
        for (const std::string* campo : camposIndexados(movies[id])) {
            const std::string cleanText = DataCleaner::normalizeForSearch(*campo);
            for (size_t i = 0; i < cleanText.size(); ++i) {
                insertKey(cleanText.substr(i, KEY_SIZE), id);
            }
            procesadas += cleanText.size();
        }
        if (observador && (id % 256 == 255)) observador->onProgresoIndexacion(procesadas, total);
    }
    if (observador) observador->onProgresoIndexacion(total, total);
}

// Indexacion paralela por particion del primer caracter de la clave.
//
// Idea: toda clave que empieza con el caracter c vive en el subarbol root->children[c].
// Si cada caracter tiene un unico hilo duenio, los subarboles son disjuntos y ningun nodo
// es escrito por dos hilos: no hacen falta locks ni fusion posterior.
//
// Fases:
//  A) (paralela, por bloques de peliculas) normalizar los 5 campos de cada pelicula una
//     sola vez y contar cuantas claves empiezan con cada byte (= peso del subarbol).
//  B) (secuencial, trivial) repartir los bytes entre hilos con LPT y crear los hijos de
//     la raiz. Despues de esto root->children ya no se modifica.
//  C) (paralela, por particion) cada hilo recorre todas las peliculas en orden de id y solo
//     inserta las claves cuyo primer byte le pertenece.
//
// Datos compartidos y quien los escribe:
//  - movies:               solo lectura para todos los hilos.
//  - textos[id]:           fase A: lo escribe solo el hilo que tomo ese bloque de ids;
//                          fase C: solo lectura (la union de hilos separa ambas fases).
//  - pesosPorHilo[h]:      solo lo escribe el hilo h.
//  - root->children:       se llena en la fase B (un hilo); en la fase C solo se lee
//                          (via tablaRaiz, que es una copia de punteros).
//  - subarbol de byte c:   lo escribe unicamente duenio[c]. Cada hilo inserta los ids en
//                          orden creciente, igual que el secuencial, asi que cada nodo
//                          termina con el mismo conjunto de ids.
//  - progreso:             std::atomic, los trabajadores lo incrementan (relaxed: solo es
//                          un contador informativo) y el hilo que llamo lo lee para
//                          notificar al observador.
//
// Costo de memoria extra: los textos normalizados (~1 byte por caracter indexado, ~80 MB
// en el dataset completo), que se liberan al terminar. El arbol final es identico al
// secuencial, sin copias por hilo.
void MovieTrie::buildParalelo(TrieNode* nuevaRaiz, const std::vector<RawMovie>& movies, size_t hilos,
                              IObservadorIndexacion* observador) {
    const size_t n = movies.size();

    // ---- Fase A: normalizar en paralelo y medir pesos ----
    std::vector<std::array<std::string, NUM_CAMPOS>> textos(n);
    std::vector<std::array<size_t, NUM_BYTES>> pesosPorHilo(hilos);
    std::atomic<size_t> siguienteBloque{0};
    paralelo::ejecutarEnHilos(hilos, [&](size_t h) {
        auto& pesos = pesosPorHilo[h];
        pesos.fill(0);
        for (;;) {
            const size_t inicio = siguienteBloque.fetch_add(BLOQUE_NORMALIZAR, std::memory_order_relaxed);
            if (inicio >= n) break;
            const size_t fin = std::min(n, inicio + BLOQUE_NORMALIZAR);
            for (size_t id = inicio; id < fin; ++id) {
                const auto campos = camposIndexados(movies[id]);
                for (size_t k = 0; k < NUM_CAMPOS; ++k) {
                    textos[id][k] = DataCleaner::normalizeForSearch(*campos[k]);
                    for (char c : textos[id][k]) ++pesos[byteDe(c)];
                }
            }
        }
    });

    std::array<size_t, NUM_BYTES> peso{};
    for (const auto& pesos : pesosPorHilo) {
        for (size_t b = 0; b < NUM_BYTES; ++b) peso[b] += pesos[b];
    }
    const size_t total = std::accumulate(peso.begin(), peso.end(), size_t{0});

    // ---- Fase B: reparto LPT y creacion de los hijos de la raiz (un solo hilo) ----
    const size_t bytesUsados = static_cast<size_t>(
        std::count_if(peso.begin(), peso.end(), [](size_t p) { return p > 0; }));
    const size_t hilosC = std::max<size_t>(1, std::min(hilos, bytesUsados));
    const std::array<size_t, NUM_BYTES> duenio = repartirLPT(peso, hilosC);

    std::array<TrieNode*, NUM_BYTES> tablaRaiz{};
    for (size_t b = 0; b < NUM_BYTES; ++b) {
        if (peso[b] == 0) continue;
        auto nuevo = std::make_unique<TrieNode>();
        tablaRaiz[b] = nuevo.get();
        nuevaRaiz->children.emplace(static_cast<char>(b), nuevo.release());
    }

    // ---- Fase C: cada hilo construye solo sus subarboles ----
    std::atomic<size_t> progreso{0};
    auto construirParticion = [&](size_t h) {
        for (size_t id = 0; id < n; ++id) {
            size_t insertadas = 0;
            for (const std::string& texto : textos[id]) {
                for (size_t i = 0; i < texto.size(); ++i) {
                    const size_t b = byteDe(texto[i]);
                    if (duenio[b] != h) continue;
                    insertarDesde(tablaRaiz[b], texto, i, KEY_SIZE, id);
                    ++insertadas;
                }
            }
            if (insertadas > 0) progreso.fetch_add(insertadas, std::memory_order_relaxed);
        }
    };

    if (observador) {
        // El hilo que llamo no inserta: solo notifica, asi nunca hay llamadas concurrentes.
        paralelo::ejecutarEnHilos(hilosC, construirParticion, [&]() {
            observador->onProgresoIndexacion(progreso.load(std::memory_order_relaxed), total);
        });
        observador->onProgresoIndexacion(total, total);
    } else {
        paralelo::ejecutarEnHilos(hilosC, construirParticion);
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

namespace {
bool nodosIguales(const TrieNode* a, const TrieNode* b) {
    if (a->movieIds != b->movieIds || a->children.size() != b->children.size()) return false;
    for (const auto& [caracter, hijoA] : a->children) {
        auto it = b->children.find(caracter);
        if (it == b->children.end() || !nodosIguales(hijoA, it->second)) return false;
    }
    return true;
}
} // namespace

bool MovieTrie::mismoContenido(const MovieTrie& otro) const {
    return nodosIguales(root, otro.root);
}
