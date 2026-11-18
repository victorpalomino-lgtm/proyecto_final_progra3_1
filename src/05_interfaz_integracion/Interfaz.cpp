#include "05_interfaz_integracion/Interfaz.h"
#include "05_interfaz_integracion/Recomendador.h"
#include "04_busqueda/DecoradorBusqueda.h"
#include "comun/Utilidades.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
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

Interfaz::Interfaz(const std::vector<RawMovie>& peliculas, const MovieTrie& arbol,
                   const std::string& rutaListas)
    : peliculas(peliculas), buscador(arbol, peliculas),
      listas(rutaListas.empty() ? Interfaz::rutaListas() : rutaListas) {
    for (size_t id = 0; id < peliculas.size(); id++) {
        idPorClave.emplace(ListasUsuario::clave(peliculas[id]), id);
    }
    listas.agregarObservador(&cacheRecomendadas);
}

Interfaz::~Interfaz() {
    listas.eliminarObservador(&cacheRecomendadas);
}

//el archivo va en la carpeta data, aunque se ejecute desde la carpeta del build
std::string Interfaz::rutaListas() {
    if (std::filesystem::is_directory("data")) {
        return "data/listas_usuario.txt";
    }
    if (std::filesystem::is_directory("../data")) {
        return "../data/listas_usuario.txt";
    }
    return "listas_usuario.txt";
}

//pasa las claves guardadas a resultados, solo las peliculas que estan cargadas
std::vector<Resultado> Interfaz::resultadosDe(const std::vector<std::string>& claves) const {
    std::vector<Resultado> resultados;
    for (auto it = claves.rbegin(); it != claves.rend(); ++it) { //la ultima agregada sale primero
        auto encontrada = idPorClave.find(*it);
        if (encontrada != idPorClave.end()) {
            Resultado resultado;
            resultado.id = encontrada->second;
            resultado.pelicula = &peliculas[encontrada->second];
            resultados.push_back(resultado);
        }
    }
    return resultados;
}

const std::vector<Resultado>& Interfaz::obtenerRecomendadas() {
    // La cache empieza invalida y ListasUsuario la invalida (Observer) al cambiar un Like.
    if (!cacheRecomendadas.estaActualizada()) {
        std::vector<size_t> idsConLike;
        for (const Resultado& resultado : resultadosDe(listas.likes())) {
            idsConLike.push_back(resultado.id);
        }
        recomendadas = Recomendador::recomendar(peliculas, idsConLike);
        cacheRecomendadas.marcarActualizada();
    }
    return recomendadas;
}

