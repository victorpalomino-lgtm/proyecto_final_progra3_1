# Buscador de películas — Programación III

Aplicación en C++ para buscar películas por título, sinopsis, director, reparto o género. Lee los datos desde un CSV, los limpia y construye un índice Trie para buscar palabras, frases y subcadenas desde la terminal.

## Integrantes

- Lama Flor Juan Diego
- Lescano Garamendi Juan Carlos Sebastian
- Palomino Arcos Víctor Valentino
- Valdiviezo Ortiz José Rodrigo
- Jossue Caceres

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

`main.cpp` es el punto de entrada y coordina el flujo completo. `CMakeLists.txt` configura la compilación y las pruebas.

## Compilar y ejecutar

Requisitos: CMake 3.20 o superior (`ctest --test-dir` existe desde 3.20) y un compilador compatible con C++20 (probado con GCC 16.2 en Linux). No hay otras dependencias: solo la biblioteca estándar y los hilos del sistema (`Threads`). Desde la raíz del proyecto:

```sh
cmake -S . -B cmake-build-release -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release -j2
ctest --test-dir cmake-build-release --output-on-failure
./cmake-build-release/proyecto_progra3_1 --limite 1000
```

`--limite 1000` construye el árbol con las primeras 1000 películas. Para usar todo el catálogo, quitar esa opción.

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

Los resultados de las búsquedas son idénticos con cualquier número de hilos. Mediciones con el catálogo completo en un Intel Core 5 210H (12 hilos, con batería y perfil `balanced`), comparadas con la versión sin paralelismo:

| Operación | Sin paralelismo | 12 hilos |
|---|---|---|
| Construir el índice | 44,1 s | 4,0 s (11,0×) |
| Búsqueda frecuente (`the`) | 2 426 ms | 244 ms (9,9×) |
| Sesión de prueba completa de la app | 59,6 s | 15,7 s (3,8×) |

Detalles, metodología y limitaciones en [docs/programacion_paralela.md](docs/programacion_paralela.md). Para repetir las mediciones: `bench/comparar_versiones.sh` y `python3 bench/resumir.py`.

El catálogo completo consumió aproximadamente 1,8 GiB de RAM en las pruebas locales (1,9 GiB con la indexación paralela). Los Like y Ver más tarde se guardan en `data/listas_usuario.txt`.
