#pragma once

#include "RawMovie.h"
#include <filesystem>
#include <vector>

class CSVWriter {
public:
    // Escribe las mismas ocho columnas. Lanza una excepcion si no logra guardar.
    static void writeFile(const std::filesystem::path& path,
                          const std::vector<RawMovie>& movies);
};
