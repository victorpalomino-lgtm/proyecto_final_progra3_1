#include "02_limpieza_normalizacion/CSVWriter.h"

#include <fstream>
#include <stdexcept>

namespace {
void writeField(std::ostream& out, const std::string& value) {
    // Entrecomillar todos los campos tambien protege comas y saltos de linea.
    out.put('"');
    for (char c : value) {
        if (c == '"') out.put('"'); // Comilla literal -> comilla doble CSV.
        out.put(c);
    }
    out.put('"');
}
}

void CSVWriter::writeFile(const std::filesystem::path& path,
                          const std::vector<RawMovie>& movies) {
    auto temporary = path;
    temporary += ".tmp";
    try {
        std::ofstream out;
        out.exceptions(std::ios::failbit | std::ios::badbit);
        out.open(temporary, std::ios::binary | std::ios::trunc);
        out << "Release Year,Title,Origin/Ethnicity,Director,Cast,Genre,Wiki Page,Plot\r\n";
        for (const auto& movie : movies) {
            const std::string* fields[] = {
                &movie.releaseYear, &movie.title, &movie.origin, &movie.director,
                &movie.cast, &movie.genre, &movie.wikiPage, &movie.plot
            };
            for (std::size_t i = 0; i < 8; ++i) {
                if (i != 0) out.put(',');
                writeField(out, *fields[i]);
            }
            out << "\r\n";
        }
        out.close();
        // Publicar solo al terminar: un fallo de escritura no deja un CSV parcial.
        std::filesystem::rename(temporary, path);
    } catch (const std::exception& error) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw std::runtime_error("No se pudo guardar " + path.string() + ": " + error.what());
    }
}