// Usa el indice unico de la aplicacion (Singleton). Debe estar construido antes.
Interfaz::Interfaz()
    : Interfaz(IndiceGlobal::instancia().peliculas(), IndiceGlobal::instancia().arbol()) {
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

// Barra de progreso de la indexacion (patron Observer).
// La notifica solo el hilo que construye el indice (ver ObservadorIndexacion.h) y solo
// guarda el avance en atomicos: no imprime ni bloquea, asi no frena la indexacion.
// El hilo principal lee esos valores y dibuja la barra.
namespace {
const size_t ANCHO_BARRA = 20; // cada # es un 5% de las claves insertadas

class BarraProgreso : public IObservadorIndexacion {
public:
    std::atomic<size_t> procesadas{0};
    std::atomic<size_t> total{0};

    void onProgresoIndexacion(size_t hechas, size_t totalClaves) override {
        total.store(totalClaves, std::memory_order_relaxed);
        procesadas.store(hechas, std::memory_order_relaxed);
    }

    size_t marcas() const {
        const size_t t = total.load(std::memory_order_relaxed);
        if (t == 0) return 0;
        return std::min(ANCHO_BARRA, procesadas.load(std::memory_order_relaxed) * ANCHO_BARRA / t);
    }
};
}

// Construye el indice unico (Singleton) en otro hilo. Mientras tanto, el hilo principal
// dibuja la barra con el avance real que informa el observador.
bool Interfaz::construirIndice(IndiceGlobal& indice, std::vector<RawMovie> peliculas) {
    std::string linea = " [3/3] Construccion del arbol ";
    while (linea.size() < ANCHO_ETAPA) {
        linea += '.';
    }
    std::cout << linea << " " << std::flush;

    std::atomic<bool> terminado(false);
    std::string error;
    Cronometro reloj;
    BarraProgreso barra;

    std::thread hilo([&]() {
        try {
            indice.construir(std::move(peliculas), &barra);
        } catch (const std::bad_alloc&) {
            error = "no hay memoria suficiente";
        } catch (const std::exception& e) {
            error = e.what();
        }
        terminado = true;
    });

    size_t dibujadas = 0;
    auto dibujar = [&](size_t hasta) {
        if (hasta <= dibujadas) return;
        std::cout << std::string(hasta - dibujadas, '#') << std::flush;
        dibujadas = hasta;
    };
    while (!terminado) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        dibujar(barra.marcas());
    }
    hilo.join(); // despues del join ya se puede leer "error" sin problemas

    if (!error.empty()) {
        std::cout << " error" << std::endl;
        std::cerr << "No se pudo construir el arbol: " << error << "." << std::endl;
        std::cerr << "Pruebe con menos peliculas usando el argumento --limite N" << std::endl;
        return false;
    }
    dibujar(ANCHO_BARRA);
    std::cout << " listo, " << indice.peliculas().size() << " peliculas (" << reloj << ")" << std::endl;
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
        mostrarResumen("VER MAS TARDE", resultadosDe(listas.verMasTarde()),
                       "Todavia no agrego peliculas. Use [V] en el detalle de una pelicula.");
        mostrarResumen("RECOMENDADAS PARA USTED", obtenerRecomendadas(),
                       "De Like a una pelicula para recibir recomendaciones.");
        mostrarLinea('-');
        std::cout << " 1. Buscar por palabra, frase o parte de una palabra" << std::endl;
        std::cout << " 2. Buscar por tag (director, reparto o genero)" << std::endl;
        std::cout << " 3. Ver mas tarde" << std::endl;
        std::cout << " 4. Mis Likes" << std::endl;
        std::cout << " 5. Todas las recomendadas" << std::endl;
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
        } else if (opcion == "3") {
            pantallaLista("VER MAS TARDE", resultadosDe(listas.verMasTarde()),
                          "Su lista de Ver mas tarde esta vacia.");
        } else if (opcion == "4") {
            pantallaLista("MIS LIKES", resultadosDe(listas.likes()),
                          "Todavia no dio Like a ninguna pelicula.");
        } else if (opcion == "5") {
            std::vector<Resultado> copia = obtenerRecomendadas(); //copia porque cambia si da like en el detalle
            pantallaLista("RECOMENDADAS PARA USTED", copia,
                          "De Like a una pelicula para recibir recomendaciones.");
        } else if (opcion == "0") {
            std::cout << " Gracias por usar la plataforma. Hasta pronto." << std::endl;
            return;
        } else {
            aviso = "Opcion no valida. Escriba un numero del 0 al 5.";
        }
    }
}

//muestra hasta 5 peliculas de una lista en la pantalla de inicio
void Interfaz::mostrarResumen(const std::string& titulo, const std::vector<Resultado>& resultados,
                              const std::string& vacio) {
    mostrarLinea('-');
    std::cout << " " << titulo;
    if (resultados.size() > POR_PAGINA) {
        std::cout << " (" << POR_PAGINA << " de " << resultados.size() << ")";
    }
    std::cout << std::endl;
    if (resultados.empty()) {
        std::cout << " " << vacio << std::endl;
        return;
    }
    for (size_t i = 0; i < resultados.size() && i < POR_PAGINA; i++) {
        std::cout << " - " << resultados[i] << std::endl;
    }
}

void Interfaz::pantallaLista(const std::string& titulo, const std::vector<Resultado>& resultados,
                             const std::string& vacio) {
    if (resultados.empty()) {
        std::cout << std::endl;
        std::cout << " " << vacio << std::endl;
        pausa(" Presione Enter para volver al menu...");
        return;
    }
    mostrarLista(titulo, resultados, "");
}

// Pantallas de busqueda

void Interfaz::pantallaBusquedaTexto() {
    limpiarPantalla();
    mostrarCabecera();
    std::cout << " BUSQUEDA POR TEXTO" << std::endl;
    mostrarLinea('-');
    std::cout << " Puede escribir una palabra (ship), una frase (ghost ship)" << std::endl;
    std::cout << " o solo una parte de una palabra (bar)." << std::endl;
    std::cout << " Una frase encuentra peliculas con cualquiera de sus palabras: primero" << std::endl;
    std::cout << " las que tienen la frase completa, luego las que tienen todas las palabras" << std::endl;
    std::cout << " y despues las que tienen solo algunas." << std::endl;
    std::cout << " Las sinopsis estan en ingles, por eso conviene buscar en ingles." << std::endl;
    std::cout << " Deje el texto vacio y presione Enter para volver al menu." << std::endl;
    mostrarLinea('-');

    std::string texto;
    if (!leerLinea(" Buscar: ", texto) || texto.empty()) {
        return;
    }
    mostrarResultados(texto, []() { return std::make_unique<BusquedaTexto>(); });
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

        mostrarResultados(texto, [campo]() { return std::make_unique<BusquedaTag>(campo); });
        return;
    }
}

