#include "04_busqueda/Busqueda.h"
#include "comun/Utilidades.h"

#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <vector>

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

RawMovie crearPelicula(const std::string& titulo, const std::string& anio, const std::string& director, const std::string& reparto, const std::string& genero, const std::string& sinopsis) {
    RawMovie pelicula;
    pelicula.title = titulo;
    pelicula.releaseYear = anio;
    pelicula.director = director;
    pelicula.cast = reparto;
    pelicula.genre = genero;
    pelicula.plot = sinopsis;
    return pelicula;
}

int main() {
    try {
        require(contarCaracteres("Amelie") == 6, "Contar texto ASCII");
        require(contarCaracteres("Am\xC3\xA9lie") == 6, "Una letra con tilde cuenta como un caracter");
        require(recortar("Corto", 10) == "Corto", "No recortar textos cortos");
        require(recortar("Pelicula muy larga", 12) == "Pelicula...", "Recortar y quitar el espacio final");
        require(recortar("\xC3\x81rbol \xC3\xA1rbol", 5) == "\xC3\x81r...", "No cortar una tilde a la mitad");
        require(contarCaracteres(rellenar("Ni\xC3\xB1o", 6)) == 6, "Rellenar contando caracteres");
        require(centrar("ab", 6) == "  ab", "Centrar texto");

        std::vector<std::string> lineas = ajustarTexto("uno dos tres cuatro", 8);
        require(lineas.size() == 3 && lineas[0] == "uno dos" && lineas[1] == "tres" && lineas[2] == "cuatro",
                "Ajustar texto sin cortar palabras");
        require(ajustarTexto("", 10).empty(), "Texto vacio no genera lineas");

        require(aNumero("3") == 3 && aNumero("x") == -1 && aNumero("") == -1 && aNumero("2a") == -1,
                "Convertir opciones del menu");

        Cronometro reloj;
        require(reloj.segundos() >= 0 && reloj.texto().find(" s") != std::string::npos, "Cronometro");

        // ---------- Busquedas sobre un arbol pequeno ----------
        std::vector<RawMovie> peliculas;
        peliculas.push_back(crearPelicula("The Ship", "1990", "Ana Lopez", "Mario Diaz", "drama",
                                          "A story about the sea."));
        peliculas.push_back(crearPelicula("Blue Sky", "1950", "John Ford", "John Wayne", "western",
                                          "A ghost ship appears at night."));
        peliculas.push_back(crearPelicula("Another Ship", "2001", "Steven Spielberg", "Tom Hanks", "adventure",
                                          "Pirates everywhere."));
        peliculas.push_back(crearPelicula("Harbor", "2010", "Ana Lopez", "Lucia Ford", "comedy",
                                          "They drive a Ford car."));
        peliculas.push_back(crearPelicula("Silent Night", "1930", "", "", "", "Nobody talks."));

        MovieTrie arbol;
        arbol.buildIndex(peliculas);

        std::vector<Resultado> barcos = BusquedaTexto("ship").ejecutar(arbol, peliculas);
        require(barcos.size() == 3, "Encontrar ship en titulo y sinopsis");
        require(barcos[0].pelicula->title == "Another Ship" && barcos[1].pelicula->title == "The Ship" &&
                barcos[2].pelicula->title == "Blue Sky", "Primero las que tienen el texto en el titulo");
        require(BusquedaTexto("SHIP").ejecutar(arbol, peliculas).size() == 3, "No distinguir mayusculas");
        require(BusquedaTexto("arb").ejecutar(arbol, peliculas).size() == 1, "Buscar parte de una palabra");
        require(BusquedaTexto("ghost ship").ejecutar(arbol, peliculas).size() == 1, "Buscar una frase contigua");
        require(BusquedaTexto("ghost pirates").ejecutar(arbol, peliculas).empty(),
                "No aceptar palabras separadas como frase");
        require(BusquedaTexto("zzz").ejecutar(arbol, peliculas).empty(), "Busqueda sin resultados");
        require(BusquedaTexto("ford").ejecutar(arbol, peliculas).size() == 2, "Texto en cualquier campo");

        // El tag solo mira el campo elegido
        std::vector<Resultado> directorFord = BusquedaTag(FieldType::DIRECTOR, "ford").ejecutar(arbol, peliculas);
        require(directorFord.size() == 1 && directorFord[0].pelicula->title == "Blue Sky",
                "Tag director ignora reparto y sinopsis");
        std::vector<Resultado> repartoFord = BusquedaTag(FieldType::CAST, "Ford").ejecutar(arbol, peliculas);
        require(repartoFord.size() == 1 && repartoFord[0].pelicula->title == "Harbor", "Tag reparto");
        std::vector<Resultado> deLopez = BusquedaTag(FieldType::DIRECTOR, "lopez").ejecutar(arbol, peliculas);
        require(deLopez.size() == 2 && deLopez[0].pelicula->title == "Harbor", "Tag ordenado por titulo");
        require(BusquedaTag(FieldType::GENRE, "comedy").ejecutar(arbol, peliculas).size() == 1, "Tag genero");

        require(BusquedaTexto("sh").ejecutar(arbol, peliculas).size() == 3, "Subcadena corta");
        require(BusquedaTexto("shipwreck").ejecutar(arbol, peliculas).empty(), "Descartar falso positivo del trigrama ship");
        require(BusquedaTexto("   ").ejecutar(arbol, peliculas).empty(), "Consulta vacia");
        require(BusquedaTag(FieldType::CAST, "tom nobody").ejecutar(arbol, peliculas).empty(), "Tag exige frase completa");
        MovieTrie rebuilt;
        rebuilt.buildIndex(peliculas);
        rebuilt.buildIndex({peliculas.back()});
        require(rebuilt.searchSubstring("ship").empty(), "Reconstruccion elimina IDs anteriores");

        // Polimorfismo: se usan las dos busquedas a traves de la clase base
        std::vector<std::unique_ptr<Busqueda>> busquedas;
        busquedas.push_back(std::make_unique<BusquedaTexto>("ship"));
        busquedas.push_back(std::make_unique<BusquedaTag>(FieldType::DIRECTOR, "lopez"));
        require(busquedas[0]->descripcion() == "\"ship\"", "Descripcion de busqueda por texto");
        require(busquedas[1]->descripcion() == "director \"lopez\"", "Descripcion de busqueda por tag");
        require(busquedas[1]->ejecutar(arbol, peliculas).size() == 2, "Ejecutar desde la clase base");

        // Sobrecarga de operadores
        std::ostringstream fila;
        fila << barcos[0];
        require(fila.str().find("Another Ship (2001)") != std::string::npos &&
                fila.str().find("Steven Spielberg") != std::string::npos, "Imprimir una fila");
        std::ostringstream vacia;
        vacia << BusquedaTexto("silent").ejecutar(arbol, peliculas)[0];
        require(vacia.str().find("| -") != std::string::npos, "Campos vacios se muestran con -");

        Resultado primero;
        primero.prioridad = 1;
        primero.tituloOrden = "zeta";
        Resultado segundo;
        segundo.prioridad = 0;
        segundo.tituloOrden = "alfa";
        require(primero < segundo && !(segundo < primero), "Comparar resultados por prioridad");

        std::cout << "Pruebas de interfaz y busqueda correctas." << std::endl;
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
}
