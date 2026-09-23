#include "05_interfaz_integracion/Interfaz.h"
#include "comun/Utilidades.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <new>
#include <thread>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

// Medidas de la pantalla (en caracteres)
const size_t ANCHO = 80;
const size_t POR_PAGINA = 5;
const size_t LINEAS_POR_BLOQUE = 18;
const size_t ANCHO_ETAPA = 38;

// true si el programa corre en una terminal real
// En la consola normal de CLion es false, porque ahi la salida no es una terminal
static bool esTerminal() {
#ifdef _WIN32
    return _isatty(_fileno(stdout)) != 0;
#else
    return isatty(fileno(stdout)) != 0;
#endif
}

Interfaz::Interfaz(const std::vector<RawMovie>& peliculas, const MovieTrie& arbol)
    : peliculas(peliculas), arbol(arbol) {
}

// funciones generales y pantalla de carga
void Interfaz::prepararConsola() {
#ifdef _WIN32
    //para que la consola de Windows muestre bien las tildes de los titulos UTF-8
    system("chcp 65001 > nul");
#endif
}

void Interfaz::limpiarPantalla() {
    if (!esTerminal()) {
        // La consola de CLion no se puede borrar, solo se separan las pantallas
        std::cout << std::endl;
        return;
    }
#ifdef _WIN32
    system("cls");
#else
    std::cout << "\033[2J\033[1;1H" << std::flush;
#endif
}

void Interfaz::mostrarLinea(char simbolo) {
    std::cout << std::string(ANCHO, simbolo) << std::endl;
}

void Interfaz::mostrarCabecera() {
    mostrarLinea('=');
    std::cout << centrar("PLATAFORMA DE STREAMING", ANCHO) << std::endl;
    std::cout << centrar("Busqueda de peliculas - Programacion III", ANCHO) << std::endl;
    mostrarLinea('=');
}

void Interfaz::mostrarEtapa(const std::string& nombre, const std::string& detalle) {
    std::string linea = " " + nombre + " ";
    while (linea.size() < ANCHO_ETAPA) {
        linea += '.';
    }
    std::cout << linea << " " << detalle << std::endl;
}

// Construye el arbol en otro hilo. Mientras tanto, el hilo principal imprime un # cada
bool Interfaz::construirArbol(MovieTrie& arbol, const std::vector<RawMovie>& peliculas) {
    std::string linea = " [3/3] Construccion del arbol ";
    while (linea.size() < ANCHO_ETAPA) {
        linea += '.';
    }
    std::cout << linea << " " << std::flush;

    std::atomic<bool> terminado(false);
    std::string error;
    Cronometro reloj;

    std::thread hilo([&]() {
        try {
            arbol.buildIndex(peliculas);
        } catch (const std::bad_alloc&) {
            error = "no hay memoria suficiente";
        } catch (const std::exception& e) {
            error = e.what();
        }
        terminado = true;
    });

    int vueltas = 0;
    while (!terminado) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        vueltas++;
        if (vueltas % 10 == 0) {
            std::cout << "#" << std::flush;
        }
    }
    hilo.join(); // despues del join ya se puede leer "error" sin problemas

    if (!error.empty()) {
        std::cout << " error" << std::endl;
        std::cerr << "No se pudo construir el arbol: " << error << "." << std::endl;
        std::cerr << "Pruebe con menos peliculas usando el argumento --limite N" << std::endl;
        return false;
    }
    std::cout << " listo, " << peliculas.size() << " peliculas (" << reloj << ")" << std::endl;
    return true;
}

// Lee una linea completa y le quita los espacios de los extremos
bool Interfaz::leerLinea(const std::string& mensaje, std::string& linea) {
    std::cout << mensaje << std::flush;
    if (!std::getline(std::cin, linea)) {
        std::cout << std::endl;
        return false;
    }
    size_t inicio = linea.find_first_not_of(" \t\r");
    if (inicio == std::string::npos) {
        linea = "";
    } else {
        size_t fin = linea.find_last_not_of(" \t\r");
        linea = linea.substr(inicio, fin - inicio + 1);
    }
    return true;
}

void Interfaz::pausa(const std::string& mensaje) {
    std::string ignorar;
    leerLinea(mensaje, ignorar);
}

