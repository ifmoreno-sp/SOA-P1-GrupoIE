#!/usr/bin/env python3
"""Experimento A/B de la extension M9 (compensation tickets).

Implementa el diseno experimental de
SOA-P1-Knowledge/shared_knowledge/extension_compensation_tickets.md:
una tarea X, configurada para ceder temprano tras una fraccion fija de su
bloque en cada activacion que gana, se corre dos veces por semilla (mismo
CSV, mismo modo/parametros, misma ventana de --max-dispatches, mismo
--yield-config):

  - Corrida A (control):     con --disable-compensation
                              (cede, pero sus tickets nunca se inflan).
  - Corrida B (tratamiento): sin --disable-compensation
                              (cede Y se compensa).

Para cada semilla se calcula:
    error_A = |observed_share_X(A) - target_share_X|
    error_B = |observed_share_X(B) - target_share_X|
    mejora  = (error_A - error_B) / error_A

target_share_X es tickets_base_X / suma_de_tickets_base (la participacion
"justa" segun boletos, sin importar el modo de ejecucion).

Se repite sobre varias semillas y se reporta media + desviacion estandar
de "mejora", ademas del detalle por semilla. El script SOLO calcula y
reporta estas metricas -- la interpretacion final contra H0/H1 (incluido
el umbral de "mejora significativa") es parte del informe, no de esta
herramienta.

Requiere que el binario ya este compilado (`make all`).
"""

import argparse
import csv
import statistics
import subprocess
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent


def read_target_share(input_csv: Path, task_id: int) -> float:
    """Lee el CSV de tareas y devuelve tickets_base_X / suma_de_tickets."""
    with open(input_csv, newline="") as f:
        rows = list(csv.DictReader(f))
    total_tickets = sum(int(row["tickets"]) for row in rows)
    for row in rows:
        if int(row["id"]) == task_id:
            return int(row["tickets"]) / total_tickets
    raise ValueError(f"task_id {task_id} no existe en {input_csv}")


def write_yield_config(path: Path, task_id: int, yield_percent: int) -> None:
    with open(path, "w") as f:
        f.write("task_id,yield_percent\n")
        f.write(f"{task_id},{yield_percent}\n")


