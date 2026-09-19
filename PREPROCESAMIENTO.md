# Preprocesamiento — Integrante 2

## Flujo

`CSV → CSVReaderDiagnostic::processFile → vector<RawMovie> → diagnóstico original → DataCleaner::clean → mismo vector limpio → futuro árbol`

El lector del Integrante 1 se conserva: interpreta comas, comillas escapadas y registros multilínea. `RawMovie` se trasladó sin alterar sus ocho campos a `RawMovie.h`, para compartirlo con el limpiador. Se corrigió la ruta del CSV a `data/wiki_movie_plots_deduped.csv`; el lector mantiene su alternativa `../` para ejecutar desde el directorio de compilación dentro del proyecto.

La limpieza modifica el vector en memoria, mantiene el orden y todos los registros, incluso duplicados. `CSVWriter::writeFile` guarda ese vector en `data/movies_clean.csv`, sin modificar el original. En ejecuciones siguientes se lee directamente el CSV limpio con el lector existente, sin repetir la limpieza. No se implementa un árbol.

Para regenerarlo cuando cambien los datos originales o las reglas, ejecutar el programa con `--rebuild-clean`. La regeneración es explícita; no se detectan automáticamente cambios en el original.

`locateDataFile` busca el archivo desde la raíz o desde un directorio de compilación hijo. `CSVWriter::writeFile` escribe las mismas ocho columnas en UTF-8, entrecomilla los campos mediante `writeField` y duplica comillas literales para respetar el formato CSV. Publica el archivo mediante un renombrado solo después de cerrar correctamente el temporal; informa errores de escritura con una excepción.

## Funciones de DataCleaner

- `clean(movies)`: recorre cada registro y sus ocho campos, limpia su contenido, reconoce faltantes según la columna y devuelve un reporte nuevo. Usa punteros a miembros de `RawMovie` para aplicar las mismas reglas sin repetir ocho bloques de código.
- `cleanText(text, report)`: elimina espacios en los extremos y reduce separadores consecutivos a uno. Reemplaza controles ASCII (incluidos tabuladores, CR, LF y DEL) y espacios no separables UTF-8 por separadores. Quita BOM y espacios de ancho cero. Cuenta las correcciones. Conserva comillas, comas, puntuación, acentos y los demás bytes UTF-8.
- `lowercaseText(text)`: devuelve una copia con ASCII y letras mayúsculas Latin-1 en minúsculas. Incluye Ñ y vocales acentuadas; preserva otros alfabetos. No es una conversión Unicode universal, no elimina tildes ni unifica caracteres compuestos/descompuestos.
- `normalizeForSearch(text)`: combina limpieza y minúsculas en una copia. Se puede aplicar tanto al campo textual que se indexará como a la consulta. No altera el registro ni las métricas de limpieza. No debe aplicarse a URL ni identificadores.
- `isMissingMarker(text, column)`: reconoce `unknown`, sin distinguir mayúsculas, solo en director, reparto y género; también `-` en género. No clasifica títulos ni sinopsis como faltantes por su contenido: hay dos títulos reales `Unknown`.
- `printReport(report, out)`: presenta totales y faltantes por columna en el flujo de salida recibido.

`require` en el archivo de pruebas comprueba una condición y lanza un error descriptivo si falla, incluso en compilaciones Release.

## Decisiones para explicar

1. **Separar responsabilidades:** el lector interpreta el formato CSV; el limpiador trabaja sobre campos ya extraídos. No se vuelven a quitar comillas que forman parte del contenido.
2. **Conservar información:** títulos, nombres y sinopsis mantienen su presentación; las minúsculas se generan bajo demanda. Años siguen siendo cadenas, sin conversiones numéricas que borren ceros iniciales. Las URL conservan mayúsculas y puntuación.
3. **Tratar faltantes sin inventar datos:** un campo vacío o compuesto solo por separadores queda vacío. Los marcadores reconocidos pasan a vacío, pero su origen queda contado en el reporte. No se imputan director, reparto ni género.
4. **Aplicar reglas según la columna:** `Unknown` no siempre significa desconocido. Los duplicados diagnosticados tampoco se eliminan automáticamente.
5. **Verificar idempotencia:** limpiar dos veces debe dejar los mismos datos; en la segunda pasada no hay correcciones. El reporte corresponde a cada pasada, no acumula ejecuciones anteriores.
6. **Preparar la integración futura:** el árbol podrá asociar `DataCleaner::normalizeForSearch(movie.title)` con el registro correspondiente del vector. Las consultas deben usar la misma función. Aún no se construye ningún índice.

Ejemplo: `  Él dijo: "Hola,  mundo".\r\nFin.  ` queda como `Él dijo: "Hola, mundo". Fin.`. Su versión para búsqueda es `él dijo: "hola, mundo". fin.`.

## Reporte del dataset local

| Métrica | Cantidad |
|---|---:|
| Registros procesados y conservados | 34 886 |
| Campos modificados | 33 123 |
| Campos vacíos tras limpiar separadores | 1 490 |
| Marcadores convertidos a vacío | 7 211 |
| Campos faltantes finales | 8 701 |
| Espacios sobrantes eliminados | 100 584 |
| Espacios no separables reemplazados | 1 504 |
| Controles ASCII reemplazados | 198 554 |
| BOM / espacios de ancho cero eliminados | 6 |

Un campo modificado se cuenta una sola vez aunque tenga varias correcciones. Los contadores de caracteres describen operaciones: un salto puede convertirse en espacio y ese espacio luego eliminarse por repetido; por ello no deben sumarse como si fueran caracteres distintos. CR y LF se cuentan individualmente. Los espacios eliminados incluyen separadores producidos por reemplazos. Vacíos y marcadores son categorías separadas y sí se suman para obtener los faltantes finales.

## Verificación

Desde la raíz del proyecto, con CMake 4.0 o superior y un compilador C++20:

```sh
cmake -S . -B cmake-build-debug
cmake --build cmake-build-debug
ctest --test-dir cmake-build-debug --output-on-failure
./cmake-build-debug/proyecto_progra3_1
# Regenerar el archivo limpio y mostrar el reporte de limpieza:
./cmake-build-debug/proyecto_progra3_1 --rebuild-clean
```

Las pruebas cubren faltantes, preservación de títulos `Unknown`, duplicados, ceros iniciales, URL, acentos, comillas, controles, secuencias UTF-8, métricas e idempotencia. Además se verificó sobre el CSV local que se conservan los 34 886 registros, todos los años y URL, títulos y sinopsis no vacíos, y que una segunda pasada no modifica campos.

La limpieza es lineal en el volumen de texto. No mantiene otra copia completa del dataset para búsquedas: genera cada clave cuando se solicita. El reporte original del Integrante 1 sigue describiendo los datos antes de limpiar.

Las pruebas de exportación verifican comillas, comas, saltos, UTF-8, campos vacíos, reemplazo del archivo y errores de escritura. Se verificó también la igualdad de los ocho campos de cada película al volver a leer el CSV generado. Los CSV están excluidos de Git por el `.gitignore` existente; cada integrante puede generar el archivo limpio ejecutando el programa con el original disponible.
