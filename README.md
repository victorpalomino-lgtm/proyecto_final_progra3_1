# Buscador de películas — Programación III

Aplicación en C++ para buscar películas por título, sinopsis, director, reparto o género. Lee los datos desde un CSV, los limpia y construye un índice Trie para buscar palabras, frases ("y/o") y partes de palabras. Permite dar Like, guardar en Ver más tarde y ver recomendaciones de películas similares.

**La interfaz es de terminal (texto), no gráfica:** menús numerados que se manejan con el teclado. Ver [docs/interfaz.md](docs/interfaz.md) y una sesión de ejemplo en [docs/DEMO_TERMINAL.txt](docs/DEMO_TERMINAL.txt).

## Integrantes

- Lama Flor Juan Diego
- Lescano Garamendi Juan Carlos Sebastian
- Palomino Arcos Víctor Valentino
- Valdiviezo Ortiz José Rodrigo

## Organización

| Carpeta | Contenido |
|---|---|
| [src/01_lectura_diagnostico](src/01_lectura_diagnostico/) | Lectura y diagnóstico del CSV |
| [src/02_limpieza_normalizacion](src/02_limpieza_normalizacion/) | Limpieza, normalización y exportación de datos |
| [src/03_arbol_insercion](src/03_arbol_insercion/) | Trie e inserción |
| [src/04_busqueda](src/04_busqueda/) | Búsqueda y ordenamiento de resultados |
| [src/05_interfaz_integracion](src/05_interfaz_integracion/) | Menús y presentación en terminal |
| [src/comun](src/comun/) | Modelo de película y utilidades compartidas |
| [tests](tests/) | Pruebas |
| [data](data/) | Archivos CSV |
| [docs](docs/) | Documentación (ver abajo) |
| [bench](bench/) | Mediciones de rendimiento |

`main.cpp` es el punto de entrada y coordina el flujo completo. `CMakeLists.txt` configura la compilación y las pruebas.

## Compilar y ejecutar

Requisitos: CMake 3.20 o superior (`ctest --test-dir` existe desde 3.20) y un compilador compatible con C++20 (probado con GCC 16.2 en Linux). No hay otras dependencias: solo la biblioteca estándar y los hilos del sistema (`Threads`). Desde la raíz del proyecto:

```sh
cmake -S . -B cmake-build-release -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release -j2
ctest --test-dir cmake-build-release --output-on-failure
./cmake-build-release/proyecto_progra3_1 --limite 1000
```

### Opciones

| Opción | Qué hace |
|---|---|
| `--limite N` | Usa solo las primeras N películas (arranque rápido, menos memoria). Sin esta opción se carga el catálogo completo (34 886 películas). |
| `--hilos N` | Hilos para construir el índice y buscar. Por defecto, `std::thread::hardware_concurrency()`; `--hilos 1` usa los algoritmos secuenciales originales. |
| `--rebuild-clean` | Repite el diagnóstico y la limpieza desde el CSV original (ver más abajo). |

Una opción desconocida muestra el uso (`[--rebuild-clean] [--limite N] [--hilos N]`) y un número inválido muestra un aviso; en ambos casos el programa termina con error.

### Memoria y tiempo de arranque

Medido el 18-11-2026 en un AMD Ryzen 3 3200G (4 hilos, 16 GB), con el código actual:

| Catálogo | Pico de RAM | Construir el índice |
|---|---|---|
| Completo (34 886), 4 hilos | ~1,9 GB (1 902 MB) | 7,4 s |
| Completo, `--hilos 1` | ~1,8 GB (1 822 MB) | 35,8 s |
| `--limite 1000` | ~140 MB | 0,2 s |

Leer el CSV toma unos 2 s más. Con poca RAM conviene usar `--limite N`.

### Datos

Sin opciones extra, la aplicación usa `data/movies_clean.csv` (el CSV ya limpio), que **sí está en Git**: un clon recién hecho arranca sin descargar nada.

Para repetir el diagnóstico y la limpieza desde el CSV original, agregar `--rebuild-clean`. Ese CSV **no está en Git** (pesa ~78 MB):

1. Descargar `wiki_movie_plots_deduped.csv` del dataset [Wikipedia Movie Plots](https://www.kaggle.com/datasets/jrobischon/wikipedia-movie-plots) de Kaggle (pide iniciar sesión).
2. Colocarlo en `data/wiki_movie_plots_deduped.csv` (la carpeta `data/` de la raíz del proyecto; `.gitignore` ya lo excluye).
3. Ejecutar `./cmake-build-release/proyecto_progra3_1 --rebuild-clean`. Se muestra el informe de diagnóstico y de limpieza, y se vuelve a escribir `data/movies_clean.csv`.

Si el archivo no está, el programa indica de dónde descargarlo y dónde colocarlo.

## Paralelismo y rendimiento

La construcción del índice y la búsqueda usan varios hilos (`std::thread`). Por defecto se usan `std::thread::hardware_concurrency()` hilos; para elegir otro número:

```sh
./cmake-build-release/proyecto_progra3_1 --hilos 4      # 4 hilos
./cmake-build-release/proyecto_progra3_1 --hilos 1      # algoritmos secuenciales originales
```

Los resultados de las búsquedas son idénticos con cualquier número de hilos. Mediciones con el catálogo completo en un **AMD Ryzen 3 3200G con 4 hilos**, comparadas con la versión sin paralelismo (commit `55a2ddc`), intercaladas y con la mediana de cada una:

| Operación | Sin paralelismo | 4 hilos | Repeticiones |
|---|---|---|---|
| Construir el índice | 34,9 s | 7,4 s (4,71×) | 5 |
| Búsqueda frecuente (`the`) | 2 045 ms | 345 ms (5,93×) | **6** |
| Sesión de prueba completa de la app | 47,5 s | 17,4 s (2,74×) | 3 rondas |

La búsqueda se midió con **6 repeticiones** (no 9). En otro equipo (Intel Core 5 210H, 12 hilos, con batería) la indexación pasó de 44,1 s a 4,0 s (11,0×). Detalles, condiciones, metodología y limitaciones en [docs/programacion_paralela.md](docs/programacion_paralela.md). Para repetir las mediciones: `bench/comparar_versiones.sh` y `python3 bench/resumir.py`.

## Like, Ver más tarde y recomendaciones

Se guardan en `data/listas_usuario.txt` en el momento en que se marcan, y se vuelven a cargar al abrir la aplicación. El archivo no está en Git: cada usuario tiene el suyo. Detalles en [docs/interfaz.md](docs/interfaz.md).

## Documentación

| Documento | Contenido |
|---|---|
| [docs/arbol.md](docs/arbol.md) | El Trie de subcadenas: estructura, inserción, búsqueda, complejidad y por qué K = 3 |
| [docs/preprocesamiento.md](docs/preprocesamiento.md) | Parser del CSV, diagnóstico, limpieza y normalización |
| [docs/algoritmos.md](docs/algoritmos.md) | Puntaje de importancia, orden de las frases "y/o" y recomendación de similares |
| [docs/interfaz.md](docs/interfaz.md) | Menús, flujo, paginación, Like, Ver más tarde, persistencia y filtro [X] |
| [docs/patrones_diseno.md](docs/patrones_diseno.md) | Singleton, Strategy, Decorator y Observer, y dónde se usan |
| [docs/programacion_paralela.md](docs/programacion_paralela.md) | Indexación y búsqueda paralelas, mediciones y Amdahl |
| [docs/DEMO_TERMINAL.txt](docs/DEMO_TERMINAL.txt) | Captura de una sesión real con `--limite 1000` |
