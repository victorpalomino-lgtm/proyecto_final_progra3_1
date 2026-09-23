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

Requisitos: CMake 4 o superior y un compilador compatible con C++20. Desde la raíz del proyecto:

```sh
cmake -S . -B cmake-build-release -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release -j2
ctest --test-dir cmake-build-release --output-on-failure
./cmake-build-release/proyecto_progra3_1 --limite 1000
```

`--limite 1000` construye el árbol con las primeras 1000 películas. Para usar todo el catálogo, quitar esa opción.

Para repetir el diagnóstico y la limpieza, agregar `--rebuild-clean`. Requiere el archivo `data/wiki_movie_plots_deduped.csv`, que no está incluido en Git. Sin esa opción, se utiliza `data/movies_clean.csv` si está disponible.

El catálogo completo consumió aproximadamente 1,8 GiB de RAM en las pruebas locales. Like y Ver más tarde están pendientes para la entrega final.
