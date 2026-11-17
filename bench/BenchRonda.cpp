// Una "ronda" de medicion: construye el indice una vez y ejecuta las 8 consultas del
// benchmark. Lo usa bench/comparar_versiones.sh, que lo compila dos veces:
//   - contra el codigo BASE (por ejemplo origin/main, sin paralelismo), y
//   - contra el codigo actual con -DCON_HILOS (acepta --hilos N).
// Solo usa la API comun a ambas versiones (MovieTrie::buildIndex(movies),
// BusquedaTexto(), BusquedaTag(campo)), asi la base es el codigo original real.
//
// Salida (una metrica por linea, "clave valor"), para que el script la procese:
//   build_ms, hwm_build_mb, q_<consulta>_ms, q_<consulta>_n, q_<consulta>_chk
// chk es un hash del orden de (id, puntaje) de los resultados: si coincide entre
// versiones, los resultados son identicos.
//
// Uso: bench_ronda [--limite N] [--hilos N] [--datos ruta.csv]

#include "01_lectura_diagnostico/CSVReaderDiagnostic.h"
#include "03_arbol_insercion/MovieTrie(arbol).h"
#include "04_busqueda/Busqueda.h"
#ifdef CON_HILOS
#include "comun/Paralelismo.h"
#endif

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

using Reloj = std::chrono::steady_clock;

double msDesde(Reloj::time_point inicio) {
    return std::chrono::duration<double, std::milli>(Reloj::now() - inicio).count();
}

double leerStatusMb(const std::string& campo) {
    std::ifstream status("/proc/self/status");
    std::string linea;
    while (std::getline(status, linea)) {
        if (linea.rfind(campo + ":", 0) == 0) {
            return static_cast<double>(std::strtol(linea.c_str() + campo.size() + 1, nullptr, 10)) / 1024.0;
        }
    }
    return -1;
}

uint64_t hashResultados(const std::vector<Resultado>& resultados) {
    uint64_t h = 1469598103934665603ULL; // FNV-1a
    auto mezclar = [&](uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            h ^= (v >> (8 * i)) & 0xFF;
            h *= 1099511628211ULL;
        }
    };
    for (const Resultado& r : resultados) {
        mezclar(r.id);
        mezclar(static_cast<uint64_t>(static_cast<int64_t>(r.prioridad)));
    }
    return h;
}

struct Consulta {
    std::string nombre;
    std::string texto;
    int tipo; // 0 texto, 1 director, 2 reparto, 3 genero
};

std::unique_ptr<EstrategiaBusqueda> estrategia(int tipo) {
    switch (tipo) {
        case 1: return std::make_unique<BusquedaTag>(FieldType::DIRECTOR);
        case 2: return std::make_unique<BusquedaTag>(FieldType::CAST);
        case 3: return std::make_unique<BusquedaTag>(FieldType::GENRE);
        default: return std::make_unique<BusquedaTexto>();
    }
}

} // namespace

int main(int argc, char* argv[]) {
    size_t limite = 0;
    std::string datos = "data/movies_clean.csv";
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--limite" && i + 1 < argc) {
            limite = std::stoul(argv[++i]);
        } else if (arg == "--datos" && i + 1 < argc) {
            datos = argv[++i];
        } else if (arg == "--hilos" && i + 1 < argc) {
#ifdef CON_HILOS
            paralelo::fijarHilosPorDefecto(std::stoul(argv[++i]));
#else
            std::cerr << "Esta version (base) no acepta --hilos\n";
            return 1;
#endif
        } else {
            std::cerr << "Uso: " << argv[0] << " [--limite N] [--hilos N] [--datos ruta.csv]\n";
            return 1;
        }
    }

    DiagnosticReport report;
    std::vector<RawMovie> peliculas = CSVReaderDiagnostic::processFile(datos, report);
    if (peliculas.empty()) {
        std::cerr << "No se pudo leer " << datos << "\n";
        return 1;
    }
    if (limite > 0 && limite < peliculas.size()) peliculas.resize(limite);

    std::cout << std::fixed << std::setprecision(3);
    MovieTrie arbol;
    auto t = Reloj::now();
    arbol.buildIndex(peliculas);
    std::cout << "build_ms " << msDesde(t) << "\n";
    std::cout << "hwm_build_mb " << leerStatusMb("VmHWM") << "\n";

    const std::vector<Consulta> consultas = {
        {"ghost_ship", "ghost ship", 0}, {"the", "the", 0}, {"love", "love", 0}, {"ar", "ar", 0},
        {"a", "a", 0}, {"dir_spielberg", "spielberg", 1}, {"rep_john", "john", 2}, {"gen_drama", "drama", 3},
    };
    for (const Consulta& c : consultas) {
        auto e = estrategia(c.tipo);
        (void)e->ejecutar(c.texto, arbol, peliculas); // calentamiento (no se mide)
        t = Reloj::now();
        const std::vector<Resultado> resultados = e->ejecutar(c.texto, arbol, peliculas);
        const double ms = msDesde(t);
        std::cout << "q_" << c.nombre << "_ms " << ms << "\n"
                  << "q_" << c.nombre << "_n " << resultados.size() << "\n"
                  << "q_" << c.nombre << "_chk " << hashResultados(resultados) << "\n";
    }
    return 0;
}
