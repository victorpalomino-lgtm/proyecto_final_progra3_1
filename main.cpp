#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <iomanip>
#include "RawMovie.h"
#include "DataCleaner.h"
#include "CSVWriter.h"
#include <filesystem>
#include "MovieTrie(arbol).h"
#include "Interfaz.h"
#include "Utilidades.h"
#include <cstdlib>


// Contenedor de metricas para el diagnostico
struct DiagnosticReport {
    size_t totalRows= 0;
    size_t malformedRows= 0;
    size_t duplicateWikiPages= 0;
    size_t duplicateTitleYear= 0;
    size_t nonAsciiChars= 0;


    std::unordered_map<std::string, size_t> emptyFields; // conteo de campos vacios o desconocidos por columna
    std::unordered_map<std::string, size_t> unknownFields;
};

class CSVReaderDiagnostic {
private:
    // Parser que procesa caracter por caracter para respetar saltos de linea y comillas ("")
    static std::vector<std::string> parseNextRecord(std::istream& in) {
        std::vector<std::string> fields;
        std::string current;
        bool inQuotes= false;
        char c;

        while (in.get(c)) {
            if (c== '"') {
                if (inQuotes && in.peek() == '"') {
                    current+= '"';
                    in.get(); // Consumir comilla doble escapada
                } else {
                    inQuotes= !inQuotes;
                }
            } else if (c== ',' && !inQuotes) {
                fields.push_back(current);
                current.clear();
            } else if ((c== '\r' || c== '\n') && !inQuotes) {
                if (c== '\r' && in.peek()== '\n') {
                    in.get(); // Consumir formato de salto de linea CRLF (\r\n)
                }
                if (!fields.empty() || !current.empty()) {
                    fields.push_back(current);
                    return fields;
                }
            } else {
                current+= c;
            }
        }

        if (!fields.empty() || !current.empty()) {
            fields.push_back(current);
        }
        return fields;
    }

    // Identifica caracteres no ASCII (tildes, acentos o codificacion corrupta)
    static size_t countNonAscii(const std::string& text) {
        size_t count= 0;
        for (unsigned char c: text) {
            if (c> 127) {
                count++;
            }
        }
        return count;
    }

public:
    static std::vector<RawMovie> processFile(const std::string& filepath, DiagnosticReport& report) {
        std::ifstream file(filepath, std::ios::binary);
        std::vector<RawMovie> movies;

        if (!file.is_open()) {
            // Intento alternativo en caso de ejecutar desde cmake-build-debug
            std::string altPath= "../"+ filepath;
            file.open(altPath, std::ios::binary);
            if (!file.is_open()) {
                std::cerr <<"Error: No se pudo abrir el archivo CSV en: "<< filepath <<" ni en "
                <<altPath << std::endl;
                return movies;
            }
        }

        //Lectura de cabeceras
        std::vector<std::string> header= parseNextRecord(file);
        if (header.empty()) {
            std::cerr << "Error: El archivo CSV esta vacio." << std::endl;
            return movies;
        }

        std::unordered_map<std::string, int> colMap;
        for (size_t i = 0; i < header.size(); ++i) {
            colMap[header[i]]= static_cast<int>(i);
        }

        // Mapeo exacto segun wiki_movie_plots_deduped.csv
        int idxYear= colMap.count("Release Year") ? colMap["Release Year"]: -1;
        int idxTitle= colMap.count("Title") ? colMap["Title"]: -1;
        int idxOrigin= colMap.count("Origin/Ethnicity") ? colMap["Origin/Ethnicity"]: -1;
        int idxDirector= colMap.count("Director") ? colMap["Director"]: -1;
        int idxCast= colMap.count("Cast") ? colMap["Cast"]: -1;
        int idxGenre= colMap.count("Genre") ? colMap["Genre"]: -1;
        int idxWiki= colMap.count("Wiki Page") ? colMap["Wiki Page"]: -1;
        int idxPlot= colMap.count("Plot") ? colMap["Plot"]: -1;

        std::unordered_set<std::string> seenWikiPages;
        std::unordered_set<std::string> seenTitleYear;
        const size_t expectedCols= header.size();

        // Extraccion y conteo de anomalias
        while (true) {
            std::vector<std::string> row= parseNextRecord(file);
            if (row.empty()) break;

            report.totalRows++;

            if (row.size()!= expectedCols) {
                report.malformedRows++;
            }

            auto getField= [&](int idx, const std::string& name) -> std::string {
                if (idx< 0 || idx>= static_cast<int>(row.size())) {
                    report.emptyFields[name]++;
                    return "";
                }
                const std::string& val= row[idx];
                if (val.empty()) {
                    report.emptyFields[name]++;
                } else if (val=="Unknown" || val=="unknown") {
                    report.unknownFields[name]++;
                }
                report.nonAsciiChars+= countNonAscii(val);
                return val;
            };

            RawMovie m;
            m.releaseYear= getField(idxYear, "Release Year");
            m.title= getField(idxTitle, "Title");
            m.origin= getField(idxOrigin, "Origin/Ethnicity");
            m.director= getField(idxDirector, "Director");
            m.cast= getField(idxCast, "Cast");
            m.genre= getField(idxGenre, "Genre");
            m.wikiPage= getField(idxWiki, "Wiki Page");
            m.plot= getField(idxPlot, "Plot");

            // Diagnostico de duplicados
            if (!m.wikiPage.empty()) {
                if (seenWikiPages.count(m.wikiPage)) report.duplicateWikiPages++;
                else seenWikiPages.insert(m.wikiPage);
            }

            if (!m.title.empty() && !m.releaseYear.empty()) {
                std::string compositeKey= m.title+ " ("+ m.releaseYear+ ")";
                if (seenTitleYear.count(compositeKey)) report.duplicateTitleYear++;
                else seenTitleYear.insert(compositeKey);
            }

            movies.push_back(m);
        }

        return movies;
    }

