#pragma once

#include <chrono>
#include <iostream>
#include <string>
#include <vector>

// Mide cuanto demora cada etapa
class Cronometro {
private:
    std::chrono::steady_clock::time_point inicio;

public:
    Cronometro(); // empieza a contar apenas se crea
    void reiniciar();
    double segundos() const;
    std::string texto() const;
};

std::ostream& operator<<(std::ostream& os, const Cronometro& reloj);

// Funciones para dar formato al texto de la terminal
size_t contarCaracteres(const std::string& texto);
std::string recortar(const std::string& texto, size_t maximo);
std::string rellenar(const std::string& texto, size_t ancho);
std::string centrar(const std::string& texto, size_t ancho);
std::vector<std::string> ajustarTexto(const std::string& texto, size_t ancho);

// Convierte una opcion del menu a numero devuelve -1 si no es un numero valido
int aNumero(const std::string& texto);
