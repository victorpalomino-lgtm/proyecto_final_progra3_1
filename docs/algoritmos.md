# Algoritmos propios: importancia, frases "y/o" y recomendaciones

Este documento explica los tres algoritmos de orden de la aplicación:

1. el **puntaje de importancia** de cada película en una búsqueda (qué se muestra en las 5 primeras posiciones);
2. el **orden de las frases "y/o"** (búsqueda por texto con varias palabras);
3. la **recomendación de películas similares** a las que tienen Like.

Todo lo que sigue sale del código; las referencias `archivo:línea` apuntan a la rama actual.

> **Qué significa "importante".** El CSV (Wikipedia Movie Plots) no trae popularidad, votos ni recaudación: sus columnas son año, título, origen, director, reparto, género, página de Wikipedia y sinopsis (`src/01_lectura_diagnostico/CSVReaderDiagnostic.h:114-115`). Por eso la importancia de una película se mide **respecto de la consulta**: cuánto y dónde coincide con lo que buscó el usuario.

---

## 1. Puntaje de importancia de la búsqueda por texto

`puntajeTexto` (`src/04_busqueda/BusquedaComun.cpp:65-97`) recibe los cinco campos de la película ya normalizados (minúsculas y espacios limpios, ver [preprocesamiento.md](preprocesamiento.md)), la consulta normalizada completa (`frase`) y sus palabras distintas (`terminos`).

### Pesos por campo

| Campo | Peso | Línea |
|---|---|---|
| Título | **40** | `BusquedaComun.cpp:77` |
| Director | **18** | `BusquedaComun.cpp:78` |
| Género | **14** | `BusquedaComun.cpp:79` |
| Reparto | **12** | `BusquedaComun.cpp:80` |
| Sinopsis | **5** | `BusquedaComun.cpp:81` |

Para cada campo, `sumarCampo` (`BusquedaComun.cpp:11-23`) suma:

- **peso × 4** si la frase completa aparece contigua en el campo (`BusquedaComun.cpp:15`);
- **peso** por cada palabra de la consulta que aparece en el campo.

Y además, sobre la película entera:

- **+200** si el título normalizado es exactamente igual a la consulta (`BusquedaComun.cpp:74-76`);
- **+20** si todas las palabras aparecen en algún campo (`BusquedaComun.cpp:83-95`).

"Aparece" es una búsqueda de subcadena (`contiene`, `BusquedaComun.cpp:46-48`): por eso "ship" también coincide con "friendship" o "shipwreck" (búsqueda por sub-palabra).

**Ejemplo** (consulta `ship`, una sola palabra; la frase y la palabra son iguales):

| Película | Dónde está "ship" | Puntaje |
|---|---|---|
| Título "Ship" | título igual a la consulta, y en el título | 200 + 40×4 + 40 + 20 = **420** |
| "Another Ship" | en el título | 40×4 + 40 + 20 = **220** |
| sinopsis "A ghost ship appears" | en la sinopsis | 5×4 + 5 + 20 = **45** |

### Por qué estos pesos

El título es lo que el usuario suele recordar y escribir; el director y el género son metadatos cortos, así que una coincidencia ahí es muy específica; el reparto es más largo (muchos nombres) y la sinopsis es un texto largo donde casi cualquier palabra común aparece, por eso pesa menos. El multiplicador ×4 premia que la consulta aparezca tal cual, y el +200 asegura que una película cuyo título es exactamente lo buscado salga primera.

### Empates

Los resultados se ordenan con `Resultado::operator<` (`src/04_busqueda/Busqueda.cpp:69-77`): **prioridad de mayor a menor**, luego **título normalizado** en orden alfabético y, si todavía empatan, el **id** (posición en el catálogo). Es un orden total: el resultado es siempre el mismo, con cualquier número de hilos (ver [programacion_paralela.md](programacion_paralela.md)).

Con palabras frecuentes muchas películas empatan (por ejemplo, todas las que tienen "ship" solo en la sinopsis tienen 45), y entre ellas el orden es alfabético.

### Las 5 más importantes y las siguientes 5

La interfaz muestra los resultados ya ordenados de 5 en 5 (`POR_PAGINA = 5`, `src/05_interfaz_integracion/Interfaz.cpp:25`); `[S]` muestra las 5 siguientes y `[A]` las anteriores (`Interfaz.cpp:475-478`, `Interfaz.cpp:500-513`). Ver [interfaz.md](interfaz.md).

