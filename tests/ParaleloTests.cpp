// Pruebas de la indexacion y la busqueda paralelas.
//
// - El indice paralelo es identico (nodo por nodo) al secuencial con 1, 2, 4 y mas hilos
//   que nucleos.
// - La busqueda paralela devuelve lo mismo que la secuencial y que el algoritmo anterior
//   (IDs, puntajes y orden), por encima y por debajo del umbral.
// - El observador de indexacion se notifica desde un solo hilo, con avance monotono, y
//   termina en total/total.
// - Casos con muy pocas peliculas (0, 1 y menos que el numero de hilos).
//
// Usa datos sinteticos deterministas; si existe data/movies_clean.csv tambien compara con
// las primeras 400 peliculas reales.

#include "01_lectura_diagnostico/CSVReaderDiagnostic.h"
#include "02_limpieza_normalizacion/DataCleaner.h"
#include "03_arbol_insercion/MovieTrie(arbol).h"
#include "04_busqueda/Busqueda.h"
#include "04_busqueda/BusquedaComun.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

int fallas = 0;

void comprobar(bool condicion, const std::string& mensaje) {
    if (!condicion) {
        ++fallas;
        std::cerr << "FALLA: " << mensaje << "\n";
    }
}

size_t nucleos() {
    const unsigned hw = std::thread::hardware_concurrency();
    return hw == 0 ? 4 : hw;
}

// Generador pseudoaleatorio determinista (no depende de la plataforma).
struct Lcg {
    uint64_t estado;
    uint32_t siguiente() {
        estado = estado * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<uint32_t>(estado >> 33);
    }
};

std::vector<RawMovie> peliculasSinteticas(size_t cantidad) {
    const std::vector<std::string> palabras = {
        "the", "ghost", "ship", "love", "war", "star", "Arthur", "drama", "comedy", "harbor",
        "night", "of", "and", "Ana", "María", "Ñandú", "café", "x-ray", "o'brien", "a",
        "western", "horror", "John", "Smith", "Spielberg", "Tom", "Hanks", "sea", "art", "bar"};
    Lcg lcg{42};
    auto frase = [&](size_t minimo, size_t maximo) {
        const size_t n = minimo + lcg.siguiente() % (maximo - minimo + 1);
        std::string texto;
        for (size_t i = 0; i < n; ++i) {
            if (i > 0) texto += (lcg.siguiente() % 7 == 0) ? ",  " : " ";
            texto += palabras[lcg.siguiente() % palabras.size()];
        }
        return texto;
    };
    std::vector<RawMovie> peliculas(cantidad);
    for (size_t i = 0; i < cantidad; ++i) {
        RawMovie& p = peliculas[i];
        p.releaseYear = std::to_string(1900 + i % 120);
        p.title = frase(1, 4);
        p.director = frase(1, 2);
        p.cast = frase(0, 5);
        p.genre = frase(0, 2);
        p.plot = frase(5, 60);
        p.wikiPage = "https://example.org/" + std::to_string(i);
    }
    // Algunos repetidos exactos para tener empates de puntaje y titulo (desempata el id).
    for (size_t i = 10; i + 1 < cantidad && i < 20; ++i) peliculas[i + 1] = peliculas[i];
    return peliculas;
}

std::vector<RawMovie> peliculasReales(size_t cantidad) {
    for (const char* ruta : {"data/movies_clean.csv", "../data/movies_clean.csv", "../../data/movies_clean.csv"}) {
        if (std::filesystem::exists(ruta)) {
            DiagnosticReport report;
            auto peliculas = CSVReaderDiagnostic::processFile(ruta, report);
            if (peliculas.size() > cantidad) peliculas.resize(cantidad);
            return peliculas;
        }
    }
    return {};
}

bool mismosResultados(const std::vector<Resultado>& a, const std::vector<Resultado>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].id != b[i].id || a[i].prioridad != b[i].prioridad ||
            a[i].tituloOrden != b[i].tituloOrden || a[i].pelicula != b[i].pelicula) {
            return false;
        }
    }
    return true;
}

