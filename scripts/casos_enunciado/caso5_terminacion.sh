#!/usr/bin/env bash
# Caso 5 del enunciado (Terminacion): "Trabajos distintos; tareas terminan
# en momentos diferentes. Una tarea finalizada no vuelve a ganar; suma de
# trabajo correcta; joins completos."
#
# La prueba minima vive en scripts/casos_enunciado/caso5_terminacion.c
# (extraida de tests/test_scheduler.c, que se quedo con las otras dos
# pruebas de ingenieria de M5 que no corresponden a este escenario). Este
# script es solo el punto de entrada desde scripts/casos_enunciado/, para
# tener los 7 casos invocables desde un mismo lugar.
set -uo pipefail

echo "(prueba real en scripts/casos_enunciado/caso5_terminacion.c)"
echo
make caso5-terminacion