// Menu principal
void Interfaz::ejecutar() {
    std::string aviso;

    while (true) {
        limpiarPantalla();
        mostrarCabecera();
        std::cout << " Peliculas disponibles: " << peliculas.size() << std::endl;
        mostrarLinea('-');
        std::cout << " 1. Buscar por palabra, frase o parte de una palabra" << std::endl;
        std::cout << " 2. Buscar por tag (director, reparto o genero)" << std::endl;
        std::cout << " 0. Salir" << std::endl;
        mostrarLinea('-');
        if (!aviso.empty()) {
            std::cout << " " << aviso << std::endl;
            aviso = "";
        }

        std::string opcion;
        if (!leerLinea(" Elija una opcion: ", opcion)) {
            return;
        }

        if (opcion == "1") {
            pantallaBusquedaTexto();
        } else if (opcion == "2") {
            pantallaBusquedaTag();
        } else if (opcion == "0") {
            std::cout << " Gracias por usar la plataforma. Hasta pronto." << std::endl;
            return;
        } else {
            aviso = "Opcion no valida. Escriba 1, 2 o 0.";
        }
    }
}

// Pantallas de busqueda

void Interfaz::pantallaBusquedaTexto() {
    limpiarPantalla();
    mostrarCabecera();
    std::cout << " BUSQUEDA POR TEXTO" << std::endl;
    mostrarLinea('-');
    std::cout << " Puede escribir una palabra (ship), una frase (ghost ship)" << std::endl;
    std::cout << " o solo una parte de una palabra (bar)." << std::endl;
    std::cout << " Las frases se buscan completas y en el mismo orden." << std::endl;
    std::cout << " Las sinopsis estan en ingles, por eso conviene buscar en ingles." << std::endl;
    std::cout << " Deje el texto vacio y presione Enter para volver al menu." << std::endl;
    mostrarLinea('-');

    std::string texto;
    if (!leerLinea(" Buscar: ", texto) || texto.empty()) {
        return;
    }
    BusquedaTexto busqueda(texto);
    mostrarResultados(busqueda);
}

void Interfaz::pantallaBusquedaTag() {
    std::string aviso;

    while (true) {
        limpiarPantalla();
        mostrarCabecera();
        std::cout << " BUSQUEDA POR TAG" << std::endl;
        mostrarLinea('-');
        std::cout << " 1. Director" << std::endl;
        std::cout << " 2. Reparto (actor o actriz)" << std::endl;
        std::cout << " 3. Genero" << std::endl;
        std::cout << " 0. Volver al menu" << std::endl;
        mostrarLinea('-');
        if (!aviso.empty()) {
            std::cout << " " << aviso << std::endl;
            aviso = "";
        }

        std::string opcion;
        if (!leerLinea(" Elija una opcion: ", opcion) || opcion == "0") {
            return;
        }

        FieldType campo = FieldType::DIRECTOR;
        std::string pregunta;
        if (opcion == "1") {
            campo = FieldType::DIRECTOR;
            pregunta = " Nombre del director (ejemplo: spielberg): ";
        } else if (opcion == "2") {
            campo = FieldType::CAST;
            pregunta = " Nombre del actor o actriz (ejemplo: tom hanks): ";
        } else if (opcion == "3") {
            campo = FieldType::GENRE;
            pregunta = " Genero (ejemplo: comedy, horror, western): ";
        } else {
            aviso = "Opcion no valida. Escriba 1, 2, 3 o 0.";
            continue;
        }

        std::string texto;
        if (!leerLinea(pregunta, texto)) {
            return;
        }
        if (texto.empty()) {
            aviso = "No escribio nada para buscar.";
            continue;
        }

        BusquedaTag busqueda(campo, texto);
        mostrarResultados(busqueda);
        return;
    }
}

void Interfaz::mostrarResultados(const Busqueda& busqueda) {
    Cronometro reloj;
    std::vector<Resultado> resultados = busqueda.ejecutar(arbol, peliculas);
    std::string tiempo = reloj.texto();

    if (resultados.empty()) {
        std::cout << std::endl;
        std::cout << " No se encontraron peliculas para " << busqueda.descripcion() << "." << std::endl;
        pausa(" Presione Enter para volver al menu...");
        return;
    }

    size_t totalPaginas = (resultados.size() + POR_PAGINA - 1) / POR_PAGINA;
    size_t pagina = 0;
    std::string aviso;

    while (true) {
        // Cambiar de pagina es O(1) porque solo se calcula desde donde se muestra el vector
        size_t inicio = pagina * POR_PAGINA;
        size_t fin = inicio + POR_PAGINA;
        if (fin > resultados.size()) {
            fin = resultados.size();
        }
        size_t enPagina = fin - inicio;

        limpiarPantalla();
        mostrarCabecera();
        std::cout << " RESULTADOS PARA " << busqueda.descripcion() << std::endl;
        if (resultados.size() == 1) {
            std::cout << " 1 pelicula encontrada en " << tiempo;
        } else {
            std::cout << " " << resultados.size() << " peliculas encontradas en " << tiempo;
        }
        std::cout << "   |   Pagina " << pagina + 1 << " de " << totalPaginas << std::endl;
        mostrarLinea('-');
        for (size_t i = inicio; i < fin; i++) {
            std::cout << " " << (i - inicio + 1) << ". " << resultados[i] << std::endl;
        }
        mostrarLinea('-');

        std::string opciones = " [1";
        if (enPagina > 1) {
            opciones += "-" + std::to_string(enPagina);
        }
        opciones += "] Ver pelicula";
        if (pagina + 1 < totalPaginas) {
            opciones += "   [S] Siguientes 5";
        }
        if (pagina > 0) {
            opciones += "   [A] Anteriores";
        }
        opciones += "   [M] Menu";
        std::cout << opciones << std::endl;
        if (!aviso.empty()) {
            std::cout << " " << aviso << std::endl;
            aviso = "";
        }

        std::string opcion;
        if (!leerLinea(" Elija una opcion: ", opcion)) {
            return;
        }

        if (opcion == "s" || opcion == "S") {
            if (pagina + 1 < totalPaginas) {
                pagina++;
            } else {
                aviso = "Ya esta en la ultima pagina.";
            }
        } else if (opcion == "a" || opcion == "A") {
            if (pagina > 0) {
                pagina--;
            } else {
                aviso = "Ya esta en la primera pagina.";
            }
        } else if (opcion == "m" || opcion == "M") {
            return;
        } else {
            int numero = aNumero(opcion);
            if (numero >= 1 && numero <= (int)enPagina) {
                mostrarDetalle(*resultados[inicio + numero - 1].pelicula);
            } else {
                aviso = "Opcion no valida.";
            }
        }
    }
}