---

## 2. Frases "y/o": orden por niveles

El enunciado pide que "barco fantasma" encuentre películas con "barco" **y/o** "fantasma". La búsqueda por texto (`BusquedaTexto::ejecutar`, `src/04_busqueda/Busqueda.cpp:105-134`) funciona así:

### Candidatos

1. La consulta se normaliza con la misma función que el árbol (`DataCleaner::normalizeForSearch`) y se separa en palabras distintas por espacios (`separarTerminos`, `BusquedaComun.cpp:26-44`).
2. Los candidatos son la **unión** de lo que devuelve el árbol para cada palabra (`candidatosTexto`, `BusquedaComun.cpp:99-106`): basta con que aparezca **una** de ellas.
3. El árbol puede devolver falsos positivos (ver [arbol.md](arbol.md)), así que cada candidato se verifica: si no contiene ninguna palabra, su prioridad es 0 y se descarta (`Busqueda.cpp:125-127`).

### Prioridad por niveles

`prioridadTexto` (`BusquedaComun.cpp:108-131`):

- **nivel** = cantidad de palabras distintas de la consulta que aparecen en algún campo de la película;
- **+1 al nivel** (bonus por frase seguida) si la consulta tiene más de una palabra y la frase completa aparece contigua en algún campo;
- **prioridad = min(nivel, 1000) × 1 000 000 + puntajeTexto** (`PESO_NIVEL = 1000000` y `NIVEL_MAXIMO = 1000`, `BusquedaComun.h:50-51`).

El factor de 1 000 000 hace que el nivel mande y que **puntajeTexto solo desempate dentro del mismo nivel**. El orden queda:

1. películas con la **frase completa y contigua** (todas las palabras + bonus);
2. películas con **todas las palabras**, aunque estén separadas;
3. películas con **menos palabras** (cuantas más, antes);

y dentro de cada grupo, por puntajeTexto, título e id.

Con **una sola palabra** el nivel es siempre 1: el orden es exactamente el del puntaje, igual que antes del cambio "y/o".

**Límite:** puntajeTexto es menor que 1 000 000 mientras la consulta tenga menos de unas 1 000 palabras distintas; el nivel se limita a 1000 para no desbordar `int` (comentario en `BusquedaComun.h:40-49`).

### Ejemplo verificado (`tests/InterfazTests.cpp`)

Consulta `ghost ship` sobre cinco películas de prueba:

| Película | Contenido | Nivel | puntajeTexto | Posición |
|---|---|---|---|---|
| Night Voyage | sinopsis "The ghost ship sails at dawn." | 2 + 1 (frase) = **3** | — | 1.ª |
| Deep Water | sinopsis "A ship carries a ghost…" | **2** | 5 + 5 + 20 = 30 | 2.ª |
| Ship | título "Ship", sinopsis "A ship." | **1** | 40 + 5 = 45 | 3.ª |
| Ghost Town | título "Ghost Town" | **1** | 40 | 4.ª |
| Silent | — | 0 | — | no aparece |

"Ship" tiene más puntaje que "Deep Water", pero queda después porque solo tiene una de las dos palabras.

Con la consulta invertida `ship ghost`, ninguna película tiene esa frase contigua: Deep Water y Night Voyage empatan en nivel 2 y puntaje 30, y se ordenan por título.

### Con el catálogo real

| Consulta | Catálogo | Antes (frase contigua) | Ahora ("y/o") |
|---|---|---|---|
| `ghost ship` | primeras 1000 | 0 | 163; las 3 primeras son las únicas que tienen ambas palabras |
| `haunted house` | primeras 1000 | 2 | 188; las 2 primeras tienen la frase contigua |
| `ghost ship` | completo (34 886) | 4 | 9 145; la primera es *Ghost Ship* (2002) |

Los conteos coinciden con un recuento independiente en Python sobre el CSV. Los tiempos están en [programacion_paralela.md](programacion_paralela.md).

### Palabras muy comunes (comportamiento conocido)

