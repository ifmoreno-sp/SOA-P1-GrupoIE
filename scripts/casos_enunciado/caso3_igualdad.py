#!/usr/bin/env python3
"""Caso 3 del enunciado (Igualdad): "5 tareas, 100 boletos y 10 000 000
unidades cada una; 10 000 despachos; 30 semillas. Media de share cercana a
0.20; desviacion y error reportados."

El enunciado no fija un modo/parametro de ejecucion para este caso; se usa
quantum=1000 (arbitrario mientras sea mucho menor que work_units, para que
la ventana de 10 000 despachos combinados entre las 5 tareas nunca alcance
a completar el trabajo de ninguna)."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from proporcionalidad_lib import report, run_seeds

BINARY = "./lottery_scheduler"
CSV = os.path.join(os.path.dirname(os.path.abspath(__file__)), "data", "caso3_igualdad.csv")
SEEDS = range(1, 31)  # 30 semillas no nulas
MAX_DISPATCHES = 10000
TOLERANCE = 0.02


def main():
    print("Caso 3 -- Igualdad\n")
    shares = run_seeds(BINARY, CSV, ["--mode", "quantum", "--quantum", "1000"],
                        SEEDS, MAX_DISPATCHES)
    objective = {task_id: 0.20 for task_id in shares}
    ok = report(shares, objective, TOLERANCE, "Resultados por tarea (30 semillas):")
    print("\nResultado: Caso 3 " + ("CUMPLE" if ok else "NO CUMPLE"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