// Algoritmo de busqueda por texto ANTERIOR (normaliza en cada etapa), como referencia.
std::vector<Resultado> textoAnterior(const std::string& consulta, const MovieTrie& arbol,
                                     const std::vector<RawMovie>& peliculas) {
    using namespace busqueda_interna;
    const std::string q = DataCleaner::normalizeForSearch(consulta);
    std::vector<Resultado> resultados;
    const auto terminos = separarTerminos(q);
    if (terminos.empty()) return resultados;
    for (size_t id : arbol.searchSubstring(q)) {
        if (id >= peliculas.size()) continue;
        const RawMovie& p = peliculas[id];
        bool coincide = false;
        for (const auto* campo : {&p.title, &p.plot, &p.director, &p.cast, &p.genre}) {
            if (contiene(DataCleaner::normalizeForSearch(*campo), q)) {
                coincide = true;
                break;
            }
        }
        if (!coincide) continue;
        Resultado r = crearResultado(id, p);
        r.prioridad = puntajeTexto(p, q, terminos);
        if (r.prioridad > 0) resultados.push_back(r);
    }
    std::sort(resultados.begin(), resultados.end());
    return resultados;
}

class ObservadorPrueba : public IObservadorIndexacion {
public:
    std::vector<std::thread::id> hilos;
    std::vector<size_t> avances;
    size_t total = 0;
    bool totalConstante = true;

    void onProgresoIndexacion(size_t procesadas, size_t t) override {
        if (!avances.empty() && t != total) totalConstante = false;
        hilos.push_back(std::this_thread::get_id());
        avances.push_back(procesadas);
        total = t;
    }
};

size_t clavesEsperadas(const std::vector<RawMovie>& peliculas) {
    size_t total = 0;
    for (const auto& p : peliculas) {
        for (const auto* campo : {&p.title, &p.plot, &p.director, &p.cast, &p.genre}) {
            total += DataCleaner::normalizeForSearch(*campo).size();
        }
    }
    return total;
}

// ------------------------------------------------------------------------------------

void pruebaIndiceIdentico(const std::vector<RawMovie>& peliculas, const std::string& nombre) {
    MovieTrie secuencial;
    secuencial.buildIndex(peliculas, 1);
    for (size_t hilos : {size_t{1}, size_t{2}, size_t{4}, nucleos() * 2 + 3, size_t{0}}) {
        MovieTrie paralelo;
        paralelo.buildIndex(peliculas, hilos);
        comprobar(paralelo.mismoContenido(secuencial),
                  nombre + ": indice con " + std::to_string(hilos) + " hilos igual al secuencial");
    }
    // Reconstruir sobre un arbol ya lleno tambien da el mismo resultado.
    MovieTrie reconstruido;
    reconstruido.buildIndex(peliculasSinteticas(70), 4);
    reconstruido.buildIndex(peliculas, 4);
    comprobar(reconstruido.mismoContenido(secuencial), nombre + ": reconstruir reemplaza el indice");
}

void pruebaBusquedaIdentica(const std::vector<RawMovie>& peliculas, const std::string& nombre) {
    MovieTrie arbol;
    arbol.buildIndex(peliculas);
    const size_t muchos = nucleos() * 2 + 3;

    // Frecuentes (por encima del umbral), raras (por debajo) y sin resultados.
    for (const std::string q : {"the", "a", "ar", "love", "ghost ship", "harbor", "ñandú",
                                "café", "x-ray", "zzzz", "  ", ""}) {
        const auto esperado = textoAnterior(q, arbol, peliculas);
        const auto uno = BusquedaTexto(1).ejecutar(q, arbol, peliculas);
        comprobar(mismosResultados(esperado, uno), nombre + ": texto \"" + q + "\" 1 hilo = anterior");
        for (size_t hilos : {size_t{2}, size_t{4}, muchos}) {
            // Umbral por defecto y umbral 0 (fuerza el camino paralelo aunque haya pocos candidatos)
            const auto porDefecto = BusquedaTexto(hilos).ejecutar(q, arbol, peliculas);
            const auto forzado = BusquedaTexto(hilos, 0).ejecutar(q, arbol, peliculas);
            comprobar(mismosResultados(esperado, porDefecto),
                      nombre + ": texto \"" + q + "\" " + std::to_string(hilos) + " hilos = secuencial");
            comprobar(mismosResultados(esperado, forzado),
                      nombre + ": texto \"" + q + "\" " + std::to_string(hilos) + " hilos (umbral 0) = secuencial");
        }
    }

    for (FieldType campo : {FieldType::DIRECTOR, FieldType::CAST, FieldType::GENRE}) {
        for (const std::string q : {"a", "spielberg", "tom hanks", "drama", "john", "zzzz"}) {
            const auto uno = BusquedaTag(campo, 1).ejecutar(q, arbol, peliculas);
            for (size_t hilos : {size_t{2}, muchos}) {
                comprobar(mismosResultados(uno, BusquedaTag(campo, hilos).ejecutar(q, arbol, peliculas)),
                          nombre + ": tag \"" + q + "\" " + std::to_string(hilos) + " hilos = secuencial");
                comprobar(mismosResultados(uno, BusquedaTag(campo, hilos, 0).ejecutar(q, arbol, peliculas)),
                          nombre + ": tag \"" + q + "\" " + std::to_string(hilos) + " hilos (umbral 0) = secuencial");
            }
        }
    }
}

