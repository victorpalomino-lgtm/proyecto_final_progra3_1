#!/usr/bin/env bash
# Compara el codigo BASE (por defecto 55a2ddc, el ultimo commit sin paralelismo) con el actual.
# Antes la base por defecto era origin/main, pero desde que se fusiono el paralelismo
# origin/main ya lo incluye y la comparacion dejaba de tener sentido.
#
# Protocolo para reducir el ruido sin tocar el sistema:
#  - mediciones INTERCALADAS: en cada ronda se ejecuta una vez cada configuracion
#    (base, 1, 2, 4, N hilos), rotando el orden de ronda en ronda;
#  - cada ejecucion es un proceso nuevo (asi VmHWM es el pico de esa configuracion);
#  - la ronda 0 es de calentamiento y el resumen la descarta;
#  - se muestrea la frecuencia de la CPU (scaling_cur_freq, solo lectura) antes, durante
#    (cada 0.5 s) y despues de cada bloque.
#
# Uso (desde la raiz del repo):
#   bench/comparar_versiones.sh motor  [--rondas 10] [--limite 0] [--hilos "1 2 4 12"] [--base 55a2ddc] [--salida f.csv]
#   bench/comparar_versiones.sh sesion [--rondas 10] [--base 55a2ddc] [--salida f.csv]
#   python3 bench/resumir.py f.csv [--rondas A-B]
#
# motor:  bench/BenchRonda.cpp (buildIndex + 8 consultas) compilado contra ambas versiones.
# sesion: la app completa con la entrada bench/sesion_entrada.txt (11 consultas con
#         paginacion); mide el tiempo total y verifica que la salida sea identica.
set -euo pipefail

modo=${1:-}; shift || true
rondas=10; limite=0; hilos=""; base=55a2ddc; salida=""
while [[ $# -gt 0 ]]; do
    case $1 in
        --rondas) rondas=$2; shift 2 ;;
        --limite) limite=$2; shift 2 ;;
        --hilos) hilos=$2; shift 2 ;;
        --base) base=$2; shift 2 ;;
        --salida) salida=$2; shift 2 ;;
        *) echo "Opcion desconocida: $1" >&2; exit 1 ;;
    esac
done
[[ $modo == motor || $modo == sesion ]] || { sed -n '2,20p' "$0"; exit 1; }
[[ -n $hilos ]] || hilos="1 2 4 $(nproc)"
[[ -n $salida ]] || salida="bench_${modo}_$(date +%Y%m%d_%H%M%S).csv"
CMAKE=${CMAKE:-cmake}
raiz=$(git rev-parse --show-toplevel)
cd "$raiz"

tmp=$(mktemp -d)
limpiar() {
    [[ -n ${muestreador:-} ]] && kill "$muestreador" 2>/dev/null || true
    git worktree remove --force "$tmp/base" 2>/dev/null || true
    rm -rf "$tmp"
}
trap limpiar EXIT
git worktree add -q --detach "$tmp/base" "$base"

# ---- Frecuencia de la CPU (MHz, promedio de todos los nucleos) ----
frecuencia() {
    awk '{s += $1; n++} END {printf "%.0f", s / n / 1000}' /sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq
}
iniciarMuestreo() {
    : > "$tmp/frec"
    (while true; do frecuencia >> "$tmp/frec"; echo >> "$tmp/frec"; sleep 0.5; done) &
    muestreador=$!
}
detenerMuestreo() {
    kill "$muestreador" 2>/dev/null || true; wait "$muestreador" 2>/dev/null || true; muestreador=""
    awk 'NF {s += $1; n++; if (min == "" || $1 < min) min = $1} END {printf "%.0f %.0f", s / n, min}' "$tmp/frec"
}

echo "ronda,config,metrica,valor" > "$salida"
anotar() { echo "$1,$2,$3,$4" >> "$salida"; }

{
    echo "# base=$base ($(git rev-parse --short "$base")) actual=$(git rev-parse --short HEAD)$(git diff --quiet || echo '+cambios')"
    echo "# powerprofilesctl=$(powerprofilesctl get 2>/dev/null || echo '?')"
    echo "# governor=$(sort /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor | uniq -c | tr -s ' ' | tr '\n' ';')"
    echo "# epp=$(sort /sys/devices/system/cpu/cpu*/cpufreq/energy_performance_preference 2>/dev/null | uniq -c | tr -s ' ' | tr '\n' ';')"
    echo "# platform_profile=$(cat /sys/firmware/acpi/platform_profile 2>/dev/null || echo '?')"
    echo "# cargador=$(cat /sys/class/power_supply/ADP*/online 2>/dev/null | head -1 || echo '?')"
    echo "# modo=$modo rondas=$rondas limite=$limite hilos=$hilos"
} | tee -a "$salida.info"

