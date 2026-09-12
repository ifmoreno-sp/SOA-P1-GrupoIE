#!/usr/bin/env python3
"""Caso 4 del enunciado (Proporcionalidad): "10 000 000 unidades por
tarea; boletos 10/20/30/40/50; 10 000 despachos; 30 semillas. Shares
objetivo 1/15, 2/15, 3/15, 4/15 y 5/15; error absoluto medio <= 0.02 por
tarea o analisis de la causa."

Mismo quantum=1000 y mismo criterio que Caso 3 (ver caso3_igualdad.py) para
que ninguna tarea llegue a terminar dentro de la ventana observada."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from proporcionalidad_lib import report, run_seeds

BINARY = "./lottery_scheduler"
CSV = os.path.join(os.path.dirname(os.path.abspath(__file__)), "data",
                    "caso4_proporcionalidad.csv")
SEEDS = range(1, 31)  # 30 semillas no nulas
MAX_DISPATCHES = 10000
TOLERANCE = 0.02


def main():
    print("Caso 4 -- Proporcionalidad\n")
    shares = run_seeds(BINARY, CSV, ["--mode", "quantum", "--quantum", "1000"],
                        SEEDS, MAX_DISPATCHES)
    total_tickets = 150  # 10+20+30+40+50
    tickets_by_task = {1: 10, 2: 20, 3: 30, 4: 40, 5: 50}
    objective = {task_id: tickets / total_tickets
                 for task_id, tickets in tickets_by_task.items()}
    ok = report(shares, objective, TOLERANCE, "Resultados por tarea (30 semillas):")
    print("\nResultado: Caso 4 " + ("CUMPLE" if ok else "NO CUMPLE"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
