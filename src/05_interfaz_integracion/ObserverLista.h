#ifndef PROYECTO_PROGRA3_1_OBSERVERLISTA_H
#define PROYECTO_PROGRA3_1_OBSERVERLISTA_H
#pragma once

#include <string>
#include <vector>

enum class TipoListaUsuario {
    Like,
    VerMasTarde
};

//Patron Observer: Interfaz para reaccionar ante cambios en ListasUsuario
//sin acoplar ListasUsuario con el Recomendador ni con la Interfaz.
class IObserverLista {
public:
    virtual ~IObserverLista()= default;
    virtual void onCambioLista(TipoListaUsuario tipo, const std::string& clavePelicula, bool agregada)= 0;
};

//Observador concreto 1: Mantiene sincronizada la cache de recomendaciones.
//Cada vez que cambia la lista de Likes, marca las recomendaciones como desactualizadas.
class CacheRecomendadorObserver: public IObserverLista {
private:
    bool cacheValida= false;
    size_t invalidaciones= 0;

public:
    void onCambioLista(TipoListaUsuario tipo, const std::string& /*clavePelicula*/, bool /*agregada*/) override {
        if (tipo== TipoListaUsuario::Like) {
            cacheValida= false;
            invalidaciones++;
        }
    }

    void marcarActualizada() { cacheValida= true;}
    bool estaActualizada() const { return cacheValida;}
    size_t totalInvalidaciones() const { return invalidaciones;}
};

// Observador concreto 2: Registra un historial de acciones de la sesion del usuario
class AuditorListasObserver: public IObserverLista {
private:
    std::vector<std::string> eventos;

public:
    void onCambioLista(TipoListaUsuario tipo, const std::string& clavePelicula,
                       bool agregada) override {
        std::string lista= (tipo== TipoListaUsuario::Like) ? "Like": "Ver mas tarde";
        std::string accion= agregada ? "Agregado a ": "Quitado de ";
        eventos.push_back(accion+ lista +": "+ clavePelicula);
    }

    const std::vector<std::string>& historialEventos() const { return eventos;}
    void limpiar() { eventos.clear();}
};

#endif //PROYECTO_PROGRA3_1_OBSERVERLISTA_H