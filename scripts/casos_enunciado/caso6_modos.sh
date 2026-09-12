#!/usr/bin/env bash
# Caso 6 del enunciado (Modos): "Misma entrada y semilla en cooperativo y
# quantum discreto. Mismo trabajo final y pi; comparacion de despachos y
# costo de coordinacion."
#
# La prueba en si vive en tests/test_execution_modes.c
# (test_modes_produce_same_result), junto a las demas pruebas de los modos
# de ejecucion -- no se duplica aqui por la misma razon que el Caso 5 (ver
# caso5_terminacion.sh). Este script es solo el punto de entrada desde
# scripts/casos_enunciado/.
set -uo pipefail

echo "Caso 6 -- Modos"
echo "(prueba real en tests/test_execution_modes.c: test_modes_produce_same_result)"
echo
make test-modes
