#include "CSVWriter.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

int main() {
    const auto folder = std::filesystem::temp_directory_path() /
        ("csv-writer-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        std::filesystem::create_directory(folder);
        const auto path = folder / "movies.csv";
        RawMovie movie{"0019", "Título, \"citado\"", "", "", "", "", "URL_Case", "Uno\r\nDos"};
        CSVWriter::writeFile(path, {movie});
        std::ifstream in(path, std::ios::binary);
        const std::string contents{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
        const std::string header = "Release Year,Title,Origin/Ethnicity,Director,Cast,Genre,Wiki Page,Plot\r\n";
        if (contents != header + "\"0019\",\"Título, \"\"citado\"\"\",\"\",\"\",\"\",\"\",\"URL_Case\",\"Uno\r\nDos\"\r\n") {
            throw std::runtime_error("Escape CSV incorrecto");
        }
        in.close();
        CSVWriter::writeFile(path, {});
        if (std::filesystem::file_size(path) != header.size() ||
            std::filesystem::exists(path.string() + ".tmp")) {
            throw std::runtime_error("Reemplazo de CSV incorrecto");
        }
        bool failed = false;
        try { CSVWriter::writeFile(folder / "inexistente" / "movies.csv", {movie}); }
        catch (const std::exception&) { failed = true; }
        if (!failed) throw std::runtime_error("Debe informar errores de escritura");
        std::filesystem::remove_all(folder);
        std::cout << "Pruebas de exportacion correctas.\n";
    } catch (const std::exception& error) {
        std::filesystem::remove_all(folder);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
