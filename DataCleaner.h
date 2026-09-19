#pragma once

#include "RawMovie.h"
#include <array>
#include <cstddef>
#include <iosfwd>
#include <string>
#include <vector>

struct PreprocessingReport {
    std::size_t processedRecords = 0;
    std::size_t modifiedFields = 0;
    std::size_t whitespaceRemoved = 0;
    std::size_t specialSpacesReplaced = 0;
    std::size_t controlCharactersReplaced = 0;
    std::size_t invisibleCharactersRemoved = 0;
    std::array<std::size_t, 8> emptyFields{}; // Vacios tras limpiar espacios.
    std::array<std::size_t, 8> missingMarkers{}; // Marcadores convertidos a vacio.
};

class DataCleaner {
public:
    // Limpia el mismo vector, sin eliminar ni reordenar peliculas.
    static PreprocessingReport clean(std::vector<RawMovie>& movies);
    // Para campos textuales y consultas futuras; no aplicar a URL ni identificadores.
    static std::string normalizeForSearch(const std::string& text);
    static void printReport(const PreprocessingReport& report, std::ostream& out);

private:
    static std::string cleanText(const std::string& text, PreprocessingReport& report);
    static std::string lowercaseText(std::string text);
    static bool isMissingMarker(const std::string& text, std::size_t column);
};
