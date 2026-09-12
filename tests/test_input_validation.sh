#!/usr/bin/env bash
# Pruebas extra de validacion de CSV/CLI, mas alla de los 4 escenarios
# minimos del Caso 1 del enunciado (ver scripts/casos_enunciado/caso1_validacion.sh
# para esos). Estas nacieron en Milestone 1 (parseo/CLI) y se reforzaron en
# Milestone 8 (ausencia de ejecucion parcial); viven aqui porque son
# cobertura de ingenieria del ciclo normal de desarrollo, no evidencia que
# pida el enunciado.
#
# Rutas relativas ("tests/fixtures", el binario "./lottery_scheduler"):
# este script asume que se invoca desde la raiz del repo -- ver como lo
# invoca `make test`.
set -uo pipefail

BIN="./lottery_scheduler"
FIXTURES="tests/fixtures"
OUT="$(mktemp)"
trap 'rm -f "$OUT"' EXIT

passed=0
failed=0

expect_success() {
    local desc="$1"
    shift
    if "$@" >"$OUT" 2>&1; then
        echo "  ok   - $desc"
        passed=$((passed + 1))
    else
        echo "  FALLO- $desc (se esperaba exito, salio con codigo $?)"
        sed 's/^/         /' "$OUT"
        failed=$((failed + 1))
    fi
}

expect_failure() {
    local desc="$1"
    shift
    if "$@" >"$OUT" 2>&1; then
        echo "  FALLO- $desc (se esperaba error, salio con codigo 0)"
        sed 's/^/         /' "$OUT"
        failed=$((failed + 1))
    else
        echo "  ok   - $desc"
        passed=$((passed + 1))
    fi
}

echo "Entrada valida:"
expect_success "CSV de 5 tareas en modo cooperative" \
    "$BIN" --input "$FIXTURES/valid_5.csv" --mode cooperative --slice-percent 10 \
    --seed 2026 --log /dev/null --summary /dev/null --max-dispatches 100

echo "Validacion del CSV (mas alla de los 4 casos minimos):"
expect_failure "work_units en cero" \
    "$BIN" --input "$FIXTURES/invalid_zero_work_units.csv" --mode quantum \
    --quantum 10 --seed 2026 --log /dev/null
expect_failure "suma de tickets sobre UINT32_MAX" \
    "$BIN" --input "$FIXTURES/invalid_tickets_overflow.csv" --mode quantum \
    --quantum 10 --seed 2026 --log /dev/null
expect_failure "columna adicional" \
    "$BIN" --input "$FIXTURES/invalid_extra_column.csv" --mode quantum \
    --quantum 10 --seed 2026 --log /dev/null
expect_failure "valor faltante entre comas (5,,10)" \
    "$BIN" --input "$FIXTURES/invalid_missing_value.csv" --mode quantum \
    --quantum 10 --seed 2026 --log /dev/null
expect_failure "valor no numerico donde se espera un entero" \
    "$BIN" --input "$FIXTURES/invalid_non_numeric.csv" --mode quantum \
    --quantum 10 --seed 2026 --log /dev/null
expect_failure "archivo inexistente" \
    "$BIN" --input "$FIXTURES/no_existe.csv" --mode quantum --quantum 10 \
    --seed 2026 --log /dev/null

echo "Validacion de --yield-config (extension M9):"
expect_success "archivo de yield-config valido" \
    "$BIN" --input "$FIXTURES/valid_5.csv" --mode quantum --quantum 10 \
    --seed 2026 --log /dev/null --yield-config "$FIXTURES/yield_valid.csv"
expect_failure "encabezado invalido" \
    "$BIN" --input "$FIXTURES/valid_5.csv" --mode quantum --quantum 10 \
    --seed 2026 --log /dev/null --yield-config "$FIXTURES/yield_invalid_header.csv"
expect_failure "yield_percent fuera de rango (100)" \
    "$BIN" --input "$FIXTURES/valid_5.csv" --mode quantum --quantum 10 \
    --seed 2026 --log /dev/null --yield-config "$FIXTURES/yield_invalid_out_of_range.csv"
expect_failure "task_id que no existe en el CSV de entrada" \
    "$BIN" --input "$FIXTURES/valid_5.csv" --mode quantum --quantum 10 \
    --seed 2026 --log /dev/null --yield-config "$FIXTURES/yield_invalid_unknown_id.csv"
expect_failure "task_id repetido dentro del yield-config" \
    "$BIN" --input "$FIXTURES/valid_5.csv" --mode quantum --quantum 10 \
    --seed 2026 --log /dev/null --yield-config "$FIXTURES/yield_invalid_duplicate.csv"
expect_failure "valor faltante entre comas (1,)" \
    "$BIN" --input "$FIXTURES/valid_5.csv" --mode quantum --quantum 10 \
    --seed 2026 --log /dev/null --yield-config "$FIXTURES/yield_invalid_missing_value.csv"
