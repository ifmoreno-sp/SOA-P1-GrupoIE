#!/usr/bin/env bash
# Pruebas del Milestone 1: parseo de CSV, validaciones de entrada y CLI.
# Cubren el Caso 1 del enunciado (rechazo con codigo distinto de cero y sin
# ejecucion parcial). Los 7 casos completos llegan con el Milestone 10.
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
expect_success "CSV de 5 tareas en modo quantum" \
    "$BIN" --input "$FIXTURES/valid_5.csv" --mode quantum --quantum 10 \
    --seed 2026 --log /dev/null
expect_success "CSV de 5 tareas en modo cooperative" \
    "$BIN" --input "$FIXTURES/valid_5.csv" --mode cooperative --slice-percent 10 \
    --seed 2026 --log /dev/null --summary /dev/null --max-dispatches 100

echo "Validacion del CSV:"
expect_failure "id duplicado" \
    "$BIN" --input "$FIXTURES/invalid_duplicate_id.csv" --mode quantum \
    --quantum 10 --seed 2026 --log /dev/null
expect_failure "tickets en cero" \
    "$BIN" --input "$FIXTURES/invalid_zero_tickets.csv" --mode quantum \
    --quantum 10 --seed 2026 --log /dev/null
expect_failure "work_units en cero" \
    "$BIN" --input "$FIXTURES/invalid_zero_work_units.csv" --mode quantum \
    --quantum 10 --seed 2026 --log /dev/null
expect_failure "fila incompleta" \
    "$BIN" --input "$FIXTURES/invalid_incomplete_row.csv" --mode quantum \
    --quantum 10 --seed 2026 --log /dev/null
expect_failure "menos de 5 tareas" \
    "$BIN" --input "$FIXTURES/invalid_too_few_tasks.csv" --mode quantum \
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

echo
echo "Resultado: $passed pasaron, $failed fallaron."
[ "$failed" -eq 0 ]
