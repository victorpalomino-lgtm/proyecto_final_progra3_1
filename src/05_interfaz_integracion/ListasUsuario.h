#pragma once

#include <string>
#include <unordered_set>
#include <vector>
#include "comun/RawMovie.h"

//listas de like y ver mas tarde del usuario, se guardan en un archivo de texto
class ListasUsuario {
private:
    std::string ruta;
    std::vector<std::string> listaLikes; //orden en que se agregaron
    std::vector<std::string> listaVerMasTarde;
    std::unordered_set<std::string> conLike; //para saber rapido si ya esta
    std::unordered_set<std::string> enVerMasTarde;

    static bool alternar(std::vector<std::string>& lista, std::unordered_set<std::string>& conjunto,
                         const std::string& clave);
    void cargar();
    void guardar() const;

public:
    explicit ListasUsuario(const std::string& ruta);

    //clave de una pelicula: titulo, anio y pagina de wikipedia separados por tab
    static std::string clave(const RawMovie& pelicula);

    //devuelven true si la pelicula quedo agregada y false si se quito
    bool alternarLike(const std::string& clave);
    bool alternarVerMasTarde(const std::string& clave);

    bool tieneLike(const std::string& clave) const;
    bool estaEnVerMasTarde(const std::string& clave) const;

    const std::vector<std::string>& likes() const;
    const std::vector<std::string>& verMasTarde() const;
};
