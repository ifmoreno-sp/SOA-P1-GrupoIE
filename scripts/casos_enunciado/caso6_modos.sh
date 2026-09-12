#!/usr/bin/env bash
# Caso 6 del enunciado (Modos): "Misma entrada y semilla en cooperativo y
# quantum discreto. Mismo trabajo final y pi; comparacion de despachos y
# costo de coordinacion."
#
# La prueba minima vive en scripts/casos_enunciado/caso6_modos.c (extraida
# de tests/test_execution_modes.c, que se quedo con las otras ocho pruebas
# de ingenieria de M6 que no corresponden a este escenario). Mismo
# criterio que el Caso 5 (ver caso5_terminacion.sh). Este script es solo
# el punto de entrada desde scripts/casos_enunciado/.
set -uo pipefail

echo "(prueba real en scripts/casos_enunciado/caso6_modos.c)"
echo
make caso6-modos
