#!/usr/bin/env bash
# Caso 7 del enunciado (Estres): "25 tareas, parametros validos variados y
# varias semillas. Sin deadlock, fuga, acceso invalido, data race ni
# comportamiento indefinido."
#
# Este script NO construye el binario -- recibe la ruta de un binario ya
# instrumentado como argumento, para poder correrse contra distintas
# variantes de sanitizer (ver entorno_desarrollo.md del repo de
# conocimiento sobre por que ASan+LSan necesita Docker/Linux y TSan
# necesita correr fuera de Docker en este entorno).
#
# Uso: scripts/casos_enunciado/caso7_estres.sh <ruta-al-binario> [etiqueta]
set -uo pipefail

BIN="${1:?uso: $0 <ruta-al-binario> [etiqueta]}"
LABEL="${2:-$BIN}"
CSV="scripts/casos_enunciado/data/caso7_estres.csv"
TIMEOUT_SECS=30

# timeout no viene por defecto en macOS (coreutils); gtimeout si esta
# instalado via 'brew install coreutils', si no se corre sin limite de
# tiempo (un deadlock real se notaria de todas formas porque el script
# nunca termina).
if command -v timeout >/dev/null 2>&1; then
    TIMEOUT_CMD="timeout ${TIMEOUT_SECS}s"
elif command -v gtimeout >/dev/null 2>&1; then
    TIMEOUT_CMD="gtimeout ${TIMEOUT_SECS}s"
else
    echo "aviso: no se encontro 'timeout'/'gtimeout', corriendo sin limite de tiempo" >&2
    TIMEOUT_CMD=""
fi

echo "Caso 7 -- Estres ($LABEL)"

passed=0
failed=0

check_run() {
    local desc="$1"
    shift
    local out
    out="$($TIMEOUT_CMD "$@" 2>&1)"
    local rc=$?

    if [ "$rc" -eq 124 ]; then
        echo "  FALLO - $desc (timeout: posible deadlock)"
        failed=$((failed + 1))
        return
    fi
    if [ "$rc" -ne 0 ]; then
        echo "  FALLO - $desc (codigo de salida $rc)"
        echo "$out" | tail -5 | sed 's/^/         /'
        failed=$((failed + 1))
        return
    fi
    if echo "$out" | grep -qiE "ERROR|WARNING: (Address|ThreadSanitizer|LeakSanitizer)|runtime error|Sanitizer"; then
        echo "  FALLO - $desc (el sanitizer reporto algo)"
        echo "$out" | grep -iE "ERROR|WARNING|runtime error|Sanitizer" | head -5 | sed 's/^/         /'
        failed=$((failed + 1))
        return
    fi
    echo "  ok    - $desc"
    passed=$((passed + 1))
}

for seed in 11 22 33 44 55; do
    check_run "quantum Q=13, seed=$seed" \
        "$BIN" --input "$CSV" --mode quantum --quantum 13 --seed "$seed" \
        --log /dev/null --summary /dev/null
    check_run "cooperative P=15%, seed=$seed" \
        "$BIN" --input "$CSV" --mode cooperative --slice-percent 15 --seed "$seed" \
        --log /dev/null --summary /dev/null
done

check_run "quantum Q=1 (el mas exigente: una unidad por activacion), seed=7" \
    "$BIN" --input "$CSV" --mode quantum --quantum 1 --seed 7 \
    --log /dev/null --summary /dev/null
check_run "con --max-dispatches (ejercita el camino de STOPPED bajo estres)" \
    "$BIN" --input "$CSV" --mode quantum --quantum 5 --seed 99 \
    --max-dispatches 40 --log /dev/null --summary /dev/null

echo
echo "Resultado ($LABEL): $passed pasaron, $failed fallaron."
[ "$failed" -eq 0 ]
