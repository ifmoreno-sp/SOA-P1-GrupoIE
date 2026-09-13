#!/usr/bin/env python3
"""Genera los datos de las graficas del experimento de Proporcionalidad
(Caso 4 del enunciado) para el informe: (1) objetivo vs. promedio observado
tras 30 semillas (reusa proporcionalidad_lib, mismos parametros exactos que
scripts/casos_enunciado/caso4_proporcionalidad.py), y (2) convergencia del
share acumulado sobre una semilla representativa, calculada directamente
del log de eventos real.

Requiere el binario ya compilado (./lottery_scheduler en la raiz del repo).
Uso: python3 docs/informe/graficas/generar_datos_proporcionalidad.py
"""
import csv
import os
import subprocess
import sys
import tempfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "scripts", "casos_enunciado"))
from proporcionalidad_lib import run_seeds  # noqa: E402

BINARY = os.path.join(ROOT, "lottery_scheduler")
CSV = os.path.join(ROOT, "scripts", "casos_enunciado", "data", "caso4_proporcionalidad.csv")
SEEDS = range(1, 31)
MAX_DISPATCHES = 10000
OUT_DIR = os.path.dirname(os.path.abspath(__file__))

TICKETS = {1: 10, 2: 20, 3: 30, 4: 40, 5: 50}
TOTAL_TICKETS = sum(TICKETS.values())


def write_objetivo_vs_observado():
    shares = run_seeds(BINARY, CSV, ["--mode", "quantum", "--quantum", "1000"],
                        SEEDS, MAX_DISPATCHES)
    out_path = os.path.join(OUT_DIR, "objetivo_vs_observado.dat")
    with open(out_path, "w") as f:
        f.write("id objetivo observado\n")
        for task_id in sorted(shares):
            objetivo = TICKETS[task_id] / TOTAL_TICKETS
            observado = sum(shares[task_id]) / len(shares[task_id])
            f.write(f"{task_id} {objetivo:.6f} {observado:.6f}\n")
    print(f"escrito: {out_path}")


def write_convergencia(seed=1, sample_every=100):
    log_fd, log_path = tempfile.mkstemp(suffix=".csv")
    os.close(log_fd)
    try:
        cmd = [BINARY, "--input", CSV, "--mode", "quantum", "--quantum", "1000",
               "--seed", str(seed), "--max-dispatches", str(MAX_DISPATCHES),
               "--log", log_path]
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode != 0:
            raise RuntimeError(f"seed {seed} fallo: {result.stderr}")

        with open(log_path, newline="") as f:
            rows = list(csv.DictReader(f))

        task_ids = sorted(TICKETS)
        cum_run = {t: 0 for t in task_ids}
        total_run = 0
        out_path = os.path.join(OUT_DIR, "convergencia_caso4.dat")
        with open(out_path, "w") as out:
            out.write("dispatch " + " ".join(f"t{t}" for t in task_ids) + "\n")
            for i, row in enumerate(rows, start=1):
                cum_run[int(row["winner_id"])] += int(row["run_units"])
                total_run += int(row["run_units"])
                if i % sample_every == 0:
                    shares = [cum_run[t] / total_run if total_run else 0.0 for t in task_ids]
                    out.write(f"{i} " + " ".join(f"{s:.6f}" for s in shares) + "\n")
        print(f"escrito: {out_path} (semilla {seed})")
    finally:
        os.unlink(log_path)


if __name__ == "__main__":
    write_objetivo_vs_observado()
    write_convergencia()
