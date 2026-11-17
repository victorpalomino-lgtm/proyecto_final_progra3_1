#pragma once

// Utilidades minimas de paralelismo (solo biblioteca estandar).
//
// ejecutarEnHilos lanza "hilos" tareas con std::thread y espera a que terminen.
// Si alguna lanza una excepcion, se guarda (std::exception_ptr) y se relanza en el
// hilo que llamo, despues del join: nunca queda un hilo sin unir.

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <exception>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace paralelo {

// 0 significa "automatico": std::thread::hardware_concurrency() (o 1 si no se conoce).
inline size_t resolverHilos(size_t pedidos) {
    if (pedidos > 0) return pedidos;
    const unsigned hw = std::thread::hardware_concurrency();
    return hw == 0 ? 1 : static_cast<size_t>(hw);
}

// Ejecuta tarea(indiceHilo) para indiceHilo = 0 .. hilos-1, cada una en su propio hilo.
//
// Sin "mientrasEspera": el hilo que llama ejecuta la tarea 0 (se ahorra crear un hilo).
// Con "mientrasEspera": el hilo que llama no trabaja; lanza todas las tareas y llama a
// mientrasEspera() cada "intervalo" hasta que terminen. Asi un unico hilo (el que llamo)
// puede notificar progreso, sin llamadas concurrentes a los observadores.
inline void ejecutarEnHilos(size_t hilos, const std::function<void(size_t)>& tarea,
                            const std::function<void()>& mientrasEspera = nullptr,
                            std::chrono::milliseconds intervalo = std::chrono::milliseconds(50)) {
    if (hilos == 0) return;
    std::vector<std::exception_ptr> errores(hilos); // cada hilo escribe solo su casilla
    std::mutex mutexFin;
    std::condition_variable finCv;
    size_t terminados = 0; // protegido por mutexFin

    auto ejecutarProtegido = [&](size_t indice) {
        try {
            tarea(indice);
        } catch (...) {
            errores[indice] = std::current_exception();
        }
        {
            std::lock_guard<std::mutex> lock(mutexFin);
            ++terminados;
        }
        finCv.notify_one();
    };

    const size_t primero = mientrasEspera ? 0 : 1;
    std::vector<std::thread> trabajadores;
    trabajadores.reserve(hilos);
    try {
        for (size_t i = primero; i < hilos; ++i) trabajadores.emplace_back(ejecutarProtegido, i);
    } catch (...) {
        // No se pudo crear un hilo: esperar a los ya creados antes de propagar.
        for (auto& h : trabajadores) h.join();
        throw;
    }

    if (mientrasEspera) {
        std::unique_lock<std::mutex> lock(mutexFin);
        while (!finCv.wait_for(lock, intervalo, [&] { return terminados == hilos; })) {
            lock.unlock();
            mientrasEspera(); // solo este hilo llama al callback
            lock.lock();
        }
    } else {
        ejecutarProtegido(0);
    }

    for (auto& h : trabajadores) h.join();
    for (const auto& error : errores) {
        if (error) std::rethrow_exception(error);
    }
}

} // namespace paralelo