No se filtran palabras vacías (*stopwords*). Si la consulta incluye una palabra que aparece en casi todas las sinopsis, el "y/o" devuelve casi todo el catálogo: `the ship` encuentra **34 537 de 34 886** películas. El orden sigue siendo útil, porque las que tienen la frase "the ship" contigua salen primero (por ejemplo *Don't Give Up the Ship*, 1959), pero la consulta es más lenta que una de palabras raras, porque hay que verificar y puntuar casi todo el catálogo.

### La búsqueda por tag no cambia

`BusquedaTag` (`Busqueda.cpp:162-188`) sigue exigiendo la frase completa y contigua dentro del campo elegido (director, reparto o género) (`Busqueda.cpp:180`). Su puntaje es `puntajeCampoNormalizado` (`BusquedaComun.cpp:138-150`): **+40** si el campo contiene la frase y **+10** por cada palabra que contiene. Así, el tag de reparto `tom hanks` no encuentra una película cuyo reparto sea "Tom Smith, Bob Hanks": tiene las dos palabras, pero no la frase.

---

## 3. Recomendación de películas similares

`Recomendador::recomendar` (`src/05_interfaz_integracion/Recomendador.cpp:95-130`) recibe el catálogo y los ids de las películas con Like.

### Perfil de cada película

`crearPerfil` (`Recomendador.cpp:54-62`) normaliza y separa:

| Dato | Cómo se obtiene |
|---|---|
| Director | el campo director normalizado |
| Origen | el campo `Origin/Ethnicity` normalizado (por ejemplo "american") |
| Actores | el reparto separado por **comas** y normalizado: un conjunto de nombres (`separarActores`, `Recomendador.cpp:22-37`) |
| Géneros | cada **palabra** del género (separa por todo lo que no es letra o número): "romantic comedy" → {romantic, comedy}; "comedy, drama" → {comedy, drama} (`separarGeneros`, `Recomendador.cpp:40-52`) |
| Año | el año de estreno como número (0 si no hay) |

### Puntaje contra cada película con Like

`puntaje(candidata, conLike)` (`Recomendador.cpp:74-91`), con las constantes de `Recomendador.h:13-17`:

| Coincidencia | Puntos |
|---|---|
| Mismo director (y no vacío) | **+5** |
| Cada actor en común | **+3** |
| Cada palabra de género en común | **+2** |
| — si hasta aquí suma 0, la película **no** se recomienda (`Recomendador.cpp:81-83`) — | |
| Mismo origen | **+1** |
| Estrenos con 10 años o menos de diferencia | **+1** |

El origen y la época solo suman si ya hay algo en común en director, reparto o género: compartir solo "american" y la década no basta para recomendar.

El puntaje final de una candidata es la **suma** de sus puntajes contra **todas** las películas con Like (`Recomendador.cpp:114-118`). Una película parecida a varias de las que le gustan al usuario suma más.

### Qué se excluye y cómo se ordena

- No se recomiendan las películas que ya tienen Like (`Recomendador.cpp:99-113`). Las de Ver más tarde **sí** pueden aparecer.
- Solo entran las de puntaje mayor que 0.
- Se ordenan con el mismo `Resultado::operator<`: puntaje de mayor a menor, luego título e id (`Recomendador.cpp:128`).

Como el CSV limpio convierte "Unknown" en vacío en director, reparto y género (ver [preprocesamiento.md](preprocesamiento.md)), dos películas con director desconocido no se consideran del mismo director.

**Ejemplo** (`tests/ListasUsuarioTests.cpp:63-73`): con Like en una sola película, *Duel* (mismo director, un género en común, mismo origen y estreno cercano) suma 5 + 2 + 1 + 1 = **9** y queda primera; *Jaws 2* (un actor en común) va antes que *Psycho* (solo un género en común), y una película sin director, actores ni géneros en común no se recomienda.

### Costo y caché

Recorre todo el catálogo y compara cada película con cada Like: O(P × L) comparaciones de perfiles, con P películas y L Likes. Para no repetirlo en cada pantalla, la `Interfaz` guarda el resultado y lo recalcula solo cuando cambia la lista de Likes, gracias al Observer `CacheRecomendadorObserver` (ver [patrones_diseno.md](patrones_diseno.md), sección 4.1).

### Dónde se ven

En la pantalla de inicio ("RECOMENDADAS PARA USTED", las 5 primeras y el total) y en la opción 5 del menú (todas, paginadas). Ver [interfaz.md](interfaz.md).