expect_failure "archivo de yield-config inexistente" \
    "$BIN" --input "$FIXTURES/valid_5.csv" --mode quantum --quantum 10 \
    --seed 2026 --log /dev/null --yield-config "$FIXTURES/no_existe.csv"
expect_failure "--disable-compensation sin --yield-config" \
    "$BIN" --input "$FIXTURES/valid_5.csv" --mode quantum --quantum 10 \
    --seed 2026 --log /dev/null --disable-compensation
expect_success "--disable-compensation junto con --yield-config" \
    "$BIN" --input "$FIXTURES/valid_5.csv" --mode quantum --quantum 10 \
    --seed 2026 --log /dev/null --yield-config "$FIXTURES/yield_valid.csv" --disable-compensation

echo "Validacion de la CLI:"
expect_failure "sin --input" \
    "$BIN" --mode quantum --quantum 10 --seed 2026 --log /dev/null
expect_failure "sin --log" \
    "$BIN" --input "$FIXTURES/valid_5.csv" --mode quantum --quantum 10 --seed 2026
expect_failure "sin --mode" \
    "$BIN" --input "$FIXTURES/valid_5.csv" --quantum 10 --seed 2026 --log /dev/null
expect_failure "modo invalido" \
    "$BIN" --input "$FIXTURES/valid_5.csv" --mode rr --quantum 10 --seed 2026 \
    --log /dev/null
expect_failure "seed en cero" \
    "$BIN" --input "$FIXTURES/valid_5.csv" --mode quantum --quantum 10 --seed 0 \
    --log /dev/null
expect_failure "cooperative sin --slice-percent" \
    "$BIN" --input "$FIXTURES/valid_5.csv" --mode cooperative --seed 2026 \
    --log /dev/null
expect_failure "quantum sin --quantum" \
    "$BIN" --input "$FIXTURES/valid_5.csv" --mode quantum --seed 2026 --log /dev/null
expect_failure "--quantum en modo cooperative" \
    "$BIN" --input "$FIXTURES/valid_5.csv" --mode cooperative --slice-percent 10 \
    --quantum 10 --seed 2026 --log /dev/null
expect_failure "--slice-percent fuera de rango" \
    "$BIN" --input "$FIXTURES/valid_5.csv" --mode cooperative --slice-percent 0 \
    --seed 2026 --log /dev/null
expect_failure "bandera sin valor" \
    "$BIN" --input "$FIXTURES/valid_5.csv" --mode quantum --quantum --seed 2026 \
    --log /dev/null
expect_failure "argumento desconocido" \
    "$BIN" --input "$FIXTURES/valid_5.csv" --mode quantum --quantum 10 --seed 2026 \
    --log /dev/null --turbo

BAD_DIR="/no/existe/de/verdad"

echo "Validacion de archivos de salida:"
expect_failure "--log con ruta invalida (directorio inexistente)" \
    "$BIN" --input "$FIXTURES/valid_5.csv" --mode quantum --quantum 10 --seed 2026 \
    --log "$BAD_DIR/events.csv"
expect_failure "--summary con ruta invalida (directorio inexistente)" \
    "$BIN" --input "$FIXTURES/valid_5.csv" --mode quantum --quantum 10 --seed 2026 \
    --log /dev/null --summary "$BAD_DIR/summary.csv"

# Los checks generales de "sin ejecucion parcial" (CSV invalido / --summary
# invalido) viven en scripts/casos_enunciado/caso1_validacion.sh -- son
# evidencia del Caso 1 del enunciado, no cobertura extra. Aqui se quedan
# solo los que no estan cubiertos alla: un escenario de CLI invalida (no
# uno de los 4 del enunciado) y la validacion de --yield-config (M9).
echo "Sin ejecucion parcial (escenarios no cubiertos en caso1_validacion.sh):"

TMP_LOG="$(mktemp -u)"
expect_no_log_created() {
    local desc="$1"
    shift
    rm -f "$TMP_LOG"
    if "$@" --log "$TMP_LOG" >"$OUT" 2>&1; then
        echo "  FALLO- $desc (se esperaba error, salio con codigo 0)"
        failed=$((failed + 1))
    elif [ -e "$TMP_LOG" ]; then
        echo "  FALLO- $desc (se creo '$TMP_LOG' pese al error)"
        failed=$((failed + 1))
    else
        echo "  ok   - $desc"
        passed=$((passed + 1))
    fi
}
expect_no_log_created "CLI invalida no crea --log" \
    "$BIN" --input "$FIXTURES/valid_5.csv" --mode rr --quantum 10 --seed 2026
expect_no_log_created "yield-config invalido no crea --log" \
    "$BIN" --input "$FIXTURES/valid_5.csv" --mode quantum --quantum 10 --seed 2026 \
    --yield-config "$FIXTURES/yield_invalid_unknown_id.csv"
rm -f "$TMP_LOG"

echo
echo "Resultado: $passed pasaron, $failed fallaron."
[ "$failed" -eq 0 ]
