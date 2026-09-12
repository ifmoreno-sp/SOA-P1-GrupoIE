#!/usr/bin/env bash
# Caso 1 del enunciado (Validacion): "4 tareas; tickets cero; id duplicado;
# archivo incompleto. Rechazo con codigo no cero y sin crear ejecucion
# parcial." Parsea CSV y valida CLI (src/csv_parser.c, src/cli.c, desde
# Milestone 1); tambien corre como parte de `make test`.
#
# Cubre unicamente los 4 escenarios que nombra el enunciado, mas la
# evidencia de "sin ejecucion parcial" (tambien parte de lo que exige este
# caso). El resto de la validacion de CSV/CLI que ya existia desde
# Milestone 1/8 (overflow, columna extra, flags de CLI, rutas de salida,
# etc.) vive en tests/test_input_validation.sh -- cobertura extra de
# ingenieria, no evidencia del enunciado.
#
# Rutas relativas ("tests/fixtures", el binario "./lottery_scheduler"):
# este script asume que se invoca desde la raiz del repo, sin importar
# donde viva el archivo -- ver como lo invocan `make test` y
# `make casos-enunciado`.
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

echo "Entrada valida (control):"
expect_success "CSV de 5 tareas en modo quantum" \
    "$BIN" --input "$FIXTURES/valid_5.csv" --mode quantum --quantum 10 \
    --seed 2026 --log /dev/null

echo "Los 4 escenarios del enunciado:"
expect_failure "4 tareas (menos de las 5 minimas)" \
    "$BIN" --input "$FIXTURES/invalid_too_few_tasks.csv" --mode quantum \
    --quantum 10 --seed 2026 --log /dev/null
expect_failure "tickets en cero" \
    "$BIN" --input "$FIXTURES/invalid_zero_tickets.csv" --mode quantum \
    --quantum 10 --seed 2026 --log /dev/null
expect_failure "id duplicado" \
    "$BIN" --input "$FIXTURES/invalid_duplicate_id.csv" --mode quantum \
    --quantum 10 --seed 2026 --log /dev/null
expect_failure "archivo incompleto (fila con columna faltante)" \
    "$BIN" --input "$FIXTURES/invalid_incomplete_row.csv" --mode quantum \
    --quantum 10 --seed 2026 --log /dev/null

echo "Sin ejecucion parcial:"

# Corre el binario con --log apuntando a un archivo temporal recien
# eliminado; si el comando falla como se espera, --log NUNCA debio
# crearse (la validacion de CSV/CLI ocurre antes de abrir cualquier
# archivo de salida).
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
expect_no_log_created "CSV invalido (tickets en cero) no crea --log" \
    "$BIN" --input "$FIXTURES/invalid_zero_tickets.csv" --mode quantum --quantum 10 --seed 2026
expect_no_log_created "CSV invalido (id duplicado) no crea --log" \
    "$BIN" --input "$FIXTURES/invalid_duplicate_id.csv" --mode quantum --quantum 10 --seed 2026
rm -f "$TMP_LOG"

# --summary invalido debe fallar ANTES de correr el scheduler: el log de
# eventos (que si tiene una ruta valida) debe quedar vacio, no con la
# corrida completa desperdiciada.
TMP_LOG2="$(mktemp)"
"$BIN" --input "$FIXTURES/valid_5.csv" --mode quantum --quantum 10 --seed 2026 \
    --log "$TMP_LOG2" --summary "/no/existe/de/verdad/summary.csv" >"$OUT" 2>&1
if [ -s "$TMP_LOG2" ]; then
    echo "  FALLO- --summary invalido: el log de eventos se escribio de todas formas (ejecucion desperdiciada)"
    failed=$((failed + 1))
else
    echo "  ok   - --summary invalido corta antes de escribir el log de eventos"
    passed=$((passed + 1))
fi
rm -f "$TMP_LOG2"

echo
echo "Resultado: $passed pasaron, $failed fallaron."
[ "$failed" -eq 0 ]
