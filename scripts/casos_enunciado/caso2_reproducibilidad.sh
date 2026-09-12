#!/usr/bin/env bash
# Caso 2 del enunciado (Reproducibilidad): "5 tareas, 1 000 unidades cada
# una, boletos 10/20/30/40/50; quantum 10; seed 2026. Dos ejecuciones
# producen logs identicos y todas las tareas terminan."
#
# Corre el binario dos veces con exactamente esos parametros y compara el
# log de eventos byte a byte, ademas de confirmar que las 5 tareas llegan a
# completed_units == work_units.
set -uo pipefail

BIN="./lottery_scheduler"
CSV="scripts/casos_enunciado/data/caso2_reproducibilidad.csv"
LOG1="$(mktemp)"
LOG2="$(mktemp)"
SUM1="$(mktemp)"
trap 'rm -f "$LOG1" "$LOG2" "$SUM1"' EXIT

echo "Caso 2 -- Reproducibilidad"

"$BIN" --input "$CSV" --mode quantum --quantum 10 --seed 2026 \
    --log "$LOG1" --summary "$SUM1" >/dev/null
rc1=$?
"$BIN" --input "$CSV" --mode quantum --quantum 10 --seed 2026 \
    --log "$LOG2" >/dev/null
rc2=$?

failed=0

if [ "$rc1" -ne 0 ] || [ "$rc2" -ne 0 ]; then
    echo "  FALLO - alguna de las dos ejecuciones termino con codigo distinto de cero"
    failed=1
elif diff -q "$LOG1" "$LOG2" >/dev/null; then
    echo "  ok    - ambas ejecuciones producen el mismo log de eventos, byte a byte"
else
    echo "  FALLO - los logs de las dos ejecuciones difieren"
    diff "$LOG1" "$LOG2" | head -10
    failed=1
fi

# Todas las tareas deben terminar: completed_units == work_units en el resumen.
unfinished=$(awk -F, 'NR>1 && $3!=$4 {print $1}' "$SUM1")
if [ -z "$unfinished" ]; then
    echo "  ok    - las 5 tareas llegan a completed_units == work_units"
else
    echo "  FALLO - tareas sin terminar: $unfinished"
    failed=1
fi

if [ "$failed" -eq 0 ]; then
    echo "Resultado: Caso 2 CUMPLE"
    exit 0
else
    echo "Resultado: Caso 2 NO CUMPLE"
    exit 1
fi
