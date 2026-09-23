#include "01_lectura_diagnostico/CSVReaderDiagnostic.h"
#include "02_limpieza_normalizacion/CSVWriter.h"
#include <filesystem>
#include <chrono>

void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

int main() {
    const auto dir = std::filesystem::temp_directory_path() /
        ("movie-reader-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(dir);
    const auto path = dir / "test.csv";
    const std::string header = "Release Year,Title,Origin/Ethnicity,Director,Cast,Genre,Wiki Page,Plot\n";
    auto write = [&](const std::string& contents) {
        std::ofstream out(path, std::ios::binary);
        out << contents;
    };
    auto rejected = [&]() {
        DiagnosticReport r;
        try { CSVReaderDiagnostic::processFile(path.string(), r); }
        catch (const std::runtime_error&) { return true; }
        return false;
    };
    try {
        write("\xEF\xBB\xBF" + header +
              "2000,\"A, \"\"movie\"\"\",US, UNKNOWN ,  ,drama,url,\"Line 1\nLine 2\"\r\n"
              "2000,\"A, \"\"movie\"\"\",US,Jane,,drama,url,Plot\n");
        DiagnosticReport r;
        auto movies = CSVReaderDiagnostic::processFile(path.string(), r);
        require(movies.size() == 2 && movies[0].title == "A, \"movie\"" && movies[0].plot == "Line 1\nLine 2", "CSV quoting/multiline/BOM");
        require(r.duplicateWikiPages == 1 && r.duplicateTitleYear == 1, "Duplicates diagnosed");
        require(r.emptyFields["Cast"] == 2 && r.unknownFields["Director"] == 1, "Whitespace and unknown diagnosis");
        DataCleaner::clean(movies);
        CSVWriter::writeFile(path, movies);
        auto roundtrip = CSVReaderDiagnostic::processFile(path.string(), r);
        require(r.totalRows == 2 && roundtrip[0].plot == "Line 1 Line 2", "Report reset and clean roundtrip");
        write("Title,Plot\na,b\n"); require(rejected(), "Missing columns rejected");
        write(header + "2000,\"Unclosed,US,,,,,plot"); require(rejected(), "Unclosed quote rejected");
        write(header + "2000,A\"bad,US,,,,,plot\n"); require(rejected(), "Embedded unescaped quote rejected");
        write(header + "2000,\"Title\"bad,US,,,,,plot\n"); require(rejected(), "Text after closing quote rejected");
        write(header + "2000,too,few\n");
        movies = CSVReaderDiagnostic::processFile(path.string(), r);
        require(movies.empty() && r.malformedRows == 1, "Malformed rows excluded");
        std::filesystem::remove_all(dir);
        std::cout << "Pruebas del lector CSV correctas.\n";
    } catch (const std::exception& e) {
        std::filesystem::remove_all(dir);
        std::cerr << e.what() << '\n';
        return 1;
    }
}
