#!/usr/bin/env bash
# Caso 5 del enunciado (Terminacion): "Trabajos distintos; tareas terminan
# en momentos diferentes. Una tarea finalizada no vuelve a ganar; suma de
# trabajo correcta; joins completos."
#
# La prueba en si vive en tests/test_scheduler.c (test_multiple_tasks_all_finish),
# junto a las demas pruebas del bucle del scheduler -- no se duplica aqui
# porque comparten el mismo protocolo y el mismo binario de prueba. Este
# script es solo el punto de entrada desde scripts/casos_enunciado/, para
# tener los 7 casos invocables desde un mismo lugar.
set -uo pipefail

echo "Caso 5 -- Terminacion"
echo "(prueba real en tests/test_scheduler.c: test_multiple_tasks_all_finish)"
echo
make test-scheduler
