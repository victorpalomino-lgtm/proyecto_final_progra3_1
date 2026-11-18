// Benchmark de indexacion y busqueda (Parte 2: programacion paralela).
//
// Modos (--modo):
//   indice    Tiempo de MovieTrie::buildIndex con 1 hilo (algoritmo original) y con los
//             hilos pedidos. Con --verificar compara cada arbol con el secuencial nodo por
//             nodo. Tambien mide el costo de que cada hilo escanee todas las claves.
//   busqueda  Tiempo promedio por consulta en tres versiones:
//               base  = replica del algoritmo "y/o" que normaliza en cada etapa (sin la normalizacion unica)
//               norm1 = codigo actual con 1 hilo (normalizacion unica)
//               hN    = codigo actual con N hilos
//             y verifica que todas devuelven exactamente lo mismo (id, puntaje y orden).
//   perfil    Desglose por fases de la busqueda base (replica instrumentada).
//   umbral    Barrido de 1 hilo vs N hilos (umbral 0) segun cantidad de candidatos.
//   todo      indice + busqueda + perfil (por defecto).
//
// Memoria: VmRSS antes/despues de construir y pico del proceso (VmHWM). Para obtener el
// pico de cada configuracion, ejecutar un proceso por configuracion (--modo indice --hilos H).
//
// Uso: benchmark [--modo M] [--limite N] [--reps R] [--build-reps B] [--hilos 1,2,4,12]
//                [--verificar] [--datos ruta.csv]

#include "01_lectura_diagnostico/CSVReaderDiagnostic.h"
#include "02_limpieza_normalizacion/DataCleaner.h"
#include "03_arbol_insercion/MovieTrie(arbol).h"
#include "04_busqueda/Busqueda.h"
#include "04_busqueda/BusquedaComun.h"
#include "comun/Paralelismo.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#ifdef __GLIBC__
#include <malloc.h> // malloc_trim: devolver al SO la memoria liberada antes de medir RSS
#endif