if [[ $modo == motor ]]; then
    fuentes=(src/02_limpieza_normalizacion/DataCleaner.cpp "src/03_arbol_insercion/MovieTrie(arbol).cpp"
             src/04_busqueda/Busqueda.cpp src/04_busqueda/BusquedaComun.cpp src/comun/Utilidades.cpp)
    banderas=(-std=c++20 -O3 -DNDEBUG -Wall -Wextra -pthread)
    g++ "${banderas[@]}" -I"$tmp/base/src" bench/BenchRonda.cpp "${fuentes[@]/#/$tmp/base/}" -o "$tmp/ronda_base"
    g++ "${banderas[@]}" -DCON_HILOS -Isrc bench/BenchRonda.cpp "${fuentes[@]}" -o "$tmp/ronda_actual"

    configs=(base); for h in $hilos; do configs+=("h$h"); done
    for ((r = 0; r < rondas; r++)); do
        n=${#configs[@]}
        for ((k = 0; k < n; k++)); do
            cfg=${configs[$(((k + r) % n))]}   # orden rotado en cada ronda
            if [[ $cfg == base ]]; then cmd=("$tmp/ronda_base"); else cmd=("$tmp/ronda_actual" --hilos "${cfg#h}"); fi
            [[ $limite -gt 0 ]] && cmd+=(--limite "$limite")
            anotar "$r" "$cfg" freq_antes "$(frecuencia)"
            iniciarMuestreo
            "${cmd[@]}" > "$tmp/out"
            read -r fprom fmin <<< "$(detenerMuestreo)"
            anotar "$r" "$cfg" freq_despues "$(frecuencia)"
            anotar "$r" "$cfg" freq_prom "$fprom"
            anotar "$r" "$cfg" freq_min "$fmin"
            while read -r clave valor; do anotar "$r" "$cfg" "$clave" "$valor"; done < "$tmp/out"
            echo "ronda $r $cfg build=$(awk '$1=="build_ms"{print $2}' "$tmp/out") ms (freq prom ${fprom} MHz)"
        done
    done
else
    "$CMAKE" -S "$tmp/base" -B "$tmp/build-base" -DCMAKE_BUILD_TYPE=Release > /dev/null
    "$CMAKE" --build "$tmp/build-base" --target proyecto_progra3_1 -j"$(nproc)" > /dev/null
    "$CMAKE" -S . -B "$tmp/build-actual" -DCMAKE_BUILD_TYPE=Release > /dev/null
    "$CMAKE" --build "$tmp/build-actual" --target proyecto_progra3_1 -j"$(nproc)" > /dev/null
    filtrar() { # se ignoran solo tiempos y la linea de la barra de progreso
        sed -E -e 's/\([0-9.]+ ?(ms|s)[^)]*\)/(T)/g' -e 's/encontradas? en [^|]*\|/encontradas en T |/; s/encontrada en .*$/encontrada en T/' \
               -e 's/^( \[3\/3\] Construccion del arbol \.*) .*$/\1 <progreso>/' "$1"
    }
    for ((r = 0; r < rondas; r++)); do
        orden=(base actual); ((r % 2)) && orden=(actual base)
        for cfg in "${orden[@]}"; do
            anotar "$r" "$cfg" freq_antes "$(frecuencia)"
            iniciarMuestreo
            inicio=$(date +%s.%N)
            "$tmp/build-$cfg/proyecto_progra3_1" < bench/sesion_entrada.txt > "$tmp/sesion_$cfg.txt" 2>&1
            fin=$(date +%s.%N)
            read -r fprom fmin <<< "$(detenerMuestreo)"
            anotar "$r" "$cfg" freq_despues "$(frecuencia)"
            anotar "$r" "$cfg" freq_prom "$fprom"
            anotar "$r" "$cfg" freq_min "$fmin"
            anotar "$r" "$cfg" sesion_s "$(awk -v a="$inicio" -v b="$fin" 'BEGIN {printf "%.3f", b - a}')"
            filtrar "$tmp/sesion_$cfg.txt" > "$tmp/sesion_$cfg.filtrada"
        done
        if cmp -s "$tmp/sesion_base.filtrada" "$tmp/sesion_actual.filtrada"; then igual=1; else igual=0; fi
        anotar "$r" actual salida_identica "$igual"
        echo "ronda $r: salida identica=$igual"
    done
fi
echo "Resultados en $salida (resumen: python3 bench/resumir.py $salida)"