//claves de las peliculas con Like o en Ver mas tarde
std::unordered_set<std::string> Interfaz::clavesGuardadas() const {
    std::unordered_set<std::string> claves(listas.likes().begin(), listas.likes().end());
    claves.insert(listas.verMasTarde().begin(), listas.verMasTarde().end());
    return claves;
}

void Interfaz::mostrarResultados(const std::string& consulta, const FabricaEstrategia& fabrica) {
    bool ocultarGuardadas = false; // por defecto se muestran todas
    while (true) {
        std::unique_ptr<EstrategiaBusqueda> estrategia = fabrica();
        if (ocultarGuardadas) {
            // Patron Decorator: la misma busqueda, envuelta en un filtro.
            estrategia = std::make_unique<FiltroExcluirClavesBusqueda>(std::move(estrategia), clavesGuardadas(),
                                                                       "guardadas");
        }
        buscador.setEstrategia(std::move(estrategia));

        Cronometro reloj;
        std::vector<Resultado> resultados = buscador.buscar(consulta);
        std::string tiempo = reloj.texto();
        const std::string descripcion = buscador.descripcion(consulta);

        if (resultados.empty()) {
            std::cout << std::endl;
            if (ocultarGuardadas) {
                // Todas estaban guardadas: se vuelve a mostrar la lista completa.
                std::cout << " Todas las peliculas de " << descripcion << " ya estan guardadas." << std::endl;
                pausa(" Presione Enter para ver la lista completa...");
                ocultarGuardadas = false;
                continue;
            }
            std::cout << " No se encontraron peliculas para " << descripcion << "." << std::endl;
            pausa(" Presione Enter para volver al menu...");
            return;
        }
        if (!mostrarLista("RESULTADOS PARA " + descripcion, resultados, tiempo, true, ocultarGuardadas)) {
            return;
        }
        ocultarGuardadas = !ocultarGuardadas;
    }
}

//paginacion de 5 en 5, la usan la busqueda y las listas del usuario
//esBusqueda: ofrece [X] para ocultar las guardadas, solo si el filtro esta activo o el
//usuario tiene alguna pelicula guardada (si no, la pantalla queda igual que antes).
//Devuelve true si se eligio [X].
bool Interfaz::mostrarLista(const std::string& titulo, const std::vector<Resultado>& resultados,
                            const std::string& tiempo, bool esBusqueda, bool filtroActivo) {
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
        std::cout << " " << titulo << std::endl;
        if (tiempo.empty()) {
            std::cout << " " << resultados.size() << (resultados.size() == 1 ? " pelicula" : " peliculas");
        } else if (resultados.size() == 1) {
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
        // Se calcula en cada vuelta: puede dar Like desde el detalle de estos mismos resultados.
        const bool ofrecerFiltro = esBusqueda &&
            (filtroActivo || !listas.likes().empty() || !listas.verMasTarde().empty());
        if (ofrecerFiltro) {
            std::cout << (filtroActivo ? " [X] Mostrar tambien las guardadas"
                                       : " [X] Ocultar las que tienen Like o estan en Ver mas tarde")
                      << std::endl;
        }
        if (!aviso.empty()) {
            std::cout << " " << aviso << std::endl;
            aviso = "";
        }

        std::string opcion;
        if (!leerLinea(" Elija una opcion: ", opcion)) {
            return false;
        }

        if (ofrecerFiltro && (opcion == "x" || opcion == "X")) {
            return true;
        } else if (opcion == "s" || opcion == "S") {
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
            return false;
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

    const std::string clave = ListasUsuario::clave(pelicula);
    std::string aviso;
    while (true) {
        mostrarLinea('-');
        std::cout << (listas.tieneLike(clave) ? " [L] Quitar Like" : " [L] Like");
        std::cout << (listas.estaEnVerMasTarde(clave) ? "   [V] Quitar de Ver mas tarde" : "   [V] Ver mas tarde");
        std::cout << std::endl;
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
        if (opcion == "l" || opcion == "L") {
            bool agregada = listas.alternarLike(clave); //notifica a cacheRecomendadas (Observer)
            aviso = agregada ? "Le dio Like a esta pelicula." : "Se quito el Like.";
        } else if (opcion == "v" || opcion == "V") {
            bool agregada = listas.alternarVerMasTarde(clave);
            aviso = agregada ? "Se agrego a Ver mas tarde." : "Se quito de Ver mas tarde.";
        } else {
            aviso = "Opcion no valida. Escriba L, V o R.";
        }
    }
}