namespace {

using Reloj = std::chrono::steady_clock;

double msDesde(Reloj::time_point inicio) {
    return std::chrono::duration<double, std::milli>(Reloj::now() - inicio).count();
}

// Lee un campo de /proc/self/status en kB (VmRSS = memoria actual, VmHWM = pico).
long leerStatusKb(const std::string& campo) {
    std::ifstream status("/proc/self/status");
    std::string linea;
    while (std::getline(status, linea)) {
        if (linea.rfind(campo + ":", 0) == 0) {
            return std::strtol(linea.c_str() + campo.size() + 1, nullptr, 10);
        }
    }
    return -1;
}

double mb(long kb) { return static_cast<double>(kb) / 1024.0; }

void devolverMemoria() {
#ifdef __GLIBC__
    malloc_trim(0);
#endif
}

std::filesystem::path ubicarDatos(const std::filesystem::path& ruta) {
    for (const auto& prefijo : {std::filesystem::path("."), std::filesystem::path(".."),
                                std::filesystem::path("../..")}) {
        if (std::filesystem::exists(prefijo / ruta)) return prefijo / ruta;
    }
    return ruta;
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

// ---------------------------------------------------------------------------------------
// Replicas del codigo ANTERIOR (base), con las funciones originales de busqueda_interna.
// ---------------------------------------------------------------------------------------

struct Fases {
    double normalizarConsulta = 0, recorridoTrie = 0, verificacion = 0, puntaje = 0, ordenamiento = 0;
};

std::vector<Resultado> busquedaTextoBase(const std::string& consulta, const MovieTrie& arbol,
                                         const std::vector<RawMovie>& peliculas, Fases* f = nullptr) {
    using namespace busqueda_interna;
    auto t = Reloj::now();
    const std::string textoNormalizado = DataCleaner::normalizeForSearch(consulta);
    std::vector<Resultado> resultados;
    std::vector<std::string> terminos = separarTerminos(textoNormalizado);
    if (f) f->normalizarConsulta += msDesde(t);
    if (terminos.empty()) return resultados;

    t = Reloj::now();
    const auto ids = candidatosTexto(arbol, terminos); // "y/o": union por palabra
    if (f) f->recorridoTrie += msDesde(t);

    for (size_t id : ids) {
        if (id >= peliculas.size()) continue;
        Reloj::time_point tv;
        if (f) tv = Reloj::now();
        const auto& pelicula = peliculas[id];
        // Verificacion: cuantas palabras aparecen (normalizando cada campo en cada etapa).
        auto aparece = [&](const std::string& texto) {
            for (const auto* campo : {&pelicula.title, &pelicula.plot, &pelicula.director,
                                      &pelicula.cast, &pelicula.genre}) {
                if (contiene(DataCleaner::normalizeForSearch(*campo), texto)) return true;
            }
            return false;
        };
        int nivel = 0;
        for (const std::string& termino : terminos) {
            if (aparece(termino)) ++nivel;
        }
        if (nivel > 0 && terminos.size() > 1 && aparece(textoNormalizado)) ++nivel;
        if (f) f->verificacion += msDesde(tv);
        if (nivel == 0) continue;
        if (f) tv = Reloj::now();
        Resultado resultado = crearResultado(id, peliculas[id]);
        resultado.prioridad = std::min(nivel, NIVEL_MAXIMO) * PESO_NIVEL +
                              puntajeTexto(peliculas[id], textoNormalizado, terminos);
        resultados.push_back(resultado);
        if (f) f->puntaje += msDesde(tv);
    }

    t = Reloj::now();
    std::sort(resultados.begin(), resultados.end());
    if (f) f->ordenamiento += msDesde(t);
    return resultados;
}

const std::string& valorCampo(const RawMovie& p, FieldType campo) {
    switch (campo) {
        case FieldType::TITLE: return p.title;
        case FieldType::PLOT: return p.plot;
        case FieldType::DIRECTOR: return p.director;
        case FieldType::CAST: return p.cast;
        case FieldType::GENRE: return p.genre;
    }
    return p.title;
}

std::vector<Resultado> busquedaTagBase(FieldType tipo, const std::string& consulta, const MovieTrie& arbol,
                                       const std::vector<RawMovie>& peliculas) {
    using namespace busqueda_interna;
    const std::string textoNormalizado = DataCleaner::normalizeForSearch(consulta);
    std::vector<Resultado> resultados;
    std::vector<std::string> terminos = separarTerminos(textoNormalizado);
    if (terminos.empty()) return resultados;
    const auto candidatos = arbol.searchSubstring(textoNormalizado);
    for (size_t id : candidatos) {
        if (id >= peliculas.size()) continue;
        const RawMovie& pelicula = peliculas[id];
        if (!contiene(DataCleaner::normalizeForSearch(valorCampo(pelicula, tipo)), textoNormalizado)) continue;
        Resultado resultado = crearResultado(id, pelicula);
        resultado.prioridad = puntajeCampo(valorCampo(pelicula, tipo), textoNormalizado, terminos);
        if (resultado.prioridad > 0) resultados.push_back(resultado);
    }
    std::sort(resultados.begin(), resultados.end());
    return resultados;
}

// ---------------------------------------------------------------------------------------

struct Opciones {
    std::string modo = "todo";
    size_t limite = 0;
    int reps = 10;
    int buildReps = 1;
    std::vector<size_t> hilos;
    bool verificar = false;
    std::string datos = "data/movies_clean.csv";
};

std::vector<size_t> leerLista(const std::string& texto) {
    std::vector<size_t> lista;
    std::stringstream ss(texto);
    std::string item;
    while (std::getline(ss, item, ',')) lista.push_back(std::stoul(item));
    return lista;
}

struct Medicion {
    double promedio = 0, minimo = 0;
};

Medicion medir(int reps, const std::function<void()>& f) {
    f(); // calentamiento
    Medicion m;
    m.minimo = 1e18;
    double suma = 0;
    for (int r = 0; r < reps; ++r) {
        auto t = Reloj::now();
        f();
        const double ms = msDesde(t);
        suma += ms;
        m.minimo = std::min(m.minimo, ms);
    }
    m.promedio = suma / reps;
    return m;
}

// ---------------------------------------------------------------------------------------

void benchIndice(const Opciones& op, const std::vector<RawMovie>& peliculas) {
    std::cout << "\n== Indexacion (buildIndex, " << op.buildReps << " rep(s) por configuracion) ==\n";

    // Costo de normalizar y de que UN hilo escanee todas las claves (lo que hace cada hilo
    // en la fase C del algoritmo paralelo, sin insertar).
    auto t = Reloj::now();
    std::vector<std::array<std::string, 5>> textos(peliculas.size());
    for (size_t id = 0; id < peliculas.size(); ++id) {
        const auto& p = peliculas[id];
        textos[id] = {DataCleaner::normalizeForSearch(p.title), DataCleaner::normalizeForSearch(p.plot),
                      DataCleaner::normalizeForSearch(p.director), DataCleaner::normalizeForSearch(p.cast),
                      DataCleaner::normalizeForSearch(p.genre)};
    }
    const double msNormalizar = msDesde(t);
    std::array<size_t, 256> duenio{};
    for (size_t b = 0; b < 256; ++b) duenio[b] = b % 12;
    t = Reloj::now();
    size_t propias = 0, caracteres = 0;
    for (const auto& campos : textos) {
        for (const auto& texto : campos) {
            caracteres += texto.size();
            for (char c : texto) propias += (duenio[static_cast<unsigned char>(c)] == 0);
        }
    }
    const double msEscaneo = msDesde(t);
    size_t bytesTextos = 0;
    for (const auto& campos : textos) for (const auto& s : campos) bytesTextos += s.capacity() + sizeof(s);
    std::cout << "  normalizar todo (1 hilo): " << msNormalizar << " ms | escanear todas las claves (1 hilo): "
              << msEscaneo << " ms (" << caracteres << " claves, " << propias << " propias del hilo 0)\n"
              << "  memoria de los textos normalizados (fase A): ~" << bytesTextos / (1024.0 * 1024.0) << " MB\n";
    textos.clear();
    textos.shrink_to_fit();
    devolverMemoria();

    std::unique_ptr<MovieTrie> referencia;
    if (op.verificar) {
        referencia = std::make_unique<MovieTrie>();
        referencia->buildIndex(peliculas, 1);
    }

    double base = 0;
    std::cout << std::setw(8) << "hilos" << std::setw(14) << "prom ms" << std::setw(14) << "min ms"
              << std::setw(10) << "speedup" << std::setw(14) << "RSS indice" << std::setw(12) << "identico" << "\n";
    for (size_t h : op.hilos) {
        std::vector<double> tiempos;
        long rssIndice = 0;
        for (int r = 0; r < op.buildReps; ++r) {
            devolverMemoria();
            const long rssAntes = leerStatusKb("VmRSS");
            auto arbol = std::make_unique<MovieTrie>();
            t = Reloj::now();
            arbol->buildIndex(peliculas, h);
            tiempos.push_back(msDesde(t));
            rssIndice = leerStatusKb("VmRSS") - rssAntes;
            if (r == op.buildReps - 1 && referencia) {
                const bool igual = arbol->mismoContenido(*referencia);
                std::cout << std::setw(8) << h;
                double prom = 0;
                for (double x : tiempos) prom += x;
                prom /= static_cast<double>(tiempos.size());
                if (base == 0) base = prom;
                std::cout << std::setw(14) << prom << std::setw(14) << *std::min_element(tiempos.begin(), tiempos.end())
                          << std::setw(9) << base / prom << "x" << std::setw(11) << mb(rssIndice) << " MB"
                          << std::setw(12) << (igual ? "si" : "NO") << "\n";
                if (!igual) throw std::runtime_error("El indice paralelo no coincide con el secuencial");
                tiempos.clear();
            }
        }
        if (!tiempos.empty()) {
            double prom = 0;
            for (double x : tiempos) prom += x;
            prom /= static_cast<double>(tiempos.size());
            if (base == 0) base = prom;
            std::cout << std::setw(8) << h << std::setw(14) << prom << std::setw(14)
                      << *std::min_element(tiempos.begin(), tiempos.end()) << std::setw(9) << base / prom << "x"
                      << std::setw(11) << mb(rssIndice) << " MB" << std::setw(12) << "-" << "\n";
        }
    }
}

struct ConsultaBench {
    std::string tipo;
    std::string texto;
    FieldType campo = FieldType::TITLE;
};

const std::vector<ConsultaBench>& consultasBench() {
    static const std::vector<ConsultaBench> lista = {
        {"texto", "ghost ship"}, {"texto", "zxqv"}, {"texto", "the"}, {"texto", "love"},
        {"texto", "ar"}, {"texto", "a"},
        {"director", "spielberg", FieldType::DIRECTOR}, {"reparto", "john", FieldType::CAST},
        {"genero", "drama", FieldType::GENRE},
    };
    return lista;
}

std::unique_ptr<EstrategiaBusqueda> crearEstrategia(const ConsultaBench& c, size_t hilos, size_t umbral) {
    if (c.tipo == "texto") return std::make_unique<BusquedaTexto>(hilos, umbral);
    return std::make_unique<BusquedaTag>(c.campo, hilos, umbral);
}

std::unique_ptr<EstrategiaBusqueda> crearEstrategia(const ConsultaBench& c, size_t hilos) {
    if (c.tipo == "texto") return std::make_unique<BusquedaTexto>(hilos);
    return std::make_unique<BusquedaTag>(c.campo, hilos);
}

void benchBusqueda(const Opciones& op, const MovieTrie& arbol, const std::vector<RawMovie>& peliculas) {
    std::cout << "\n== Busqueda (" << op.reps << " reps, promedio ms; umbral por defecto) ==\n";
    std::cout << std::left << std::setw(9) << "tipo" << std::setw(13) << "consulta" << std::right
              << std::setw(8) << "cand." << std::setw(8) << "res." << std::setw(10) << "base"
              << std::setw(10) << "norm1";
    for (size_t h : op.hilos) {
        if (h != 1) std::cout << std::setw(9) << ("h" + std::to_string(h));
    }
    std::cout << std::setw(10) << "mejor x" << "  identico\n";

    for (const auto& c : consultasBench()) {
        const size_t candidatos = c.tipo == "texto"
            ? busqueda_interna::candidatosTexto(arbol, busqueda_interna::separarTerminos(
                  DataCleaner::normalizeForSearch(c.texto))).size()
            : arbol.searchSubstring(c.texto).size();
        auto ejecutarBase = [&]() {
            return c.tipo == "texto" ? busquedaTextoBase(c.texto, arbol, peliculas)
                                     : busquedaTagBase(c.campo, c.texto, arbol, peliculas);
        };
        const auto esperado = ejecutarBase();
        bool identico = true;
        const Medicion base = medir(op.reps, [&]() { (void)ejecutarBase(); });

        auto norm1 = crearEstrategia(c, 1);
        identico = identico && mismosResultados(esperado, norm1->ejecutar(c.texto, arbol, peliculas));
        const Medicion m1 = medir(op.reps, [&]() { (void)norm1->ejecutar(c.texto, arbol, peliculas); });

        std::cout << std::left << std::setw(9) << c.tipo << std::setw(13) << ("\"" + c.texto + "\"") << std::right
                  << std::setw(8) << candidatos << std::setw(8) << esperado.size() << std::setw(10) << base.promedio
                  << std::setw(10) << m1.promedio;
        double mejor = m1.promedio;
        for (size_t h : op.hilos) {
            if (h == 1) continue;
            auto est = crearEstrategia(c, h);
            identico = identico && mismosResultados(esperado, est->ejecutar(c.texto, arbol, peliculas));
            const Medicion mh = medir(op.reps, [&]() { (void)est->ejecutar(c.texto, arbol, peliculas); });
            mejor = std::min(mejor, mh.promedio);
            std::cout << std::setw(9) << mh.promedio;
        }
        std::cout << std::setw(9) << (mejor > 0 ? base.promedio / mejor : 0.0) << "x" << "  "
                  << (identico ? "si" : "NO") << "\n";
        if (!identico) throw std::runtime_error("Los resultados no coinciden con la version base");
    }
}

void benchPerfil(const Opciones& op, const MovieTrie& arbol, const std::vector<RawMovie>& peliculas) {
    std::cout << "\n== Perfil de la busqueda de texto BASE por fases (" << op.reps << " reps, promedio ms) ==\n";
    std::cout << std::left << std::setw(14) << "consulta" << std::right << std::setw(10) << "normQ"
              << std::setw(10) << "trie" << std::setw(12) << "verific." << std::setw(12) << "puntaje"
              << std::setw(10) << "sort" << "\n";
    for (const std::string q : {"ghost ship", "the", "love", "ar", "a"}) {
        Fases f;
        for (int r = 0; r < op.reps; ++r) (void)busquedaTextoBase(q, arbol, peliculas, &f);
        std::cout << std::left << std::setw(14) << ("\"" + q + "\"") << std::right
                  << std::setw(10) << f.normalizarConsulta / op.reps << std::setw(10) << f.recorridoTrie / op.reps
                  << std::setw(12) << f.verificacion / op.reps << std::setw(12) << f.puntaje / op.reps
                  << std::setw(10) << f.ordenamiento / op.reps << "\n";
    }
}

// Barrido para elegir el umbral: 1 hilo vs N hilos forzados (umbral 0) con distintas
// cantidades de candidatos (se varia el tamanio del dataset con el prefijo de peliculas).
void benchUmbral(const Opciones& op, const std::vector<RawMovie>& todas) {
    const size_t n = paralelo::resolverHilos(0);
    std::cout << "\n== Barrido de umbral: 1 hilo vs " << n << " hilos forzados (promedio ms) ==\n";
    std::cout << std::setw(10) << "peliculas" << std::setw(10) << "consulta" << std::setw(8) << "cand."
              << std::setw(10) << "1 hilo" << std::setw(10) << "N hilos" << "\n";
    for (size_t tam : {8, 16, 32, 64, 128, 256, 1000, 2000, 4000, 8000, 16000, 34886}) {
        if (tam > todas.size()) break;
        std::vector<RawMovie> peliculas(todas.begin(), todas.begin() + static_cast<long>(tam));
        MovieTrie arbol;
        arbol.buildIndex(peliculas);
        for (const ConsultaBench& c : {ConsultaBench{"texto", "the"},
                                       ConsultaBench{"genero", "drama", FieldType::GENRE},
                                       ConsultaBench{"director", "a", FieldType::DIRECTOR}}) {
            auto uno = crearEstrategia(c, 1, 0);
            auto varios = crearEstrategia(c, n, 0);
            const Medicion m1 = medir(op.reps, [&]() { (void)uno->ejecutar(c.texto, arbol, peliculas); });
            const Medicion mn = medir(op.reps, [&]() { (void)varios->ejecutar(c.texto, arbol, peliculas); });
            std::cout << std::setw(10) << tam << std::setw(10) << c.texto << std::setw(8)
                      << arbol.searchSubstring(c.texto).size() << std::setw(10) << m1.promedio << std::setw(10)
                      << mn.promedio << "\n";
        }
    }
}

} // namespace

int main(int argc, char* argv[]) {
    Opciones op;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--modo" && i + 1 < argc) op.modo = argv[++i];
        else if (arg == "--limite" && i + 1 < argc) op.limite = std::stoul(argv[++i]);
        else if (arg == "--reps" && i + 1 < argc) op.reps = std::stoi(argv[++i]);
        else if (arg == "--build-reps" && i + 1 < argc) op.buildReps = std::stoi(argv[++i]);
        else if (arg == "--hilos" && i + 1 < argc) op.hilos = leerLista(argv[++i]);
        else if (arg == "--verificar") op.verificar = true;
        else if (arg == "--datos" && i + 1 < argc) op.datos = argv[++i];
        else {
            std::cerr << "Uso: " << argv[0] << " [--modo indice|busqueda|perfil|umbral|todo] [--limite N]"
                      << " [--reps R] [--build-reps B] [--hilos 1,2,4,12] [--verificar] [--datos ruta.csv]\n";
            return 1;
        }
    }
    if (op.hilos.empty()) {
        op.hilos = {1, 2, 4};
        const size_t n = paralelo::resolverHilos(0);
        if (n > 4) op.hilos.push_back(n);
    }

