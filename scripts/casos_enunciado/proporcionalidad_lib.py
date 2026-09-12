#!/usr/bin/env python3
"""Helper compartido por los Casos 3 (Igualdad) y 4 (Proporcionalidad) del
enunciado: ambos corren el mismo binario una vez por semilla, con una
ventana de despachos truncada (--max-dispatches) sobre trabajo grande para
que ninguna tarea termine, y comparan el observed_share resultante contra
un share objetivo. Solo libreria estandar de Python -- sin dependencias
externas.
"""
import csv
import os
import statistics
import subprocess
import tempfile


def run_seeds(binary, input_csv, mode_args, seeds, max_dispatches):
    """Corre `binary` una vez por cada semilla en `seeds`, con `mode_args`
    (p. ej. ["--mode", "quantum", "--quantum", "1000"]) y --max-dispatches
    fijo. Retorna un dict {task_id: [observed_share por semilla, ...]}.

    Lanza RuntimeError si alguna corrida termina con codigo distinto de
    cero (no se espera ningun error aqui: la entrada ya es valida)."""
    shares_by_task = {}
    for seed in seeds:
        log_fd, log_path = tempfile.mkstemp(suffix=".csv")
        sum_fd, sum_path = tempfile.mkstemp(suffix=".csv")
        os.close(log_fd)
        os.close(sum_fd)
        try:
            cmd = [binary, "--input", input_csv, *mode_args,
                   "--seed", str(seed), "--max-dispatches", str(max_dispatches),
                   "--log", log_path, "--summary", sum_path]
            result = subprocess.run(cmd, capture_output=True, text=True)
            if result.returncode != 0:
                raise RuntimeError(
                    f"seed {seed} fallo (exit {result.returncode}): {result.stderr}")
            with open(sum_path, newline="") as f:
                reader = csv.DictReader(f)
                for row in reader:
                    task_id = int(row["id"])
                    share = float(row["observed_share"])
                    shares_by_task.setdefault(task_id, []).append(share)
        finally:
            os.unlink(log_path)
            os.unlink(sum_path)
    return shares_by_task


def report(shares_by_task, objective_by_task, error_tolerance, label):
    """Imprime media/desviacion estandar/error absoluto por tarea frente a
    su share objetivo. Retorna True si el error absoluto MEDIO entre todas
    las tareas queda dentro de error_tolerance (criterio del enunciado
    para el Caso 4: "error absoluto medio <= 0.02 por tarea")."""
    print(label)
    print(f"{'id':<6}{'objetivo':<12}{'media':<12}{'desv.std':<12}{'error abs.':<12}")
    errors = []
    for task_id in sorted(shares_by_task):
        samples = shares_by_task[task_id]
        mean = statistics.mean(samples)
        stdev = statistics.stdev(samples) if len(samples) > 1 else 0.0
        objective = objective_by_task[task_id]
        error = abs(mean - objective)
        errors.append(error)
        print(f"{task_id:<6}{objective:<12.6f}{mean:<12.6f}{stdev:<12.6f}{error:<12.6f}")

    mean_error = statistics.mean(errors)
    within = mean_error <= error_tolerance
    print(f"\nError absoluto medio entre tareas: {mean_error:.6f} "
          f"(tolerancia: {error_tolerance})")
    return within
