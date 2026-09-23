#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <iomanip>
#include "comun/RawMovie.h"
#include "02_limpieza_normalizacion/DataCleaner.h"
#include "02_limpieza_normalizacion/CSVWriter.h"
#include <filesystem>
#include "03_arbol_insercion/MovieTrie(arbol).h"
#include "05_interfaz_integracion/Interfaz.h"
#include "comun/Utilidades.h"
#include <cstdlib>


#include "01_lectura_diagnostico/CSVReaderDiagnostic.h"

// Mantener la ejecucion desde la raiz o desde un directorio de build hijo.
std::filesystem::path locateDataFile(const std::filesystem::path& path) {
    if (std::filesystem::exists(path)) return path;
    const auto alternative = std::filesystem::path("..") / path;
    return std::filesystem::exists(alternative) ? alternative : path;
}

// CSV - limpieza - arbol - busqueda - resultados
int main(int argc, char* argv[]) {
    bool rebuild = false;
    size_t limite = 0; // 0 significa usar todas las peliculas

    for (int i = 1; i < argc; i++) {
        std::string argumento = argv[i];
        if (argumento == "--rebuild-clean") {
            rebuild = true;
        } else if (argumento == "--limite" && i + 1 < argc) {
            try {
                const std::string value = argv[i + 1];
                if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos)
                    throw std::invalid_argument("limite");
                limite = std::stoul(value);
            } catch (const std::exception&) {
                limite = 0;
            }
            if (limite == 0) {
                std::cerr << "--limite necesita un numero mayor que 0." << std::endl;
                return 1;
            }
            i++;
        } else {
            std::cerr << "Uso: " << argv[0] << " [--rebuild-clean] [--limite N]" << std::endl;
            return 1;
        }
    }

    Interfaz::prepararConsola();
    Interfaz::limpiarPantalla();
    Interfaz::mostrarCabecera();
    std::cout << " Cargando la base de datos..." << std::endl;
    std::cout << std::endl;

    try {
        DiagnosticReport report;
        std::vector<RawMovie> movies;
        Cronometro reloj;
        auto cleanPath = locateDataFile("data/movies_clean.csv");

        if (!rebuild && std::filesystem::exists(cleanPath)) {
            movies = CSVReaderDiagnostic::processFile(cleanPath.string(), report);
            if (movies.empty() || report.malformedRows != 0) {
                std::cerr << "CSV limpio vacio o con filas inconsistentes. "
                        "Regeneralo con --rebuild-clean." << std::endl;
                return 1;
            }
            Interfaz::mostrarEtapa("[1/3] Lectura del CSV",
                                std::to_string(movies.size()) + " peliculas (" + reloj.texto() + ")");

            Interfaz::mostrarEtapa("[2/3] Limpieza", "ya aplicada, se usa " + cleanPath.string());
        } else {
            const auto csvPath = locateDataFile("data/wiki_movie_plots_deduped.csv");
            cleanPath = csvPath.parent_path() / "movies_clean.csv";

            movies = CSVReaderDiagnostic::processFile(csvPath.string(), report);
            if (movies.empty()) {
                return 1;
            }
            Interfaz::mostrarEtapa("[1/3] Lectura del CSV original",
                                std::to_string(movies.size()) + " registros (" + reloj.texto() + ")");
            CSVReaderDiagnostic::printReport(report);

            if (report.malformedRows != 0) {
                std::cerr << "CSV original con filas inconsistentes; corrijalo antes de regenerar." << std::endl;
                return 1;
            }

            // 2) Limpieza y guardado del CSV limpio (Integrante 2)
            reloj.reiniciar();
            const auto preprocessing = DataCleaner::clean(movies);
            CSVWriter::writeFile(cleanPath, movies);
            Interfaz::mostrarEtapa("[2/3] Limpieza",
                                std::to_string(preprocessing.processedRecords) + " registros limpios ("
                                + reloj.texto() + ")");
            DataCleaner::printReport(preprocessing, std::cout);
            std::cout << "CSV limpio guardado en: " << cleanPath.string() << std::endl;
            std::cout << std::endl;
        }

        // Opcion para hacer pruebas rapidas con menos peliculas
        if (limite > 0 && limite < movies.size()) {
            movies.resize(limite);
            std::cout << " Aviso: solo se usan las primeras " << limite << " peliculas (--limite)." << std::endl;
        }

        MovieTrie trie;
        if (!Interfaz::construirArbol(trie, movies)) {
            return 1;
        }

        std::cout << std::endl;
        Interfaz::pausa(" Todo listo. Presione Enter para ir al menu principal...");
        Interfaz interfaz(movies, trie);
        interfaz.ejecutar();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << std::endl;
        return 1;
    }
}
