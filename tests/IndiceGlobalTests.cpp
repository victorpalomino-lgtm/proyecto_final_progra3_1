#include "03_arbol_insercion/IndiceGlobal.h"

#include <atomic>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

// Ejecutable separado: el Singleton vive todo el proceso, asi su estado
// no se mezcla con las pruebas de busqueda (que usan Tries locales).

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

RawMovie pelicula(const std::string& titulo) {
    RawMovie p;
    p.title = titulo;
    return p;
}

int main() {
    try {
        IndiceGlobal& a = IndiceGlobal::instancia();
        IndiceGlobal& b = IndiceGlobal::instancia();
        require(&a == &b, "instancia() devuelve siempre el mismo objeto");

        // Desde otro hilo tambien es el mismo objeto
        IndiceGlobal* desdeOtroHilo = nullptr;
        std::thread([&]() { desdeOtroHilo = &IndiceGlobal::instancia(); }).join();
        require(desdeOtroHilo == &a, "instancia() es la misma en otro hilo");

        require(!a.estaConstruido(), "Al inicio no esta construido");
        bool lanzo = false;
        try { (void)a.arbol(); } catch (const std::logic_error&) { lanzo = true; }
        require(lanzo, "arbol() antes de construir lanza logic_error");

        // Varios hilos intentan construir a la vez: solo uno indexa
        std::atomic<int> construcciones{0};
        std::vector<std::thread> hilos;
        for (int i = 0; i < 8; ++i) {
            hilos.emplace_back([&construcciones]() {
                if (IndiceGlobal::instancia().construir({pelicula("Ghost Ship"), pelicula("Harbor")})) {
                    ++construcciones;
                }
            });
        }
        for (auto& h : hilos) h.join();
        require(construcciones == 1, "Con varios hilos se indexa una sola vez");
        require(a.estaConstruido(), "Queda construido");

        // Construir de nuevo con otros datos no reindexa
        require(!a.construir({pelicula("Zebra Movie")}), "La segunda llamada devuelve false");
        require(a.peliculas().size() == 2 && a.peliculas()[0].title == "Ghost Ship",
                "Las peliculas no cambian");
        require(a.arbol().searchSubstring("zebra").empty(), "El arbol no se reindexa");
        require(a.arbol().searchSubstring("ship").count(0) == 1, "El arbol sigue con los datos originales");

        // MovieTrie conserva su constructor publico: se puede inyectar uno distinto
        MovieTrie local;
        local.buildIndex({pelicula("Zebra Movie")});
        require(local.searchSubstring("zebra").count(0) == 1, "Trie local independiente del Singleton");
        require(a.arbol().searchSubstring("zebra").empty(), "El Trie local no toca el Singleton");

        std::cout << "Pruebas del Singleton IndiceGlobal correctas." << std::endl;
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
}
