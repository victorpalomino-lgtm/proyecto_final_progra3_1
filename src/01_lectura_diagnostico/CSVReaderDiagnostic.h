#pragma once
#include <fstream>
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "comun/RawMovie.h"
#include "02_limpieza_normalizacion/DataCleaner.h"

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
        bool closedQuote = false;
        char c;

        while (in.get(c)) {
            if (c== '"') {
                if (inQuotes && in.peek() == '"') {
                    current+= '"';
                    in.get(); // Consumir comilla doble escapada
                } else {
                    if (!inQuotes && (!current.empty() || closedQuote))
                        throw std::runtime_error("CSV invalido: comilla dentro de campo sin entrecomillar.");
                    if (inQuotes) closedQuote = true;
                    inQuotes= !inQuotes;
                }
            } else if (c== ',' && !inQuotes) {
                fields.push_back(current);
                current.clear();
                closedQuote = false;
            } else if ((c== '\r' || c== '\n') && !inQuotes) {
                if (c== '\r' && in.peek()== '\n') {
                    in.get(); // Consumir formato de salto de linea CRLF (\r\n)
                }
                if (!fields.empty() || !current.empty() || closedQuote) {
                    fields.push_back(current);
                    return fields;
                }
            } else {
                if (closedQuote && !inQuotes)
                    throw std::runtime_error("CSV invalido: texto despues de comilla de cierre.");
                current+= c;
            }
        }

        if (inQuotes) throw std::runtime_error("CSV invalido: comillas sin cerrar.");
        if (!fields.empty() || !current.empty() || closedQuote) {
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
        report = {};
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

        if (header[0].compare(0, 3, "\xEF\xBB\xBF") == 0) header[0].erase(0, 3);
        std::unordered_map<std::string, int> colMap;
        for (size_t i = 0; i < header.size(); ++i) {
            if (!colMap.emplace(header[i], static_cast<int>(i)).second)
                throw std::runtime_error("Columna duplicada: " + header[i]);
        }

        for (const auto* name : {"Release Year", "Title", "Origin/Ethnicity", "Director",
                                 "Cast", "Genre", "Wiki Page", "Plot"}) {
            if (!colMap.count(name)) throw std::runtime_error(std::string("Falta columna requerida: ") + name);
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
                continue; // Nunca desplazar campos ni insertar registros incompletos.
            }

            auto getField= [&](int idx, const std::string& name) -> std::string {
                if (idx< 0 || idx>= static_cast<int>(row.size())) {
                    report.emptyFields[name]++;
                    return "";
                }
                const std::string& val= row[idx];
                const auto normalized = DataCleaner::normalizeForSearch(val);
                if (normalized.empty()) {
                    report.emptyFields[name]++;
                } else if (normalized == "unknown") {
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
        std::cout <<"Bytes no ASCII (no implica corrupcion): "<< r.nonAsciiChars << std::endl;
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

