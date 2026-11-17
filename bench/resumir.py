#!/usr/bin/env python3
"""Resume un CSV de bench/comparar_versiones.sh en tablas Markdown.

- Por defecto descarta la ronda 0 (calentamiento); con --rondas A-B usa solo esas rondas.
- Por configuracion: mediana, rango (min-max) y rango relativo = (max-min)/mediana.
  Si el rango relativo supera el 25 % la medicion se marca como poco fiable (⚠).
- Speedup = mediana(base) / mediana(config), con su intervalo pesimista-optimista
  min(base)/max(config) .. max(base)/min(config).
- Verifica que los resultados de cada consulta (hash de ids y puntajes) sean iguales
  a los de la base en todas las rondas.

Uso: python3 bench/resumir.py archivo.csv [--rondas A-B]
"""
import csv
import statistics
import sys
from collections import defaultdict

UMBRAL_RUIDO = 0.25


def leer(ruta, desde=1, hasta=None):
    datos = defaultdict(lambda: defaultdict(list))  # config -> metrica -> [valores]
    chk = defaultdict(set)  # (config, consulta) -> hashes
    orden = []
    with open(ruta, newline="") as f:
        for fila in csv.DictReader(f):
            if not (int(fila["ronda"]) >= desde and (hasta is None or int(fila["ronda"]) <= hasta)):
                continue
            if fila["config"] not in orden:
                orden.append(fila["config"])
            if fila["metrica"].endswith("_chk"):
                chk[(fila["config"], fila["metrica"][2:-4])].add(fila["valor"])
                continue
            ronda = int(fila["ronda"])
            if ronda < desde or (hasta is not None and ronda > hasta):
                continue
            datos[fila["config"]][fila["metrica"]].append(float(fila["valor"]))
    return datos, chk, orden


def resumen(valores):
    med = statistics.median(valores)
    lo, hi = min(valores), max(valores)
    rel = (hi - lo) / med if med > 0 else 0.0
    return med, lo, hi, rel


def fmt(x):
    if x >= 100:
        return f"{x:,.0f}".replace(",", " ")
    if x >= 10:
        return f"{x:.1f}"
    return f"{x:.2f}"


def celda(valores):
    med, lo, hi, rel = resumen(valores)
    marca = " ⚠" if rel > UMBRAL_RUIDO else ""
    return f"{fmt(med)} ({fmt(lo)}–{fmt(hi)}, ±{rel * 100:.0f}%){marca}"


def speedup(base, valores):
    mb, lb, hb, _ = resumen(base)
    mv, lv, hv, _ = resumen(valores)
    return f"{mb / mv:.2f}× ({lb / hv:.2f}–{hb / lv:.2f})"


def main():
    desde, hasta = 1, None
    if len(sys.argv) > 3 and sys.argv[2] == "--rondas":
        a, b = sys.argv[3].split("-")
        desde, hasta = int(a), int(b)
    datos, chk, orden = leer(sys.argv[1], desde, hasta)
    # Orden fijo de columnas: base primero, luego hilos de menor a mayor (o "actual").
    orden.sort(key=lambda c: (c != "base", int(c[1:]) if c[1:].isdigit() else 0))
    rondas = len(next(iter(datos.values()))["freq_antes"])
    reps = len(next(iter(datos.values())).get("q_the_ms", [])) or rondas
    print(f"Rondas usadas: {rondas} (desde la ronda {desde}); mediciones por consulta: {reps}. "
          "Celdas: mediana (min–max, rango relativo).\n")

    if any("build_ms" in m for m in datos.values()):
        base = datos["base"]["build_ms"]
        print("| Config | buildIndex ms | Speedup (intervalo) | VmHWM MB | Frec. prom / min MHz |")
        print("|---|---|---|---|---|")
        for cfg in orden:
            m = datos[cfg]
            print(f"| {cfg} | {celda(m['build_ms'])} | {speedup(base, m['build_ms'])} | "
                  f"{fmt(statistics.median(m['hwm_build_mb']))} | "
                  f"{statistics.median(m['freq_prom']):.0f} / {min(m['freq_min']):.0f} |")

        consultas = [k[2:-3] for k in datos["base"] if k.startswith("q_") and k.endswith("_ms")]
        print("\n| Consulta | Resultados | " + " | ".join(orden) + " | Mejor speedup | Identico |")
        print("|---|---|" + "---|" * len(orden) + "---|---|")
        for q in consultas:
            fila = [celda(datos[cfg][f"q_{q}_ms"]) for cfg in orden]
            mejor = min(orden[1:], key=lambda c: statistics.median(datos[c][f"q_{q}_ms"]))
            identico = all(chk[(cfg, q)] == chk[("base", q)] and len(chk[(cfg, q)]) == 1 for cfg in orden)
            n = int(statistics.median(datos["base"][f"q_{q}_n"]))
            print(f"| {q} | {n} | " + " | ".join(fila) +
                  f" | {mejor}: {speedup(datos['base'][f'q_{q}_ms'], datos[mejor][f'q_{q}_ms'])} | "
                  f"{'si' if identico else 'NO'} |")
    else:
        base = datos["base"]["sesion_s"]
        print("| Version | Sesion completa s | Speedup (intervalo) | Frec. prom / min MHz |")
        print("|---|---|---|---|")
        for cfg in orden:
            m = datos[cfg]
            print(f"| {cfg} | {celda(m['sesion_s'])} | {speedup(base, m['sesion_s'])} | "
                  f"{statistics.median(m['freq_prom']):.0f} / {min(m['freq_min']):.0f} |")
        iguales = datos["actual"]["salida_identica"]
        print(f"\nSalida identica en {int(sum(iguales))}/{len(iguales)} rondas.")

    print("\nFrecuencia antes -> despues de cada bloque (mediana por config, MHz):")
    for cfg in orden:
        m = datos[cfg]
        print(f"  {cfg}: {statistics.median(m['freq_antes']):.0f} -> {statistics.median(m['freq_despues']):.0f}"
              f" (min durante: {min(m['freq_min']):.0f})")


if __name__ == "__main__":
    main()