def run_scheduler(binary: Path, input_csv: Path, mode: str, quantum: int,
                   slice_percent: int, seed: int, max_dispatches: int,
                   yield_config: Path, disable_compensation: bool,
                   log_path: Path, summary_path: Path) -> None:
    args = [
        str(binary),
        "--input", str(input_csv),
        "--mode", mode,
        "--seed", str(seed),
        "--log", str(log_path),
        "--summary", str(summary_path),
        "--max-dispatches", str(max_dispatches),
        "--yield-config", str(yield_config),
    ]
    if mode == "quantum":
        args += ["--quantum", str(quantum)]
    else:
        args += ["--slice-percent", str(slice_percent)]
    if disable_compensation:
        args.append("--disable-compensation")

    result = subprocess.run(args, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(
            f"lottery_scheduler fallo (codigo {result.returncode}): "
            f"{result.stderr.strip()}\ncomando: {' '.join(args)}"
        )


def read_observed_share(summary_path: Path, task_id: int) -> float:
    with open(summary_path, newline="") as f:
        rows = list(csv.DictReader(f))
    for row in rows:
        if int(row["id"]) == task_id:
            return float(row["observed_share"])
    raise ValueError(f"task_id {task_id} no aparece en {summary_path}")


def run_one_seed(binary: Path, input_csv: Path, task_id: int, mode: str,
                  quantum: int, slice_percent: int, yield_percent: int,
                  max_dispatches: int, seed: int, target_share: float,
                  tmpdir: Path) -> dict:
    yield_cfg = tmpdir / f"yield_{seed}.csv"
    write_yield_config(yield_cfg, task_id, yield_percent)

    log_a = tmpdir / f"log_a_{seed}.csv"
    summary_a = tmpdir / f"summary_a_{seed}.csv"
    run_scheduler(binary, input_csv, mode, quantum, slice_percent, seed,
                  max_dispatches, yield_cfg, True, log_a, summary_a)
    observed_a = read_observed_share(summary_a, task_id)

    log_b = tmpdir / f"log_b_{seed}.csv"
    summary_b = tmpdir / f"summary_b_{seed}.csv"
    run_scheduler(binary, input_csv, mode, quantum, slice_percent, seed,
                  max_dispatches, yield_cfg, False, log_b, summary_b)
    observed_b = read_observed_share(summary_b, task_id)

    error_a = abs(observed_a - target_share)
    error_b = abs(observed_b - target_share)
    # error_a == 0 es posible en principio (el control termino exactamente
    # en el target por casualidad); "mejora" no esta definida ahi -- se
    # reporta como None en vez de dividir entre cero, y se excluye del
    # promedio final (documentado en el resumen impreso).
    mejora = (error_a - error_b) / error_a if error_a != 0 else None

    return {
        "seed": seed,
        "observed_a": observed_a,
        "observed_b": observed_b,
        "error_a": error_a,
        "error_b": error_b,
        "mejora": mejora,
    }


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Experimento A/B de compensation tickets (M9): "
                     "compara el error de observed_share de una tarea que "
                     "cede temprano, con y sin compensacion, sobre varias "
                     "semillas.")
    parser.add_argument("--binary", default=str(REPO_ROOT / "lottery_scheduler"))
    parser.add_argument("--input", default=str(REPO_ROOT / "tests/fixtures/valid_5.csv"),
                         help="CSV de tareas (default: tests/fixtures/valid_5.csv, "
                              "5 tareas con tickets 10/20/30/40/50)")
    parser.add_argument("--task-id", type=int, default=3,
                         help="id de la tarea que cede temprano (default: 3)")
    parser.add_argument("--yield-percent", type=int, default=25,
                         help="fraccion del bloque que corre antes de ceder, 1-99 (default: 25)")
    parser.add_argument("--mode", choices=["quantum", "cooperative"], default="quantum")
    parser.add_argument("--quantum", type=int, default=50)
    parser.add_argument("--slice-percent", type=int, default=10)
    parser.add_argument("--max-dispatches", type=int, default=40,
                         help="ventana de observacion truncada (default: 40; "
                              "40 << ~60 despachos totales esperados para que "
                              "la tarea de mas tickets termine, ver "
                              "milestone9_notas_tecnicas.md en el repo de Knowledge)")
    parser.add_argument("--seeds", type=int, default=100,
                         help="cuantas semillas correr, numeradas 1..N (default: 100; "
                              "calculado con n=(1.96*desv_std/margen)^2 a partir de un "
                              "piloto de 30 semillas para un margen de +/-10 puntos "
                              "porcentuales en la metrica de mejora)")
    parser.add_argument("--out", default=str(REPO_ROOT / "results/compensation_ab.csv"),
                         help="CSV con el detalle por semilla (default: results/compensation_ab.csv)")
    args = parser.parse_args()

    binary = Path(args.binary)
    input_csv = Path(args.input)
    if not binary.is_file():
        print(f"error: no se encontro el binario '{binary}' (corre 'make all' primero)",
              file=sys.stderr)
        return 1
    if not input_csv.is_file():
        print(f"error: no se encontro el CSV de entrada '{input_csv}'", file=sys.stderr)
        return 1

    target_share = read_target_share(input_csv, args.task_id)
    print(f"tarea {args.task_id}: target_share = {target_share:.4f} "
          f"(segun tickets base en {input_csv.name})")
    print(f"yield_percent = {args.yield_percent}%, modo = {args.mode}, "
          f"max-dispatches = {args.max_dispatches}, semillas = 1..{args.seeds}\n")

    rows = []
    with tempfile.TemporaryDirectory() as tmp:
        tmpdir = Path(tmp)
        for seed in range(1, args.seeds + 1):
            row = run_one_seed(binary, input_csv, args.task_id, args.mode,
                                args.quantum, args.slice_percent, args.yield_percent,
                                args.max_dispatches, seed, target_share, tmpdir)
            rows.append(row)
            mejora_str = f"{row['mejora']:+.3f}" if row["mejora"] is not None else "N/A"
            print(f"  seed {seed:3d}: observed_A={row['observed_a']:.4f} "
                  f"observed_B={row['observed_b']:.4f} "
                  f"error_A={row['error_a']:.4f} error_B={row['error_b']:.4f} "
                  f"mejora={mejora_str}")

    Path(args.out).parent.mkdir(parents=True, exist_ok=True)
    with open(args.out, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=["seed", "observed_a", "observed_b",
                                                "error_a", "error_b", "mejora"])
        writer.writeheader()
        writer.writerows(rows)
    print(f"\ndetalle por semilla escrito en {args.out}")

    mejoras = [r["mejora"] for r in rows if r["mejora"] is not None]
    excluded = len(rows) - len(mejoras)

    print(f"\nResumen sobre {len(rows)} semillas"
          f"{f' ({excluded} excluidas: error_A == 0)' if excluded else ''}:")
    print(f"  error_A (sin compensar):  media={statistics.mean(r['error_a'] for r in rows):.4f}  "
          f"desv_std={statistics.pstdev(r['error_a'] for r in rows):.4f}")
    print(f"  error_B (compensando):    media={statistics.mean(r['error_b'] for r in rows):.4f}  "
          f"desv_std={statistics.pstdev(r['error_b'] for r in rows):.4f}")
    if mejoras:
        print(f"  mejora relativa:          media={statistics.mean(mejoras):+.4f}  "
              f"desv_std={statistics.pstdev(mejoras):.4f}")
    else:
        print("  mejora relativa: no calculable (error_A == 0 en todas las semillas)")

    return 0


if __name__ == "__main__":
    sys.exit(main())
