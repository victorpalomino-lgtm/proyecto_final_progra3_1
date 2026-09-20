# Interfaz e integración (Integrante 5)

## De qué se encarga esta parte

Es la que junta todo lo que hicieron los demás en un solo programa que se puede usar desde la terminal. El flujo es: se lee el CSV, se limpia, se arma el árbol, y con eso ya se puede buscar y ver los resultados.

Esta parte no toca los datos ni el árbol, solo usa las funciones que ya estaban hechas (`processFile`, `clean`, `writeFile`, `buildIndex`, `searchSubstring`).

## Cómo correrlo

Desde la raíz del proyecto:

```sh
cmake -S . -B cmake-build-debug
cmake --build cmake-build-debug
ctest --test-dir cmake-build-debug --output-on-failure
./cmake-build-debug/proyecto_progra3_1
```

Se le puede pasar `--limite N` para que solo cargue las primeras N películas (útil porque el árbol todavía no aguanta el dataset completo, se cae por falta de memoria). También existe `--rebuild-clean` para volver a generar el CSV limpio desde el original.

## Cómo funciona el flujo

1. Lee el CSV (ya sea el original o el que ya está limpio).
2. Si hace falta, limpia los datos y guarda el CSV limpio.
3. Construye el árbol con las películas cargadas.
4. Abre el menú para buscar.

Cada etapa muestra cuánto tardó, con una clase `Cronometro` que hice para no repetir el código de medir tiempo en cada parte.

## El menú

Al entrar hay dos formas de buscar:

- Por texto: se puede escribir una palabra, una frase, o incluso una parte de una palabra, y busca en título, sinopsis, director, reparto y género.
- Por tag: se elige un campo específico (director, reparto o género) y busca el texto solo ahí.

Los resultados salen de 5 en 5, con opciones para ver la siguiente página, la anterior, volver al menú o abrir el detalle de una película. En el detalle sale toda la info y la sinopsis se muestra por partes cuando es muy larga, para que no se llene toda la pantalla de una sola vez.

## Archivos que agregué

- `Utilidades.h/.cpp`: el cronómetro y funciones para cortar y acomodar texto sin romper las tildes (en UTF-8 una letra con tilde ocupa 2 bytes, y si se corta mal sale un símbolo raro).
- `Busqueda.h/.cpp`: acá está la clase `Busqueda`, que es abstracta, y de ella salen dos hijas: `BusquedaTexto` y `BusquedaTag`. Así la pantalla de resultados no necesita saber qué tipo de búsqueda se hizo, solo le pasan la clase base y funciona igual para las dos (eso es polimorfismo).
- `Interfaz.h/.cpp`: todas las pantallas, el menú, y la parte donde se arma el árbol en otro hilo mientras se muestra una barra de avance para que no parezca que el programa se colgó.
- `tests/InterfazTests.cpp`: pruebas de esta parte, con el mismo formato `require(...)` que ya usaban en el resto del proyecto.

En `main.cpp` solo cambié la función `main()`, el resto (la clase del Integrante 1 que lee el CSV) queda igual. En `CMakeLists.txt` agregué los 3 archivos nuevos y la librería de hilos (`Threads`), que hace falta porque el árbol se construye en un hilo aparte.

## Cosas del curso que usé

- **Polimorfismo**: `Busqueda` como clase base con `BusquedaTexto` y `BusquedaTag` como hijas.
- **Sobrecarga de operadores**: `operator<` para ordenar los resultados y `operator<<` para imprimirlos en la tabla.
- **Contenedores**: se usan `vector`, `unordered_set` (el que devuelve el árbol) y `vector<unique_ptr<Busqueda>>` en las pruebas.
- **Concurrencia**: el árbol se arma en un `std::thread` aparte, y usé un `std::atomic<bool>` para que el hilo principal sepa cuándo terminó sin pisarse con el otro hilo.

## Qué probé

Corrí las pruebas de `interfaz_tests` y pasan todas. Además probé el programa a mano: buscar palabras, frases, partes de palabras, moverme entre páginas, abrir el detalle de películas con sinopsis larga, buscar por cada tag, y dejar cosas vacías o mal escritas para ver que no se rompa.

## Si el programa no abre en Windows

Si compilan con CLion en Windows (usa MinGW) y luego intentan correr el `.exe` desde una terminal aparte (PowerShell, cmd) y no pasa nada o se cierra solo, es porque le faltan 3 DLLs de MinGW junto al ejecutable: `libstdc++-6.dll`, `libgcc_s_seh-1.dll` y `libwinpthread-1.dll`. Desde CLion nunca se nota este problema porque el IDE ya sabe dónde están esas DLLs, pero afuera no.

Se soluciona copiando esas 3 DLLs, una sola vez, desde la carpeta de instalación de CLion hasta la carpeta `cmake-build-debug`. Suelen estar en una ruta como:

```
C:\Program Files\JetBrains\CLion <version>\bin\mingw\bin
```

```
copy "C:\Program Files\JetBrains\CLion <version>\bin\mingw\bin\libstdc++-6.dll" .
copy "C:\Program Files\JetBrains\CLion <version>\bin\mingw\bin\libgcc_s_seh-1.dll" .
copy "C:\Program Files\JetBrains\CLion <version>\bin\mingw\bin\libwinpthread-1.dll" .
```

(reemplazar `<version>` por la versión de CLion instalada, y correr esto parado dentro de la carpeta `cmake-build-debug`).