    try {
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "hardware_concurrency = " << std::thread::hardware_concurrency() << "\n";

        DiagnosticReport report;
        auto t = Reloj::now();
        std::vector<RawMovie> peliculas = CSVReaderDiagnostic::processFile(ubicarDatos(op.datos).string(), report);
        const double msCarga = msDesde(t);
        if (peliculas.empty()) throw std::runtime_error("No se pudo leer el CSV");
        if (op.limite > 0 && op.limite < peliculas.size()) {
            peliculas.resize(op.limite);
            peliculas.shrink_to_fit();
        }
        devolverMemoria();
        std::cout << "Peliculas: " << peliculas.size() << "  (lectura CSV " << msCarga << " ms)\n";

        if (op.modo == "umbral") {
            benchUmbral(op, peliculas);
            return 0;
        }
        if (op.modo == "indice" || op.modo == "todo") benchIndice(op, peliculas);
        if (op.modo == "busqueda" || op.modo == "perfil" || op.modo == "todo") {
            MovieTrie arbol;
            arbol.buildIndex(peliculas); // hilos automaticos: solo para preparar el indice
            if (op.modo != "perfil") benchBusqueda(op, arbol, peliculas);
            if (op.modo != "busqueda") benchPerfil(op, arbol, peliculas);
        }
        std::cout << "\nPico de memoria del proceso (VmHWM): " << mb(leerStatusKb("VmHWM")) << " MB\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
