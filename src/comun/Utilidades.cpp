#include "comun/Utilidades.h"

#include <iomanip>
#include <sstream>

Cronometro::Cronometro() {
    reiniciar();
}

void Cronometro::reiniciar() {
    inicio = std::chrono::steady_clock::now();
}

double Cronometro::segundos() const {
    std::chrono::duration<double> tiempo = std::chrono::steady_clock::now() - inicio;
    return tiempo.count();
}

std::string Cronometro::texto() const {
    std::ostringstream salida;
    salida << std::fixed << std::setprecision(2) << segundos() << " s";
    return salida.str();
}

std::ostream& operator<<(std::ostream& os, const Cronometro& reloj) {
    os << reloj.texto();
    return os;
}

// En UTF-8 los bytes que continuan una letra tienen la forma 10xxxxxx
size_t contarCaracteres(const std::string& texto) {
    size_t total = 0;
    for (unsigned char c : texto) {
        if ((c & 0xC0) != 0x80) {
            total++;
        }
    }
    return total;
}

std::string recortar(const std::string& texto, size_t maximo) {
    if (contarCaracteres(texto) <= maximo) {
        return texto;
    }
    if (maximo <= 3) {
        return std::string(maximo, '.');
    }

    // Se copian letras completas hasta dejar espacio para los "..."
    std::string resultado;
    size_t caracteres = 0;
    for (size_t i = 0; i < texto.size(); i++) {
        unsigned char c = texto[i];
        if ((c & 0xC0) != 0x80) {
            if (caracteres == maximo - 3) {
                break;
            }
            caracteres++;
        }
        resultado += texto[i];
    }
    while (!resultado.empty() && resultado.back() == ' ') {
        resultado.pop_back();
    }
    return resultado + "...";
}

std::string rellenar(const std::string& texto, size_t ancho) {
    size_t caracteres = contarCaracteres(texto);
    if (caracteres >= ancho) {
        return texto;
    }
    return texto + std::string(ancho - caracteres, ' ');
}

std::string centrar(const std::string& texto, size_t ancho) {
    size_t caracteres = contarCaracteres(texto);
    if (caracteres >= ancho) {
        return texto;
    }
    return std::string((ancho - caracteres) / 2, ' ') + texto;
}


std::vector<std::string> ajustarTexto(const std::string& texto, size_t ancho) {
    std::vector<std::string> lineas;
    std::istringstream flujo(texto);
    std::string palabra;
    std::string linea;
    size_t largoLinea = 0;

    while (flujo >> palabra) {
        size_t largoPalabra = contarCaracteres(palabra);
        if (linea.empty()) {
            linea = palabra;
            largoLinea = largoPalabra;
        } else if (largoLinea + 1 + largoPalabra <= ancho) {
            linea += " " + palabra;
            largoLinea += 1 + largoPalabra;
        } else {
            lineas.push_back(linea);
            linea = palabra;
            largoLinea = largoPalabra;
        }
    }
    if (!linea.empty()) {
        lineas.push_back(linea);
    }
    return lineas;
}

int aNumero(const std::string& texto) {
    if (texto.empty() || texto.size() > 4) {
        return -1;
    }
    for (char c : texto) {
        if (c < '0' || c > '9') {
            return -1;
        }
    }
    return std::stoi(texto);
}
