#pragma once

#include <cstddef>

// Patron Observer: progreso de la construccion del indice.
//
// Garantia de hilos: MovieTrie::buildIndex llama a onProgresoIndexacion SIEMPRE desde
// el hilo que invoco buildIndex, nunca desde los hilos trabajadores y nunca de forma
// concurrente. Los trabajadores solo incrementan un contador std::atomic; el hilo que
// llamo lo lee periodicamente y notifica. Por eso un observador no necesita locks.
//
// "procesadas" y "total" se miden en claves insertadas (una por caracter normalizado),
// asi el avance refleja el trabajo real aunque las peliculas tengan largos distintos.
// La ultima notificacion siempre es (total, total).
class IObservadorIndexacion {
public:
    virtual ~IObservadorIndexacion() = default;
    virtual void onProgresoIndexacion(size_t procesadas, size_t total) = 0;
};
