# Arbol y busqueda — Integrantes 3 y 4

## Arbol elegido

Estructura usada: **Trie**.

Cada nivel representa un caracter. Cada nodo guarda IDs de peliculas asociadas a la clave formada desde la raiz.

Indice usado:

```text
texto normalizado -> claves cortas -> Trie -> IDs candidatos
```

La coincidencia final se confirma en `Busqueda.cpp`. Asi se evita guardar sufijos completos de sinopsis largas.

## Insercion

Campos indexados:

- titulo
- sinopsis
- director
- reparto
- genero

Cada campo pasa por `DataCleaner::normalizeForSearch`.

Ejemplo con `barco`:

```text
bar
arc
rco
co
o
```

La clave principal tiene hasta 3 caracteres. Esto mantiene la busqueda por subcadena y reduce el uso de memoria.

## Busqueda por texto

Archivo principal: `Busqueda.cpp`.

Casos cubiertos:

- palabra: `ship`
- frase: `ghost ship`
- sub-palabra: `bar`

En frases, la consulta se separa por palabras. Una pelicula entra si contiene una o mas palabras de la frase. Despues se ordena por importancia.

## Busqueda por tag

Campos disponibles:

- director
- reparto
- genero

El Trie entrega candidatos. Luego se revisa solo el campo elegido.

## Importancia

Puntaje usado para ordenar resultados:

| Coincidencia | Puntaje |
|---|---:|
| Titulo exacto | +200 |
| Titulo | 40 |
| Director | 18 |
| Genero | 14 |
| Reparto | 12 |
| Sinopsis | 5 |
| Todas las palabras presentes | +20 |

La frase completa suma mas que palabras separadas. En empate se ordena por titulo.

## Complejidad

Sea `N` el total de caracteres indexados.

- Construccion: `O(N)`.
- Busqueda en Trie: `O(k)`, con `k <= 3`.
- Confirmacion y ranking: proporcional a la cantidad de candidatos.

## Alcance del avance

El arbol funciona como indice de candidatos. La busqueda final confirma la coincidencia antes de mostrar resultados.