void pruebaObservador(const std::vector<RawMovie>& peliculas, size_t hilos) {
    const std::string nombre = "observador con " + std::to_string(hilos) + " hilos";
    ObservadorPrueba observador;
    MovieTrie arbol;
    arbol.buildIndex(peliculas, hilos, &observador);

    comprobar(!observador.avances.empty(), nombre + ": recibe notificaciones");
    if (observador.avances.empty()) return;
    const auto principal = std::this_thread::get_id();
    comprobar(std::all_of(observador.hilos.begin(), observador.hilos.end(),
                          [&](std::thread::id id) { return id == principal; }),
              nombre + ": todas las notificaciones vienen del hilo que llamo");
    comprobar(std::is_sorted(observador.avances.begin(), observador.avances.end()),
              nombre + ": el avance es monotono");
    comprobar(observador.totalConstante, nombre + ": el total no cambia");
    comprobar(observador.total == clavesEsperadas(peliculas), nombre + ": el total son las claves insertadas");
    comprobar(observador.avances.back() == observador.total, nombre + ": termina en total/total");
    comprobar(std::all_of(observador.avances.begin(), observador.avances.end(),
                          [&](size_t a) { return a <= observador.total; }),
              nombre + ": nunca supera el total");
}

void pruebaPocasPeliculas() {
    const size_t muchos = nucleos() * 2 + 3;
    for (size_t cantidad : {size_t{0}, size_t{1}, size_t{3}}) {
        const std::string nombre = std::to_string(cantidad) + " peliculas";
        const auto peliculas = peliculasSinteticas(cantidad);
        MovieTrie secuencial;
        secuencial.buildIndex(peliculas, 1);
        MovieTrie paralelo;
        ObservadorPrueba observador;
        paralelo.buildIndex(peliculas, muchos, &observador);
        comprobar(paralelo.mismoContenido(secuencial), nombre + ": indice igual al secuencial");
        comprobar(!observador.avances.empty() && observador.avances.back() == observador.total,
                  nombre + ": el observador termina en total/total");
        if (cantidad == 0) {
            comprobar(observador.total == 0, nombre + ": total 0");
            comprobar(paralelo.searchSubstring("a").empty(), nombre + ": sin candidatos");
        }
        for (const std::string q : {"a", "the", "zzzz"}) {
            comprobar(mismosResultados(BusquedaTexto(1).ejecutar(q, paralelo, peliculas),
                                       BusquedaTexto(muchos, 0).ejecutar(q, paralelo, peliculas)),
                      nombre + ": busqueda \"" + q + "\" con mas hilos que candidatos");
        }
    }
    // Apenas por encima del minimo para paralelizar y con muchos mas hilos que peliculas.
    const auto justas = peliculasSinteticas(MovieTrie::MIN_PELICULAS_PARALELO);
    MovieTrie secuencial;
    secuencial.buildIndex(justas, 1);
    MovieTrie paralelo;
    paralelo.buildIndex(justas, 300);
    comprobar(paralelo.mismoContenido(secuencial), "64 peliculas con 300 hilos: indice igual al secuencial");
}

} // namespace

int main() {
    try {
        const auto sinteticas = peliculasSinteticas(500);
        pruebaIndiceIdentico(sinteticas, "sinteticas");
        pruebaBusquedaIdentica(sinteticas, "sinteticas");
        for (size_t hilos : {size_t{1}, size_t{2}, size_t{4}, nucleos() * 2 + 3}) pruebaObservador(sinteticas, hilos);
        pruebaPocasPeliculas();

        const auto reales = peliculasReales(400);
        if (!reales.empty()) {
            pruebaIndiceIdentico(reales, "reales");
            pruebaBusquedaIdentica(reales, "reales");
            pruebaObservador(reales, 4);
        } else {
            std::cout << "(sin data/movies_clean.csv: se omiten las pruebas con datos reales)\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "Excepcion: " << e.what() << "\n";
        return 1;
    }

    if (fallas > 0) {
        std::cerr << fallas << " comprobaciones fallaron\n";
        return 1;
    }
    std::cout << "Pruebas de paralelismo: OK\n";
    return 0;
}