void Interfaz::mostrarCampo(const std::string& etiqueta, const std::string& valor) {
    std::string texto = valor.empty() ? "No registrado" : valor;
    std::string inicio = " " + rellenar(etiqueta, 10) + ": ";
    std::vector<std::string> lineas = ajustarTexto(texto, ANCHO - inicio.size());

    for (size_t i = 0; i < lineas.size(); i++) {
        if (i == 0) {
            std::cout << inicio << lineas[i] << std::endl;
        } else {
            std::cout << std::string(inicio.size(), ' ') << lineas[i] << std::endl;
        }
    }
}

void Interfaz::mostrarDetalle(const RawMovie& pelicula) {
    limpiarPantalla();
    mostrarLinea('=');
    std::string titulo = pelicula.title;
    if (!pelicula.releaseYear.empty()) {
        titulo += " (" + pelicula.releaseYear + ")";
    }
    for (const std::string& linea : ajustarTexto(titulo, ANCHO - 2)) {
        std::cout << " " << linea << std::endl;
    }
    mostrarLinea('=');

    mostrarCampo("Estreno", pelicula.releaseYear);
    mostrarCampo("Origen", pelicula.origin);
    mostrarCampo("Director", pelicula.director);
    mostrarCampo("Reparto", pelicula.cast);
    mostrarCampo("Genero", pelicula.genre);
    mostrarCampo("Wikipedia", pelicula.wikiPage);
    mostrarLinea('-');
    std::cout << " SINOPSIS" << std::endl;
    std::cout << std::endl;

    // Algunas sinopsis son muy largas, asi que se muestran por partes
    std::vector<std::string> lineas = ajustarTexto(pelicula.plot, ANCHO - 2);
    if (lineas.empty()) {
        std::cout << " (Esta pelicula no tiene sinopsis registrada)" << std::endl;
    }
    for (size_t i = 0; i < lineas.size(); i++) {
        std::cout << " " << lineas[i] << std::endl;

        bool finDeBloque = (i + 1) % LINEAS_POR_BLOQUE == 0;
        bool quedanLineas = i + 1 < lineas.size();
        if (finDeBloque && quedanLineas) {
            std::string opcion;
            if (!leerLinea(" [Enter] Seguir leyendo   [Q] Dejar de leer: ", opcion)) {
                return;
            }
            if (opcion == "q" || opcion == "Q") {
                std::cout << " (Quedaron " << lineas.size() - i - 1 << " lineas sin mostrar)" << std::endl;
                break;
            }
        }
    }

    std::string aviso;
    while (true) {
        mostrarLinea('-');
        std::cout << " [L] Like   [V] Ver mas tarde   (se activan en la entrega final)" << std::endl;
        std::cout << " [R] Volver a los resultados" << std::endl;
        if (!aviso.empty()) {
            std::cout << " " << aviso << std::endl;
            aviso = "";
        }

        std::string opcion;
        if (!leerLinea(" Elija una opcion: ", opcion)) {
            return;
        }
        if (opcion == "r" || opcion == "R") {
            return;
        }
        if (opcion == "l" || opcion == "L" || opcion == "v" || opcion == "V") {
            aviso = "Like y Ver mas tarde estaran disponibles en la entrega final.";
        } else {
            aviso = "Opcion no valida. Escriba R para volver.";
        }
    }
}
