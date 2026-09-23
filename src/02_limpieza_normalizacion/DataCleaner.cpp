#include "02_limpieza_normalizacion/DataCleaner.h"
#include <ostream>
#include <string_view>
#include <utility>

namespace {
constexpr std::array<const char*, 8> columnNames = {
    "Release Year", "Title", "Origin/Ethnicity", "Director",
    "Cast", "Genre", "Wiki Page", "Plot"
};
constexpr std::array<std::string RawMovie::*, 8> fields = {
    &RawMovie::releaseYear, &RawMovie::title, &RawMovie::origin,
    &RawMovie::director, &RawMovie::cast, &RawMovie::genre,
    &RawMovie::wikiPage, &RawMovie::plot
};
}

std::string DataCleaner::cleanText(const std::string& text, PreprocessingReport& report) {
    std::string result;
    result.reserve(text.size());
    std::size_t pendingSpaces = 0;

    for (std::size_t i = 0; i < text.size();) {
        const auto c = static_cast<unsigned char>(text[i]);
        // Reconocer secuencias UTF-8 completas, sin destruir acentos ni otros alfabetos.
        if (text.compare(i, 3, "\xEF\xBB\xBF") == 0 ||
            text.compare(i, 3, "\xE2\x80\x8B") == 0) {
            ++report.invisibleCharactersRemoved; // BOM o espacio de ancho cero.
            i += 3;
            continue;
        }
        if (text.compare(i, 2, "\xC2\xA0") == 0) {
            ++report.specialSpacesReplaced;
            ++pendingSpaces;
            i += 2;
            continue;
        }
        if (c == ' ' || c < 32 || c == 127) {
            if (c != ' ') ++report.controlCharactersReplaced;
            // Un separador evita unir palabras al quitar saltos o controles.
            ++pendingSpaces;
            ++i;
            continue;
        }
        if (pendingSpaces > 0) {
            const bool keepSeparator = !result.empty();
            if (keepSeparator) result += ' ';
            report.whitespaceRemoved += pendingSpaces - (keepSeparator ? 1 : 0);
            pendingSpaces = 0;
        }
        result += text[i++];
    }
    report.whitespaceRemoved += pendingSpaces; // Espacios finales.
    return result;
}

std::string DataCleaner::lowercaseText(std::string text) {
    for (char& c : text) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
    }
    // Pares UTF-8 Latin-1: incluye vocales acentuadas, Ñ y Ü.
    // No depende de la configuracion regional ni aplica tolower a bytes UTF-8.
    constexpr std::string_view upper = "ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖØÙÚÛÜÝÞ";
    constexpr std::string_view lower = "àáâãäåæçèéêëìíîïðñòóôõöøùúûüýþ";
    for (std::size_t i = 0; i + 1 < text.size(); ++i) {
        if (static_cast<unsigned char>(text[i]) != 0xC3) continue;
        for (std::size_t j = 0; j < upper.size(); j += 2) {
            if (text.compare(i, 2, upper.substr(j, 2)) == 0) {
                text.replace(i, 2, lower.substr(j, 2));
                break;
            }
        }
        ++i;
    }
    return text;
}

std::string DataCleaner::normalizeForSearch(const std::string& text) {
    PreprocessingReport ignored;
    return lowercaseText(cleanText(text, ignored));
}

bool DataCleaner::isMissingMarker(const std::string& text, std::size_t column) {
    // "Unknown" puede ser un titulo real; solo interpretar metadatos conocidos.
    if (column != 3 && column != 4 && column != 5) return false;
    return lowercaseText(text) == "unknown" || (column == 5 && text == "-");
}

PreprocessingReport DataCleaner::clean(std::vector<RawMovie>& movies) {
    PreprocessingReport report;
    report.processedRecords = movies.size();
    for (auto& movie : movies) {
        for (std::size_t column = 0; column < fields.size(); ++column) {
            auto& original = movie.*fields[column];
            auto cleaned = cleanText(original, report);
            if (cleaned.empty()) {
                ++report.emptyFields[column];
            } else if (isMissingMarker(cleaned, column)) {
                ++report.missingMarkers[column];
                cleaned.clear();
            }
            if (cleaned != original) {
                ++report.modifiedFields;
                original = std::move(cleaned);
            }
        }
    }
    return report;
}

void DataCleaner::printReport(const PreprocessingReport& report, std::ostream& out) {
    out << "\nINFORME DE PREPROCESAMIENTO - INTEGRANTE 2\n"
        << "Registros procesados (todos conservados): " << report.processedRecords << '\n'
        << "Campos modificados: " << report.modifiedFields << '\n'
        << "Espacios sobrantes eliminados: " << report.whitespaceRemoved << '\n'
        << "Espacios no separables reemplazados: " << report.specialSpacesReplaced << '\n'
        << "Caracteres de control reemplazados: " << report.controlCharactersReplaced << '\n'
        << "Caracteres invisibles eliminados: " << report.invisibleCharactersRemoved << '\n';
    std::size_t empty = 0, markers = 0;
    for (std::size_t column = 0; column < fields.size(); ++column) {
        empty += report.emptyFields[column];
        markers += report.missingMarkers[column];
        out << " - " << columnNames[column]
            << " | Vacios: " << report.emptyFields[column]
            << " | Marcadores faltantes: " << report.missingMarkers[column] << '\n';
    }
    out << "Total de campos vacios detectados: " << empty << '\n'
        << "Total de marcadores convertidos a vacio: " << markers << '\n'
        << "Total de campos faltantes finales: " << empty + markers << '\n';
}
