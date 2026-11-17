# Programación paralela — Parte 2

Plataforma de streaming de películas (C++20, CMake). En esta parte se paralelizó la **construcción del índice** (inserción en el Trie) y la **búsqueda** (verificación y puntaje de candidatos), usando solo la biblioteca estándar: `std::thread`, `std::atomic`, `std::mutex`, `std::condition_variable` y `std::call_once`.

Ninguna mejora cambia los resultados: las búsquedas devuelven el mismo contenido, con los mismos puntajes y en el mismo orden que la versión anterior (`origin/main`). La sección [Cómo se verificó](#7-cómo-se-verificó) explica cómo se comprobó.

| Qué | Antes (`origin/main`) | Ahora (12 hilos) | Detalle |
|---|---|---|---|
| Construir el índice (dataset completo) | 44,1 s | 4,0 s (11,0×) | [Tabla 1](#tabla-1--indexación) |
| Búsqueda frecuente `"the"` (dataset completo) | 2 426 ms | 244 ms (9,9×) | [Tabla 2](#tabla-2--búsqueda) |
| Sesión completa de la app (11 consultas) | 59,6 s | 15,7 s (3,8×) | [Tabla 3](#tabla-3--sesión-completa-de-la-app) |

---

## 1. Qué se paralelizó y por qué

Antes de tocar código se midió dónde se va el tiempo (Fase 1, `bench/Benchmark.cpp`, dataset completo de 34 886 películas, código de `origin/main`).

### Indexación

| Medición (Fase 1) | Valor |
|---|---|
| `MovieTrie::buildIndex` | 54–73 s |
| Parte que es solo normalizar texto | 3,7 % |
| Claves insertadas (una por carácter normalizado) | 78,4 millones |
| Memoria del índice | ~1 722 MB |

Casi todo el tiempo es **insertar claves** en el Trie (bajar 3 niveles y agregar el id a un `unordered_set` en cada nivel). Es el mayor costo absoluto del programa y ocurre en cada arranque.

### Búsqueda

Perfil de una búsqueda frecuente (`"the"`, 34 522 candidatos), por fases:

| Fase | ms | % |
|---|---|---|
| Normalizar la consulta | 0,01 | 0,0 % |
| Recorrido del Trie (`searchSubstring`) | 6,8 | 0,2 % |
| Verificación (`contiene` sobre los 5 campos normalizados) | 1 578 | 41 % |
| Puntaje (`puntajeTexto`) | 2 266 | 58 % |
| `std::sort` | 19,5 | 0,5 % |
| **Total** | **3 876** | |

Más del 99 % del tiempo es verificar y puntuar candidatos, y **cada candidato es independiente** y de solo lectura: es el caso ideal para repartir en hilos. El recorrido del Trie y el ordenamiento (<1 %) se dejan secuenciales.

El perfil mostró además que cada candidato normalizaba sus 5 campos **dos veces** (una para verificar y otra para puntuar) y el título una tercera vez. Se agregó una **normalización única** (misma lógica y mismos puntajes) y se midió aparte de los hilos.

**Decisión:** paralelizar ambas. La búsqueda tiene una fracción paralelizable cercana al 99 %; la indexación es el costo más grande y cada subárbol del Trie se puede construir por separado.

---

## 2. Estrategia de sincronización

### 2.1 Indexación: partición por primer carácter

Toda clave que empieza con el carácter `c` vive en el subárbol `root->children[c]`. Si cada carácter tiene **un único hilo dueño**, los subárboles son disjuntos y ningún nodo es escrito por dos hilos: **no hacen falta locks ni fusión**.

`MovieTrie::buildIndex(movies, hilos, observador)` (`src/03_arbol_insercion/MovieTrie(arbol).cpp`):

| Fase | Quién | Qué hace |
|---|---|---|
| A | N hilos, bloques dinámicos de 64 películas | Normaliza los 5 campos de cada película **una vez** y cuenta cuántas claves empiezan con cada byte (peso de cada subárbol). |
| B | Un hilo | Reparte los bytes entre hilos con **LPT** (el más pesado primero, al hilo con menos carga) y crea los hijos de la raíz. Desde aquí `root->children` no cambia. |
| C | N hilos, uno por partición | Cada hilo recorre todas las películas en orden de id e inserta solo las claves cuyo primer byte le pertenece. |

Por qué LPT: los caracteres no pesan lo mismo. El espacio es el 16,9 % de las claves, la `e` el 9,5 %, la `a` el 7,3 %… Repartir por orden alfabético dejaría un hilo con mucho más trabajo que los demás.

| Dato compartido | Quién escribe | Quién lee |
|---|---|---|
| `movies` | nadie | todos |
| `textos[id]` (fase A) | solo el hilo que tomó ese bloque | todos en la fase C (después del `join`) |
| `pesosPorHilo[h]` | solo el hilo `h` | el hilo que llama, después del `join` |
| `root->children` | el hilo que llama (fase B) | todos en la fase C (solo lectura) |
| subárbol del byte `c` | solo `duenio[c]` | nadie durante la construcción |
| `progreso` (`std::atomic<size_t>`) | todos (`fetch_add`, relaxed) | el hilo que llama (observador) |

Cada hilo inserta los ids en orden creciente, igual que el algoritmo secuencial, por eso cada nodo termina con exactamente el mismo conjunto de ids. `MovieTrie::mismoContenido` lo verifica nodo por nodo.

Con `hilos = 1` (o menos de 64 películas) se usa el algoritmo secuencial original.

### 2.2 Búsqueda: bloques de candidatos

`procesarCandidatos` (`src/04_busqueda/Busqueda.cpp`):

1. El Trie devuelve los candidatos (secuencial, <1 %).
2. Si hay menos candidatos que el **umbral**, todo se hace en un hilo.
3. Si no, se copian a un vector y cada hilo toma **bloques de 32** de un contador `std::atomic<size_t>` (`fetch_add`). Cada hilo verifica, puntúa y guarda en **su propio** `std::vector<Resultado>`.
4. Después del `join` se unen los vectores y se ordena con `std::sort` + `Resultado::operator<`.

`Resultado::operator<` es un **orden total** (prioridad, luego título, luego id), así que el resultado final no depende de qué hilo procesó cada candidato ni en qué orden se unieron las partes.

El reparto es dinámico y no por bloques fijos porque las sinopsis tienen largos muy distintos y el procesador tiene núcleos de distinta velocidad (4 P-cores con hyperthreading y 4 E-cores): con bloques fijos, los hilos rápidos esperarían a los lentos.

### 2.3 Singleton: `call_once` + release/acquire

`IndiceGlobal::construir` ejecuta la indexación (paralela) dentro de `std::call_once`. Todos los hilos de la indexación se unen (`join`) antes de que `construir` haga `construido.store(true, std::memory_order_release)`. `estaConstruido()` hace `load(std::memory_order_acquire)`: quien ve el índice construido ve también todo el árbol escrito por los hilos.

Después de construir, el índice es de **solo lectura** (`arbol()` y `peliculas()` devuelven referencias `const`), por eso varias búsquedas paralelas pueden leerlo **sin locks**. Cada hilo de búsqueda escribe solo en su propio vector de resultados.

### 2.4 Observer de progreso: contador atómico y un solo hilo que notifica

`IObservadorIndexacion` (`src/03_arbol_insercion/ObservadorIndexacion.h`):

- Los hilos trabajadores **no** llaman al observador: solo hacen `progreso.fetch_add(insertadas, relaxed)` al terminar cada película.
- El hilo que llamó a `buildIndex` no inserta: espera con `condition_variable::wait_for(50 ms)` y en cada vuelta notifica `onProgresoIndexacion(progreso, total)`. Nunca hay llamadas concurrentes al observador.
- La última notificación siempre es `(total, total)`.

En la interfaz, `BarraProgreso` solo guarda el avance en dos atómicos (no imprime ni bloquea la indexación) y el hilo principal dibuja 20 `#` proporcionales al avance real.

---

## 3. Diagrama del flujo

```mermaid
flowchart TD
    subgraph IDX["Indexación — MovieTrie::buildIndex"]
        direction TB
        I0{"hilos = 1 o<br/>menos de 64 películas?"}
        I0 -- sí --> SEQ["Algoritmo secuencial original"]
        I0 -- no --> A
        A["Fase A (N hilos, bloques de 64 películas)<br/>normalizar 5 campos una vez<br/>+ contar claves por primer byte"]
        A --> JA(["join"])
        JA --> B["Fase B (1 hilo)<br/>LPT: byte → hilo dueño<br/>crear hijos de la raíz"]
        B --> C1["Fase C, hilo 0<br/>solo los subárboles de sus bytes"]
        B --> C2["Fase C, hilo 1<br/>solo los subárboles de sus bytes"]
        B --> C3["Fase C, hilo N-1<br/>solo los subárboles de sus bytes"]
        C1 & C2 & C3 -- "fetch_add(progreso)" --> P[("std::atomic progreso")]
        P -. "cada 50 ms (hilo que llama)" .-> OBS["Observador → barra de progreso"]
        C1 & C2 & C3 --> JC(["join"])
        JC --> REL["IndiceGlobal: construido.store(true, release)"]
    end

    subgraph BUS["Búsqueda — BusquedaTexto / BusquedaTag"]
        direction TB
        Q["Normalizar consulta"] --> T["Trie: candidatos (searchSubstring)"]
        T --> U{"candidatos < umbral?"}
        U -- sí --> S1["1 hilo: verificar + puntuar"]
        U -- no --> K[("std::atomic siguiente")]
        K -- "fetch_add(32)" --> W1["Hilo 0 → partes[0]"]
        K -- "fetch_add(32)" --> W2["Hilo 1 → partes[1]"]
        K -- "fetch_add(32)" --> W3["Hilo N-1 → partes[N-1]"]
        W1 & W2 & W3 --> JB(["join + unir partes"])
        S1 --> SORT
        JB --> SORT["std::sort con Resultado::operator< (orden total)"]
    end

    REL -. "solo lectura, sin locks" .-> T
```

---

## 4. Configuración

| Dónde | Parámetro | Valor por defecto |
|---|---|---|
| `main` | `--hilos N` | `std::thread::hardware_concurrency()` |
| `MovieTrie::buildIndex(movies, hilos, observador)` | `hilos` | `0` = valor de `--hilos` o `hardware_concurrency` |
| `BusquedaTexto(hilos, umbral)` | `umbral` | 32 candidatos |
| `BusquedaTag(campo, hilos, umbral)` | `umbral` | 512 candidatos |
| `MovieTrie::MIN_PELICULAS_PARALELO` | — | 64 películas |

Con `hilos = 1` se usan exactamente los algoritmos secuenciales.

---

## 5. Resultados

### 5.1 Condiciones y protocolo

| Condición | Valor |
|---|---|
| Procesador | Intel Core 5 210H: 8 núcleos (4 P-cores con hyperthreading + 4 E-cores) = 12 hilos de hardware, frecuencia máxima 4,8 GHz |
| Memoria / SO / compilador | 23 GB, Linux, GCC 16.2, `-O3 -DNDEBUG` (las dos versiones con las mismas banderas) |
| Alimentación | **batería** (cargador desconectado) |
| `powerprofilesctl` / `platform_profile` | `balanced` / `balanced` |
| `scaling_governor` / `energy_performance_preference` | `powersave` / `balance_power` (12 CPUs) |
| Frecuencia observada durante las mediciones | promedio 1,1–1,3 GHz, con bajadas hasta ~570 MHz (`scaling_cur_freq`, muestreada cada 0,5 s) |

**Base** es el código real de `origin/main` (55a2ddc): `bench/BenchRonda.cpp` se compila contra esa versión y contra la actual con las mismas banderas. Cada medición es un proceso nuevo y las configuraciones se **intercalan** (en cada ronda se ejecuta una vez cada configuración, rotando el orden).

| Medición | Repeticiones usadas | Calentamiento |
|---|---|---|
| Indexación, dataset completo | 5 (rondas 0–4) | ninguno |
| Búsqueda, dataset completo | 9 (rondas 1–9) | ronda 0 descartada + cada consulta se ejecuta una vez sin medir dentro del proceso |
| Indexación y búsqueda, `--limite 1000` | 9 (rondas 1–9) | igual que la búsqueda |
| Sesión completa de la app | 3 rondas intercaladas (base/actual alternando el orden) | ninguno |

Cada celda muestra **mediana (mín–máx, ±rango relativo)**, con rango relativo = (máx − mín) / mediana. **⚠ = poco fiable**: el rango supera el 25 % de la mediana.
El **speedup** es la razón entre medianas medidas en las mismas condiciones, mediana(base) / mediana(config); entre paréntesis va el intervalo pesimista–optimista, mín(base)/máx(config) – máx(base)/mín(config).

### Tabla 1 — Indexación

`MovieTrie::buildIndex`. VmHWM = pico de memoria del proceso (incluye el CSV cargado).

**Dataset completo (34 886 películas), 5 repeticiones**

| Config | Tiempo ms | Speedup | VmHWM MB |
|---|---|---|---|
| base (`origin/main`) | 44 054 (39 581–45 243, ±13 %) | 1,00× | 1 821 |
| 1 hilo | 41 546 (39 384–43 768, ±11 %) | 1,06× (0,90–1,15) | 1 821 |
| 2 hilos | 15 897 (15 600–17 378, ±11 %) | **2,77×** (2,28–2,90) | 1 902 |
| 4 hilos | 8 143 (7 864–8 733, ±11 %) | **5,41×** (4,53–5,75) | 1 902 |
| 12 hilos | 3 992 (3 847–4 094, ±6 %) | **11,04×** (9,67–11,76) | 1 903 |

**`--limite 1000`, 9 repeticiones**

| Config | Tiempo ms | Speedup | VmHWM MB |
|---|---|---|---|
| base | 441 (431–671, ±54 %) ⚠ | 1,00× | 101 |
| 1 hilo | 446 (439–805, ±82 %) ⚠ | 0,99× (0,53–1,53) | 101 |
| 2 hilos | 202 (194–286, ±46 %) ⚠ | 2,19× (1,50–3,45) | 122 |
| 4 hilos | 113 (99–190, ±81 %) ⚠ | 3,89× (2,27–6,77) | 132 |
| 12 hilos | 71 (51–108, ±79 %) ⚠ | 6,19× (4,00–13,05) | 137 |

En `--limite 1000` todos los rangos superan el 25 % por una o dos corridas aisladas lentas (los máximos); los mínimos y las medianas son estables.

### Tabla 2 — Búsqueda

**base** = código de `origin/main`; **norm. única** = código actual con 1 hilo (solo la normalización única); **2 / 4 / 12 hilos** = normalización única + hilos. Las 8 consultas devolvieron resultados idénticos (ids y puntajes, en el mismo orden) en todas las rondas y configuraciones.

**Dataset completo, 9 repeticiones (ms)**

| Consulta | Candidatos → resultados | base | norm. única | 2 hilos | 4 hilos | 12 hilos | Speedup 12 hilos |
|---|---|---|---|---|---|---|---|
| texto `"ghost ship"` | 2 192 → 4 | 136 (134–155, ±16 %) | 138 (134–163, ±21 %) | 74,7 (74,1–77,3, ±4 %) | 43,6 (40,8–51,9, ±26 %) ⚠ | 34,6 (23,5–46,3, ±66 %) ⚠ | 3,92× (2,89–6,60) |
| texto `"the"` | 34 522 → 34 522 | 2 426 (2 393–2 722, ±14 %) | 1 457 (1 421–1 663, ±17 %) | 790 (786–872, ±11 %) | 419 (410–453, ±10 %) | 244 (238–274, ±15 %) | **9,95×** (8,72–11,44) |
| texto `"love"` | 14 688 → 14 290 | 1 398 (1 339–1 622, ±20 %) | 739 (725–823, ±13 %) | 404 (400–416, ±4 %) | 211 (207–216, ±4 %) | 123 (120–137, ±13 %) | **11,38×** (9,79–13,47) |
| texto `"ar"` | 34 430 → 34 430 | 2 667 (2 549–3 028, ±18 %) | 1 483 (1 421–1 559, ±9 %) | 792 (790–818, ±4 %) | 416 (408–428, ±5 %) | 243 (240–267, ±11 %) | **10,97×** (9,56–12,60) |
| texto `"a"` | 34 886 → 34 886 | 1 874 (1 782–2 072, ±15 %) | 1 474 (1 416–1 653, ±16 %) | 785 (778–806, ±4 %) | 408 (404–427, ±6 %) | 243 (240–272, ±13 %) | **7,71×** (6,55–8,64) |
| director `"spielberg"` | 12 515 → 31 | 7,90 (6,89–11,6, ±59 %) ⚠ | 7,93 (6,82–9,46, ±33 %) ⚠ | 8,25 (7,29–11,4, ±50 %) ⚠ | 5,62 (5,47–7,17, ±30 %) ⚠ | 3,95 (3,64–8,70, ±128 %) ⚠ | 2,00× (0,79–3,17) |
| reparto `"john"` | 6 208 → 2 161 | 14,3 (13,1–20,8, ±53 %) ⚠ | 11,2 (10,1–12,3, ±19 %) | 9,06 (7,70–13,2, ±61 %) ⚠ | 5,50 (5,13–7,73, ±47 %) ⚠ | 4,08 (3,73–11,3, ±185 %) ⚠ | 3,52× (1,16–5,57) |
| género `"drama"` | 15 419 → 9 475 | 17,7 (17,0–22,7, ±32 %) ⚠ | 15,5 (14,7–17,8, ±20 %) | 16,7 (14,3–27,6, ±80 %) ⚠ | 12,6 (11,4–22,7, ±90 %) ⚠ | 10,0 (9,42–20,6, ±112 %) ⚠ | 1,77× (0,82–2,41) |

Descomposición en las consultas frecuentes: la normalización única da entre 1,3× y 1,9× (por ejemplo `"the"`: 2 426 → 1 457 ms), y los hilos multiplican por ~6× más (1 457 → 244 ms). En `"ghost ship"` la normalización única no cambia nada: 2 188 de sus 2 192 candidatos no coinciden y esos ya se normalizaban una sola vez.

**`--limite 1000`, 9 repeticiones (ms)**

| Consulta | Resultados | base | norm. única | 2 hilos | 4 hilos | 12 hilos | Mejor speedup |
|---|---|---|---|---|---|---|---|
| texto `"ghost ship"` | 0 | 1,73 (1,71–1,85, ±8 %) | 1,74 (1,72–2,02, ±17 %) | 1,74 (1,60–3,11, ±87 %) ⚠ | 1,94 (1,78–4,01, ±115 %) ⚠ | 1,59 (1,50–3,11, ±101 %) ⚠ | 1,08× (0,55–1,23) |
| texto `"the"` | 986 | 38,4 (37,1–43,0, ±15 %) | 26,0 (24,8–27,1, ±9 %) | 17,9 (14,1–29,0, ±83 %) ⚠ | 11,8 (9,63–17,7, ±68 %) ⚠ | 7,79 (6,19–9,62, ±44 %) ⚠ | 4,92× (3,85–6,94) |
| texto `"love"` | 492 | 29,9 (29,0–32,8, ±13 %) | 16,2 (15,5–19,6, ±25 %) ⚠ | 9,44 (9,01–10,4, ±15 %) | 6,58 (5,96–11,2, ±80 %) ⚠ | 5,15 (4,35–5,91, ±30 %) ⚠ | 5,79× (4,91–7,55) |
| texto `"ar"` | 988 | 44,5 (44,3–50,8, ±15 %) | 25,7 (24,6–27,9, ±13 %) | 14,4 (14,2–14,9, ±5 %) | 8,64 (8,18–16,7, ±99 %) ⚠ | 7,70 (6,65–8,66, ±26 %) ⚠ | 5,79× (5,12–7,64) |
| texto `"a"` | 1 000 | 31,9 (31,4–34,7, ±10 %) | 24,5 (24,0–26,7, ±11 %) | 14,1 (13,7–17,8, ±29 %) ⚠ | 8,78 (8,42–16,2, ±89 %) ⚠ | 7,26 (6,90–8,81, ±26 %) ⚠ | 4,38× (3,56–5,04) |
| director `"spielberg"` | 0 | 0,08 (0,08–0,09, ±12 %) | 0,08 (0,08–0,09, ±10 %) | 0,09 (0,09–0,10, ±18 %) | 0,09 (0,09–0,16, ±85 %) ⚠ | 0,09 (0,09–0,11, ±23 %) | 0,99× (0,91–1,12) |
| reparto `"john"` | 50 | 0,20 (0,20–0,38, ±88 %) ⚠ | 0,17 (0,16–0,18, ±11 %) | 0,19 (0,18–0,20, ±7 %) | 0,19 (0,19–0,20, ±5 %) | 0,19 (0,18–0,22, ±21 %) | 1,21× (1,08–2,29) |
| género `"drama"` | 417 | 0,43 (0,42–0,74, ±73 %) ⚠ | 0,37 (0,36–0,40, ±11 %) | 0,41 (0,40–0,43, ±9 %) | 0,41 (0,40–0,41, ±3 %) | 0,42 (0,40–0,49, ±21 %) | 1,19× (1,05–2,05) |

Con `--limite 1000` las tres búsquedas por tag tienen menos de 512 candidatos (el umbral de tag), así que con cualquier número de hilos se ejecutan en un solo hilo: las diferencias entre esas columnas son ruido.

### Tabla 3 — Sesión completa de la app

Dataset completo, guion `bench/sesion_entrada.txt` (11 consultas con paginación y detalle), tiempo de reloj desde que arranca la app hasta que sale (incluye leer el CSV, ~2,6 s). 3 rondas intercaladas.

| Versión | Sesión completa | Speedup |
|---|---|---|
| base (`origin/main`) | 59,6 s (57,2–68,4, ±19 %) | 1,00× |
| actual (12 hilos) | 15,7 s (14,9–16,0, ±7 %) | **3,80×** (3,57–4,60) |

La salida fue idéntica en las 3 rondas (y en las 6 rondas que llegaron a ejecutarse). En la Fase 3, con otro perfil de energía (`quiet`), la misma sesión midió 88 s contra 24 s: los tiempos absolutos dependen de las condiciones, la razón se mantiene parecida (3,7×–3,8×).

### ¿La diferencia supera la variación?

Sí en las mediciones principales: los intervalos mín–máx **no se solapan**.

- Indexación completa: el mínimo de la base (39 581 ms) es más del doble del máximo con 2 hilos (17 378 ms).
- `"the"`: base 2 393–2 722 ms, norm. única 1 421–1 663, 2 hilos 786–872, 4 hilos 410–453, 12 hilos 238–274. Lo mismo en `"love"`, `"ar"` y `"a"`, también con `--limite 1000`.
- Sesión: base 57,2–68,4 s contra 14,9–16,0 s.

**No** en las búsquedas por tag ni en `"ghost ship"` con `--limite 1000`: sus intervalos se solapan, así que esas diferencias no son concluyentes.

### 5.2 Speedup observado frente a la ley de Amdahl

Amdahl: S(N) = 1 / (s + (1 − s) / N), con s = fracción secuencial.

**Búsqueda (`"the"`).** Se compara contra la normalización única con 1 hilo (1 457 ms), porque los hilos se aplican sobre ella. La parte secuencial es el recorrido del Trie (~7 ms) y el `std::sort` (~20 ms), según el perfil de la Fase 1: s ≈ 27 / 1 457 ≈ 1,8 %.

| Hilos | Amdahl teórico | Observado `"the"` | Observado `"love"` | Observado `"ar"` |
|---|---|---|---|---|
| 2 | 1,96× | 1,84× | 1,83× | 1,87× |
| 4 | 3,80× | 3,48× | 3,50× | 3,56× |
| 12 | 10,0× | 5,97× | 6,01× | 6,10× |

Con 2 y 4 hilos el resultado queda cerca de lo teórico. Con 12 se queda en ~6×. Las razones:

1. **12 hilos no son 12 núcleos iguales.** Son 4 P-cores con hyperthreading (dos hilos que comparten un núcleo rinden bastante menos que dos núcleos) y 4 E-cores más lentos. Si se estima la capacidad en ~7 P-cores (hyperthreading +25 %, E-core ≈ 0,55 de un P-core; son estimaciones, no mediciones), Amdahl da 1 / (0,018 + 0,982 / 7,2) ≈ 6,5×, cerca de lo observado.
2. **Menos frecuencia con más núcleos activos**: en batería y con `powersave` el procesador reparte un presupuesto de energía; con todos los núcleos ocupados cada uno va más lento.
3. **Memoria compartida**: cada candidato crea 5 strings normalizados (`malloc`) y lee su sinopsis; todos los hilos comparten el ancho de banda de memoria y el asignador.
4. Crear y unir los hilos en cada búsqueda (fracción de milisegundo).

**Indexación.** En el algoritmo particionado casi no hay parte secuencial (la fase B y crear los hilos toman menos de 1 ms), así que Amdahl permitiría hasta ~12× (o ~7× con la capacidad real del procesador). Además, si cada clave costara lo mismo, la partición más pesada (el espacio, 16,9 % de las claves) limitaría el speedup a ~5,9×. Sin embargo, se observó **11,0× contra la base**, más que ambos límites. Un experimento aparte (no incluido en el repositorio: el algoritmo particionado forzado a 1 hilo, 3 repeticiones alternadas con el secuencial) lo explica:

| Algoritmo, 1 hilo | Mediana |
|---|---|
| Secuencial original | 43 582 ms |
| Particionado | 35 476 ms (1,23× más rápido) |

- **1,23× viene del algoritmo**, no de los hilos: `insertarDesde` hace una sola búsqueda en el mapa de hijos por nivel, mientras `insertKey` hace dos (o tres si crea el nodo) y además crea un `substr`.
- **El resto (35 476 / 3 992 ≈ 8,9×) viene del paralelismo**, y con 2 y 4 hilos es levemente **superlineal** (2,23× y 4,36× frente al particionado con 1 hilo). La hipótesis (no medida, porque no hay `perf` en el equipo) es la caché: cada hilo solo toca los nodos y conjuntos de ids de sus subárboles, así que su conjunto de trabajo es mucho más pequeño que el del algoritmo secuencial, que salta por todo el árbol. Por eso el costo por clave no es uniforme y el límite de 5,9× no se cumple.

---

## 6. Limitaciones

1. **Condiciones de medición.** Todo se midió con **batería**, perfil `balanced` y governor `powersave` (`energy_performance_preference = balance_power`), sin cambiar ajustes del sistema. El procesador trabajó en promedio a 1,1–1,3 GHz (máximo 4,8 GHz), con bajadas hasta ~570 MHz durante las mediciones. Variación observada (rango relativo a la mediana):
   - indexación con el dataset completo: ±6 % a ±13 %;
   - búsquedas de texto con el dataset completo: ±4 % a ±21 %;
   - búsquedas por tag (pocos milisegundos) y casi todo `--limite 1000`: muchas mediciones entre ±25 % y ±185 % (⚠), porque una sola corrida lenta infla el máximo.

   Por eso los **tiempos absolutos** dependen de las condiciones (enchufado o con perfil de rendimiento serían menores), y el **speedup** se informa solo como razón entre mediciones hechas en las mismas condiciones y de forma intercalada. Aun así, el speedup también cambia con las condiciones: en la Fase 2, con perfil `quiet`, la indexación con 12 hilos dio 7,1× en vez de 11,0×.
2. **Repeticiones limitadas.** Para acortar la medición se usaron 5 repeticiones en la indexación completa y 3 rondas en la sesión de la app (9 en el resto). Con tan pocas muestras la mediana es razonable, pero el rango puede subestimar la variación real.
3. **Ley de Amdahl y procesador híbrido.** La búsqueda frecuente escala bien hasta 4 hilos (3,5× de 3,8× teórico) y se queda en ~6× con 12 (sección 5.2). Más hilos que `hardware_concurrency()` no aceleran.
4. **El último 5 % de la indexación es más lento.** En la barra de progreso con 12 hilos, los primeros 19 `#` aparecieron en 6,7 s y el último en 8,3 s (medición de la Fase 3). LPT reparte el *número de claves*, no el costo real, y una sola partición (la del espacio, 16,9 % de las claves) no se puede dividir: los hilos con las particiones más pesadas terminan solos al final.
5. **Memoria temporal.** La fase A guarda los textos normalizados de todas las películas (~81 MB con el dataset completo): el pico sube de 1 821 MB a 1 903 MB (+82 MB). Se liberan al terminar, pero el asignador puede no devolverlos al sistema. Con `--limite 1000` el pico sube de 101 a 137 MB, probablemente por las zonas de memoria que el asignador reserva para cada hilo.
6. **Las búsquedas por tag ganan poco.** Verificar un candidato por tag cuesta ~1 µs (se normaliza un solo campo corto), así que pesan más las partes secuenciales: copiar el conjunto de candidatos, crear los hilos, unir los resultados y ordenarlos. Con el dataset completo bajan de 8–18 ms a 4–10 ms, pero los rangos se solapan (no concluyente).
7. **Umbrales.** Por debajo de 32 candidatos (texto), 512 candidatos (tag) o 64 películas (indexación) se usa un solo hilo, porque crear hilos cuesta más de lo que se ahorra. Los valores salen del barrido de este equipo (`benchmark --modo umbral`); en otro procesador el punto de cruce puede ser distinto. Justo por encima del umbral la ganancia es pequeña (por ejemplo `"ghost ship"` con `--limite 1000`: 35 candidatos, 1,08×, no concluyente).
8. **Normalización única.** No ayuda cuando casi todos los candidatos se descartan (`"ghost ship"`), porque esos ya se normalizaban una sola vez.

---

## 7. Cómo se verificó

1. **Salida idéntica de la app.** Un guion (`bench/sesion_entrada.txt`) ejecuta 11 consultas con paginación (siguiente, siguiente, anterior, abrir el detalle y volver): palabra (`ghost`, `war`), frase (`love story`), sub-palabra (`ar`), muy frecuente (`the`), sin resultados (`zxqv`), director (`griffith`), reparto (`chaplin`), género (`drama`, `comedy`) y un tag sin resultados. La salida de la versión nueva se comparó con la de `origin/main` ignorando solo los tiempos y la línea de la barra de progreso. Fue **idéntica** con `--limite 1000` y con el dataset completo, con `--hilos 1`, `2` y `12`, y en todas las rondas de la Tabla 3.
2. **Pruebas** (`tests/ParaleloTests.cpp`, `ctest`):
   - el índice paralelo es igual al secuencial nodo por nodo con 1, 2, 4, 2×núcleos+3 y hilos automáticos, también al reconstruir;
   - la búsqueda de texto y por tag con varios hilos devuelve lo mismo (ids, puntajes y orden) que la secuencial y que el algoritmo anterior, con umbral por defecto y con umbral 0;
   - el observador recibe todas las notificaciones desde el hilo que llama, con avance monótono, total constante y final `total/total`;
   - casos con 0, 1 y 3 películas y con 64 películas y 300 hilos.

   Se comprobó que las pruebas detectan errores: un fallo introducido a propósito en el árbol paralelo hizo fallar 11 comprobaciones, y uno en la búsqueda paralela, 90.
3. **ThreadSanitizer** (`-fsanitize=thread`, build aparte): **0 data races** en las 8 pruebas de `ctest` y en la app con `--limite 1000` y `--hilos 1`, `2` y `12`. Un programa con un race a propósito confirmó que TSan estaba activo.
4. Las mediciones de `bench/comparar_versiones.sh` verifican en cada ronda que el hash de (id, puntaje) de cada consulta sea igual al de `origin/main`.

---

## 8. Cómo reproducir

```sh
cmake -S . -B cmake-build-release -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release -j
ctest --test-dir cmake-build-release --output-on-failure

# Comparación intercalada contra origin/main (10 rondas, la primera se descarta)
bench/comparar_versiones.sh motor --rondas 10 --limite 1000 --salida motor_1000.csv
bench/comparar_versiones.sh motor --rondas 10 --salida motor_full.csv
bench/comparar_versiones.sh sesion --rondas 10 --salida sesion.csv
python3 bench/resumir.py motor_full.csv --rondas 1-9   # --rondas 0-4 para la indexacion

# Perfil por fases, barrido de umbrales y verificación del árbol
./cmake-build-release/benchmark --modo perfil
./cmake-build-release/benchmark --modo umbral
./cmake-build-release/benchmark --modo indice --verificar
```

---

## 9. Guía de exposición

### Archivos, en este orden

| # | Archivo | Qué mostrar |
|---|---|---|
| 1 | Este documento, sección 1 | El perfil: más del 99 % de la búsqueda es verificar y puntuar; la indexación tarda decenas de segundos. |
| 2 | `src/03_arbol_insercion/MovieTrie(arbol).cpp` → `buildParalelo` | El comentario de "datos compartidos" y las fases A, B y C. |
| 3 | Mismo archivo → `repartirLPT` y `construirParticion` | Cómo se asigna cada byte a un hilo y la línea `if (duenio[b] != h) continue;`. |
| 4 | `src/comun/Paralelismo.h` → `ejecutarEnHilos` | Creación de hilos, `join` de todos y propagación de excepciones con `std::exception_ptr`. |
| 5 | `src/04_busqueda/Busqueda.cpp` → `procesarCandidatos` | Bloques con `fetch_add`, un vector por hilo, unión y `std::sort`. |
| 6 | `src/03_arbol_insercion/IndiceGlobal.h` | `call_once` + release/acquire: por qué se puede leer sin locks. |
| 7 | `src/03_arbol_insercion/ObservadorIndexacion.h` e `Interfaz::construirIndice` | Contador atómico y notificación desde un solo hilo; la barra en vivo. |
| 8 | `tests/ParaleloTests.cpp` | `mismoContenido` y la comparación de resultados. |
| 9 | Sección 5 | Tablas y speedups. |

**Demostración en vivo:** `./proyecto_progra3_1 --hilos 1` y luego sin `--hilos` sobre el dataset completo, para ver la barra y el tiempo de la etapa `[3/3]`.

### Fragmentos clave

Cómo se reparte el trabajo de la indexación (cada hilo solo toca sus subárboles):

```cpp
auto construirParticion = [&](size_t h) {
    for (size_t id = 0; id < n; ++id) {
        size_t insertadas = 0;
        for (const std::string& texto : textos[id]) {
            for (size_t i = 0; i < texto.size(); ++i) {
                const size_t b = byteDe(texto[i]);
                if (duenio[b] != h) continue;          // no es de este hilo
                insertarDesde(tablaRaiz[b], texto, i, KEY_SIZE, id);
                ++insertadas;
            }
        }
        if (insertadas > 0) progreso.fetch_add(insertadas, std::memory_order_relaxed);
    }
};
```

Cómo se reparte y se une el trabajo de la búsqueda:

```cpp
paralelo::ejecutarEnHilos(hilos, [&](size_t h) {
    for (;;) {
        const size_t inicio = siguiente.fetch_add(BLOQUE_CANDIDATOS, std::memory_order_relaxed);
        if (inicio >= lista.size()) break;
        const size_t fin = std::min(lista.size(), inicio + BLOQUE_CANDIDATOS);
        for (size_t i = inicio; i < fin; ++i) procesar(lista[i], partes[h]);   // vector propio
    }
});
// después del join: unir partes[0..N-1] y std::sort con Resultado::operator< (orden total)
```

### Preguntas probables del docente

**1. ¿Por qué no un lock por nodo?**
Todas las claves pasan por los nodos del primer nivel (hay unos 166 bytes distintos y el espacio es el primer carácter del 17 % de las claves), así que todos los hilos competirían por los mismos pocos locks y se ejecutarían casi en serie. Además, cada inserción tomaría y soltaría 3 locks: con 78 millones de claves eso son más de 200 millones de operaciones de lock, además de la memoria del `mutex` en cada nodo. Con la partición por primer carácter cada nodo tiene un único dueño y no hace falta ningún lock.

**2. ¿Por qué no un Trie parcial por hilo y después fusionar?**
Por memoria y por la fusión secuencial. Cada Trie parcial repetiría la estructura de nodos y los conjuntos de ids, así que el pico sería aproximadamente el doble (estimado: de ~1,8 GB a ~3,5 GB). Además, la fusión inserta de nuevo todos los ids en el árbol final; si se hace en un hilo, es una parte secuencial grande (Amdahl), y si se hace en paralelo, vuelve el problema de los locks. La partición por primer carácter construye directamente el árbol final, sin copias ni fusión, con solo ~81 MB temporales.

**3. ¿Qué pasa con más hilos que núcleos?**
Funciona y da el mismo resultado (hay pruebas con 2×núcleos+3 y con 300 hilos), pero no acelera más: los hilos extra se turnan en los mismos núcleos y agregan costo de creación y cambios de contexto. Por eso se limita el número de hilos: en la indexación, como máximo uno por carácter distinto; en la búsqueda, como máximo uno por bloque de 32 candidatos. El valor por defecto es `hardware_concurrency()` (12 en este equipo: 8 núcleos físicos, de los cuales solo 4 tienen hyperthreading).

**Pregunta adicional probable: ¿por qué la indexación acelera 11× con 12 hilos si la búsqueda solo 6×?**
Porque no todo el 11× es paralelismo: el algoritmo particionado ya es 1,23× más rápido con un solo hilo (hace menos búsquedas por nivel y no crea `substr`), y cada hilo trabaja sobre un conjunto de nodos más pequeño, que aprovecha mejor la caché (hipótesis, sección 5.2). La búsqueda, en cambio, ya tenía el trabajo bien distribuido; en ella el límite es el procesador híbrido (hyperthreading y E-cores) y el ancho de banda de memoria.