    static void printReport(const DiagnosticReport& r) {
        std::cout <<"\n=======================================================" << std::endl;
        std::cout <<"        INFORME DE DIAGNOSTICO - CSV ORIGINAL          " << std::endl;
        std::cout <<"=======================================================" << std::endl;
        std::cout <<"Total de filas leidas: "<< r.totalRows << std::endl;
        std::cout <<"Filas con columnas inconsistentes: "<< r.malformedRows << std::endl;
        std::cout <<"Paginas Wiki duplicadas: "<< r.duplicateWikiPages << std::endl;
        std::cout <<"Peliculas duplicadas (Titulo+Ano): "<< r.duplicateTitleYear << std::endl;
        std::cout <<"Caracteres especiales/no ASCII: "<< r.nonAsciiChars << std::endl;
        std::cout <<"-------------------------------------------------------"<< std::endl;
        std::cout<< "Valores faltantes por columna:"<< std::endl;

        std::vector<std::string> displayOrder= {
            "Release Year", "Title", "Origin/Ethnicity",
            "Director", "Cast", "Genre", "Wiki Page", "Plot"
        };

        for (const auto& col: displayOrder) {
            size_t emptyCount= r.emptyFields.count(col) ? r.emptyFields.at(col): 0;
            size_t unkCount= r.unknownFields.count(col) ? r.unknownFields.at(col): 0;
            double emptyPct= (r.totalRows> 0) ? (static_cast<double>(emptyCount) / r.totalRows)* 100.0: 0.0;

            std::cout <<" - "<< std::left << std::setw(18) << col
                      <<" | Vacios: "<< std::setw(6) << emptyCount
                      <<" (" << std::fixed << std::setprecision(1) << std::setw(4)<< emptyPct << "%)"
                      <<" | 'Unknown': "<< unkCount << std::endl;
        }
        std::cout << "=======================================================\n" << std::endl;
    }
};

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
                limite = std::stoul(argv[i + 1]);
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
            std::exit(1);
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
