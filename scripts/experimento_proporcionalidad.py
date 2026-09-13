#!/usr/bin/env python3
"""M11 (Issue #13): experimento estadistico sobre los Casos 3 y 4 del
enunciado ("Experimento de Proporcionalidad"). Reutiliza el barrido de 30
semillas que caso3_igualdad.py/caso4_proporcionalidad.py (M10) ya hacen con
los parametros exactos del enunciado (5 tareas, 10 000 000 unidades, 10 000
despachos), y agrega lo que pide el Issue #13 encima de eso:

  - Grafica de convergencia del share acumulado (una semilla fija, por
    caso): visualiza como el share observado se acerca al objetivo a
    medida que avanzan los despachos (ver run_single_seed_log en
    proporcionalidad_lib.py).
  - Grafica de comparacion objetivo vs. observado (barras, con la
    desviacion estandar entre las 30 semillas como barra de error).
  - CSV con el share por tarea y por semilla de cada caso, para poder
    reproducir/auditar los numeros sin volver a correr el binario.

Todo se genera en results/proporcionalidad/ (gitignorado como el resto de
results/: es reproducible corriendo este script, no hace falta versionarlo).

Requiere matplotlib (dependencia solo de este script de analisis, no del
binario ni de las pruebas automatizadas)."""
import csv
import os
import statistics
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "casos_enunciado"))
from proporcionalidad_lib import report, run_seeds, run_single_seed_log  # noqa: E402

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
CASOS_DIR = os.path.join(SCRIPT_DIR, "casos_enunciado")
OUT_DIR = os.path.join(SCRIPT_DIR, "..", "results", "proporcionalidad")

BINARY = "./lottery_scheduler"
MODE_ARGS = ["--mode", "quantum", "--quantum", "1000"]
SEEDS = range(1, 31)  # 30 semillas no nulas, igual que M10
CONVERGENCE_SEED = 1  # primera semilla del barrido: misma corrida, mas detalle
MAX_DISPATCHES = 10000
TOLERANCE = 0.02

CASOS = {
    "caso3": {
        "csv": os.path.join(CASOS_DIR, "data", "caso3_igualdad.csv"),
        "objective": {tid: 0.20 for tid in range(1, 6)},
        "label": "Caso 3 -- Igualdad",
    },
    "caso4": {
        "csv": os.path.join(CASOS_DIR, "data", "caso4_proporcionalidad.csv"),
        "objective": {1: 10 / 150, 2: 20 / 150, 3: 30 / 150, 4: 40 / 150, 5: 50 / 150},
        "label": "Caso 4 -- Proporcionalidad",
    },
}


def write_shares_csv(shares_by_task, seeds, out_path):
    """Una fila por semilla con el share final de cada tarea en esa
    corrida: los mismos numeros que resume `report`, pero sin promediar,
    para poder auditar/reproducir el calculo de media y desviacion."""
    task_ids = sorted(shares_by_task)
    with open(out_path, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["seed"] + [f"task_{tid}" for tid in task_ids])
        for i, seed in enumerate(seeds):
            writer.writerow([seed] + [shares_by_task[tid][i] for tid in task_ids])


def plot_convergence(dispatches, shares_by_task, objective_by_task, title, out_path):
    plt.figure(figsize=(8, 5))
    for task_id in sorted(shares_by_task):
        line, = plt.plot(dispatches, shares_by_task[task_id], label=f"Tarea {task_id}")
        plt.axhline(objective_by_task[task_id], color=line.get_color(),
                     linestyle="--", linewidth=0.8, alpha=0.5)
    plt.xlabel("Numero de despacho")
    plt.ylabel("Share acumulado")
    plt.title(title)
    plt.legend()
    plt.tight_layout()
    plt.savefig(out_path, dpi=120)
    plt.close()


def plot_objetivo_vs_observado(shares_by_task, objective_by_task, title, out_path):
    task_ids = sorted(shares_by_task)
    objetivos = [objective_by_task[tid] for tid in task_ids]
    medias = [statistics.mean(shares_by_task[tid]) for tid in task_ids]
    errores = [statistics.stdev(shares_by_task[tid]) if len(shares_by_task[tid]) > 1 else 0.0
               for tid in task_ids]

    x = list(range(len(task_ids)))
    width = 0.35
    plt.figure(figsize=(7, 5))
    plt.bar([i - width / 2 for i in x], objetivos, width, label="Objetivo")
    plt.bar([i + width / 2 for i in x], medias, width, yerr=errores, capsize=4,
             label="Observado (media +/- desv.std, 30 semillas)")
    plt.xticks(x, [f"Tarea {tid}" for tid in task_ids])
    plt.ylabel("Share")
    plt.title(title)
    plt.legend()
    plt.tight_layout()
    plt.savefig(out_path, dpi=120)
    plt.close()


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    resultados = {}

    for key, cfg in CASOS.items():
        print(f"\n=== {cfg['label']} ===\n")
        shares = run_seeds(BINARY, cfg["csv"], MODE_ARGS, SEEDS, MAX_DISPATCHES)
        ok = report(shares, cfg["objective"], TOLERANCE, "Resultados por tarea (30 semillas):")
        resultados[key] = ok

        write_shares_csv(shares, SEEDS, os.path.join(OUT_DIR, f"{key}_shares.csv"))

        dispatches, conv_shares = run_single_seed_log(
            BINARY, cfg["csv"], MODE_ARGS, CONVERGENCE_SEED, MAX_DISPATCHES)

        conv_path = os.path.join(OUT_DIR, f"{key}_convergencia.png")
        plot_convergence(dispatches, conv_shares, cfg["objective"],
                          f"{cfg['label']} -- convergencia (semilla {CONVERGENCE_SEED})",
                          conv_path)

        cmp_path = os.path.join(OUT_DIR, f"{key}_objetivo_vs_observado.png")
        plot_objetivo_vs_observado(shares, cfg["objective"],
                                    f"{cfg['label']} -- objetivo vs. observado (30 semillas)",
                                    cmp_path)

        print(f"\nArchivos generados: {os.path.abspath(conv_path)}, "
              f"{os.path.abspath(cmp_path)}, "
              f"{os.path.abspath(os.path.join(OUT_DIR, f'{key}_shares.csv'))}")

    print("\n=== Resumen ===")
    for key, ok in resultados.items():
        estado = "dentro de tolerancia" if ok else "fuera de tolerancia (ver analisis en Knowledge)"
        print(f"{CASOS[key]['label']}: {estado}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
