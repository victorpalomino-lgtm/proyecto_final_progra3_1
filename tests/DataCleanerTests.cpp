#include "02_limpieza_normalizacion/DataCleaner.h"

#include <iostream>
#include <stdexcept>
#include <vector>

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    try {
        RawMovie movie{" 0019 ", "  Unknown  ", " American ", " UNKNOWN ",
                       "\t\r\n", " - ", " https://example.org/Case_Sensitive_0019 ",
                       "  Él dijo: \"Hola,  mundo\".\r\nFin.  "};
        std::vector<RawMovie> movies{movie, movie};
        const auto report = DataCleaner::clean(movies);
        require(movies.size() == 2 && report.processedRecords == 2, "Conservar duplicados");
        const auto& clean = movies.front();
        require(clean.releaseYear == "0019", "Conservar ceros iniciales");
        require(clean.wikiPage == "https://example.org/Case_Sensitive_0019", "Conservar URL");
        require(clean.title == "Unknown", "Unknown es un titulo valido");
        require(clean.director.empty() && clean.cast.empty() && clean.genre.empty(), "Faltantes");
        require(clean.plot == "Él dijo: \"Hola, mundo\". Fin.", "Conservar puntuacion y acentos");
        require(report.emptyFields[4] == 2 && report.missingMarkers[3] == 2 &&
                report.missingMarkers[5] == 2 && report.modifiedFields == 16, "Metricas de campos");
        require(report.controlCharactersReplaced == 10, "Controles contados por caracter");
        const auto again = DataCleaner::clean(movies);
        require(again.modifiedFields == 0 && again.whitespaceRemoved == 0 &&
                again.controlCharactersReplaced == 0, "Limpieza idempotente");
        require(DataCleaner::normalizeForSearch("  ÁRBOL\tNIÑO Ü À Ç Ω 東京  ") ==
                "árbol niño ü à ç Ω 東京", "Minusculas sin corromper UTF-8");
        require(clean.title == "Unknown", "La clave no altera el texto mostrado");

        RawMovie unicode;
        unicode.title = "\xEF\xBB\xBF" "A\xC2\xA0\xC2\xA0" "B\xE2\x80\x8B";
        unicode.plot = std::string("a\0b\x7F", 4);
        std::vector<RawMovie> special{unicode};
        const auto counts = DataCleaner::clean(special);
        require(special.front().title == "A B" && special.front().plot == "a b", "Controles y UTF-8");
        require(counts.specialSpacesReplaced == 2 && counts.invisibleCharactersRemoved == 2 &&
                counts.controlCharactersReplaced == 2 && counts.whitespaceRemoved == 2,
                "Metricas de caracteres");
        std::vector<RawMovie> empty;
        require(DataCleaner::clean(empty).processedRecords == 0, "Vector vacio");
        std::cout << "Pruebas de preprocesamiento correctas.\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
