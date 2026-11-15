#include "05_interfaz_integracion/ListasUsuario.h"

#include <algorithm>
#include <fstream>

ListasUsuario::ListasUsuario(const std::string& ruta) : ruta(ruta) {
    cargar();
}

void ListasUsuario::agregarObservador(IObserverLista* observador) {
    if (observador!= nullptr &&
        std::find(observadores.begin(), observadores.end(), observador)== observadores.end()) {
        observadores.push_back(observador);
    }
}

void ListasUsuario::eliminarObservador(IObserverLista* observador) {
    observadores.erase(std::remove(observadores.begin(), observadores.end(), observador),
                       observadores.end());
}

void ListasUsuario::notificar(TipoListaUsuario tipo, const std::string& clavePelicula, bool agregada) {
    for (IObserverLista* obs: observadores) {
        if (obs!= nullptr) {
            obs->onCambioLista(tipo, clavePelicula, agregada);
        }
    }
}

std::string ListasUsuario::clave(const RawMovie& pelicula) {
    return pelicula.title+'\t'+ pelicula.releaseYear+'\t'+ pelicula.wikiPage;
}

bool ListasUsuario::alternar(std::vector<std::string>& lista, std::unordered_set<std::string>& conjunto,
                             const std::string& clave) {
    if (conjunto.count(clave)> 0) {
        conjunto.erase(clave);
        lista.erase(std::find(lista.begin(), lista.end(), clave));
        return false;
    }
    conjunto.insert(clave);
    lista.push_back(clave);
    return true;
}

bool ListasUsuario::alternarLike(const std::string& clave) {
    bool agregada= alternar(listaLikes, conLike, clave);
    guardar(); //se guarda al momento por si el programa se cierra
    notificar(TipoListaUsuario::Like, clave, agregada);
    return agregada;
}

bool ListasUsuario::alternarVerMasTarde(const std::string& clave) {
    bool agregada= alternar(listaVerMasTarde, enVerMasTarde, clave);
    guardar();
    notificar(TipoListaUsuario::VerMasTarde, clave, agregada);
    return agregada;
}

bool ListasUsuario::tieneLike(const std::string& clave) const {
    return conLike.count(clave) > 0;
}

bool ListasUsuario::estaEnVerMasTarde(const std::string& clave) const {
    return enVerMasTarde.count(clave) > 0;
}

const std::vector<std::string>& ListasUsuario::likes() const {
    return listaLikes;
}

const std::vector<std::string>& ListasUsuario::verMasTarde() const {
    return listaVerMasTarde;
}

//cada linea es L o V, un tab y la clave de la pelicula
void ListasUsuario::cargar() {
    std::ifstream archivo(ruta);
    std::string linea;
    while (std::getline(archivo, linea)) {
        if (!linea.empty() && linea.back() == '\r') {
            linea.pop_back();
        }
        if (linea.size() < 3 || linea[1] != '\t') {
            continue; //linea rota, se ignora
        }
        std::string clavePelicula = linea.substr(2);
        if (linea[0] == 'L' && conLike.count(clavePelicula) == 0) {
            alternar(listaLikes, conLike, clavePelicula);
        } else if (linea[0] == 'V' && enVerMasTarde.count(clavePelicula) == 0) {
            alternar(listaVerMasTarde, enVerMasTarde, clavePelicula);
        }
    }
}

void ListasUsuario::guardar() const {
    std::ofstream archivo(ruta);
    for (const std::string& clavePelicula : listaLikes) {
        archivo << "L\t" << clavePelicula << std::endl;
    }
    for (const std::string& clavePelicula : listaVerMasTarde) {
        archivo << "V\t" << clavePelicula << std::endl;
    }
}