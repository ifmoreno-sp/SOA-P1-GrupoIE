# SOA-P1-GrupoIE
Planificador por lotería en espacio de usuario (C17, POSIX pthreads). Proyecto 1 — MC 6004 Sistemas Operativos Avanzados, TEC, II semestre 2026.

Implementa la política de *proportional-share scheduling* descrita por
Waldspurger y Weihl: en cada despacho se sortea un boleto entre las tareas
READY y su propietaria recibe el siguiente intervalo de ejecución. No se
modifica el kernel ni se sustituye el scheduler de GNU/Linux.

## Integrantes

| Nombre | Carné | Usuario de GitHub |
|---|---|---|
| Emmanuel Barrantes Vargas | 200012366 | `ebarrantes07` |
| Isaac Moreno Fuentes | 2018119181 | `ifmoreno-sp` |

## Entorno

**Entorno de referencia: GNU/Linux x86_64, GCC, `make`**, vía Docker
(`Dockerfile` en la raíz, imagen basada en `ubuntu:24.04` forzada a
`--platform=linux/amd64`):

```bash
docker build -t soa-p1 .
docker run --rm -it -v "$(pwd)":/app soa-p1
# dentro del contenedor:
make all
make test
```

Verificado con GCC 13.3.0 / GNU Make 4.3 dentro del contenedor. También
compila y corre igual fuera de Docker (macOS/Linux nativo) para el ciclo
rápido de desarrollo, con dos salvedades conocidas de sanitizers en Apple
Silicon: **LeakSanitizer** solo reporta fugas de forma confiable dentro de
Docker, y **ThreadSanitizer** debe correr fuera de Docker (falla bajo la
emulación x86_64 del contenedor en hosts ARM64). Ver `make asan`/`make tsan`
más abajo.

## Compilación y pruebas

```bash
make all    # compila lottery_scheduler
make test   # corre todas las suites de pruebas (unitarias + validacion de CLI/CSV)
make asan   # make test con AddressSanitizer + UndefinedBehaviorSanitizer
make tsan   # make test con ThreadSanitizer
make clean  # limpia binarios y objetos, incluidos los de asan/tsan
```

`asan` y `tsan` no se combinan en una sola ejecución (son instrumentaciones
incompatibles entre sí): cada uno reconstruye y corre la suite completa por
separado, en su propio directorio de build.

`make casos-enunciado` (+ `make casos-enunciado-tsan`) corre los 7 casos
mínimos de la sección "Pruebas Mínimas de Funcionamiento" del enunciado —
ver `scripts/casos_enunciado/`.

## Ejecución

```bash
./lottery_scheduler --input tests/fixtures/valid_5.csv --mode quantum \
    --quantum 10 --seed 2026 --log results/eventos.csv --summary results/resumen.csv
```

Imprime en pantalla una tabla resumen por tarea y escribe dos archivos:

- **`--log`** (obligatorio): un evento por despacho —
  `dispatch,winner_id,winning_ticket,active_tickets,run_units,completed_units,state_after`.
  Una fila con `winning_ticket = 0` y `active_tickets = 0` marca `STOPPED`
  sintético (tarea que quedó `READY` cuando `--max-dispatches` cortó la
  observación, sin haber sido despachada de nuevo).
- **`--summary`** (opcional): una fila por tarea —
  `id,tickets,work_units,completed_units,dispatches_won,first_dispatch,last_dispatch,pi_final,observed_share`.

## Formato de entrada

**`--input` (obligatorio):** CSV con encabezado exacto `id,tickets,work_units`,
entre 5 y 25 filas. `id` entero único; `tickets` y `work_units` enteros
positivos representables en `uint32_t`. La suma de `tickets` de todas las
tareas se valida en `[1, UINT32_MAX]` (detectada sin overflow, acumulando en
`uint64_t`). Cualquier violación produce un mensaje de error, código de
salida distinto de cero, y ningún archivo de salida a medio escribir.

```csv
id,tickets,work_units
1,10,1000
2,20,1000
3,30,1000
4,40,1000
5,50,1000
```

**`--yield-config` (opcional):** CSV con encabezado exacto
`task_id,yield_percent`, una fila por tarea que debe ceder temprano de forma
forzada (extensión de compensation tickets — ver el informe para la
motivación completa). `task_id` debe existir en el CSV de `--input`,
sin duplicados dentro del archivo; `yield_percent` entero entre 1 y 99.

```csv
task_id,yield_percent
1,40
3,25
```

## Opciones

| Opción | Obligatoria | Descripción |
|---|---|---|
| `--input <csv>` | sí | Archivo de tareas (formato arriba). |
| `--mode <cooperative\|quantum>` | sí | Política de corte del bloque de ejecución. |
| `--quantum <Q>` | solo en modo `quantum` | Tamaño fijo del bloque, entero positivo. |
| `--slice-percent <P>` | solo en modo `cooperative` | Porcentaje (1–100) del trabajo restante por bloque. |
| `--seed <n>` | sí | Semilla del RNG, entero positivo distinto de cero. |
| `--log <csv>` | sí | Ruta de salida del log de eventos. |
| `--summary <csv>` | no | Ruta de salida del resumen por tarea. |
| `--max-dispatches <N>` | no | Corta la observación tras N despachos, sin alterar el conjunto de tareas activas. |
| `--yield-config <csv>` | no | Activa cesión temprana forzada para tareas puntuales (formato arriba). |
| `--disable-compensation` | no | Con `--yield-config`: cede temprano pero nunca infla `tickets` — control del experimento A/B de la extensión. Requiere `--yield-config`. |

`--quantum` y `--slice-percent` son mutuamente excluyentes según el modo
(el que no aplica al modo elegido produce error si se pasa).

## Ejemplos de comandos

```bash
# Modo quantum, sin limite de despachos (corre hasta que todas las tareas terminen)
./lottery_scheduler --input tests/fixtures/valid_5.csv --mode quantum \
    --quantum 10 --seed 2026 --log results/log.csv

# Modo cooperative, con resumen y limite de despachos
./lottery_scheduler --input tests/fixtures/valid_5.csv --mode cooperative \
    --slice-percent 15 --seed 2026 --log results/log.csv \
    --summary results/resumen.csv --max-dispatches 200

# Extension: cesion temprana forzada + compensacion activa (tratamiento)
./lottery_scheduler --input tests/fixtures/valid_5.csv --mode quantum \
    --quantum 10 --seed 2026 --log results/log.csv \
    --yield-config tests/fixtures/yield_valid.csv

# Extension: cesion temprana forzada SIN compensacion (control del experimento A/B)
./lottery_scheduler --input tests/fixtures/valid_5.csv --mode quantum \
    --quantum 10 --seed 2026 --log results/log.csv \
    --yield-config tests/fixtures/yield_valid.csv --disable-compensation
```

Ver `scripts/experiment_compensation_ab.py` para el experimento A/B completo
de la extensión (múltiples semillas, cálculo de error respecto al share
objetivo).

```bash
# Experimento de proporcionalidad (Casos 3 y 4, 30 semillas): barrido
# completo + graficas de convergencia y objetivo vs. observado
python3 scripts/experimento_proporcionalidad.py
```

## Estructura del repositorio

```
README.md          Makefile            Dockerfile        .gitignore
src/                # implementacion (.c)
include/            # headers publicos (.h)
tests/              # pruebas unitarias (.c) y de CLI/CSV (.sh), con tests/fixtures/
scripts/
  casos_enunciado/  # los 7 casos minimos del enunciado (make casos-enunciado)
  experiment_compensation_ab.py   # experimento A/B de la extension (M9)
  experimento_proporcionalidad.py # experimento estadistico Casos 3/4 (M11)
results/            # salidas de --log/--summary; no versionado salvo
                     # .gitkeep y results/proporcionalidad/ (graficas y CSVs
                     # seleccionados del experimento de M11)
docs/               # informe.pdf y material de entrega
```

## Entrega

Repositorio: <https://github.com/ifmoreno-sp/SOA-P1-GrupoIE>. La versión
entregada queda marcada con la etiqueta anotada `p1-entrega` sobre el commit
final — ver [Tags](https://github.com/ifmoreno-sp/SOA-P1-GrupoIE/tags) del
repositorio.

## Trazabilidad del equipo

Detalle de PRs y revisiones por *milestone* en el
[historial de commits](https://github.com/ifmoreno-sp/SOA-P1-GrupoIE/commits/main)
y en [Issues](https://github.com/ifmoreno-sp/SOA-P1-GrupoIE/issues)/[Pull
Requests](https://github.com/ifmoreno-sp/SOA-P1-GrupoIE/pulls) cerrados.

| Milestone | Responsabilidad | Responsable(s) | Issue | PR (autor) | Revisor principal |
|---|---|---|---|---|---|
| M0 | Setup del proyecto y gestión (repo, Issues, flujo de Git) | Isaac | #2 | #15 | Emmanuel |
| M1 | Modelo de datos (`Task`), parser de CSV y CLI | Emmanuel | #3 | #16 | Isaac |
| M2 | RNG determinista (`xorshift32`, sorteo de boletos) | Isaac | #4 | #17 | Emmanuel |
| M3 | Carga de trabajo simulada (serie de Leibniz para π) | Emmanuel | #5 | #18 | Isaac |
| M4 | Núcleo de concurrencia: protocolo mutex/condvars | Isaac y Emmanuel (compartido, ver nota) | #6 | #19 | Emmanuel |
| M5 | Scheduler por lotería (bucle de despacho real) | Isaac | #7 | #21 | Emmanuel |
| M6 | Modos de ejecución (cooperativo y quantum) | Emmanuel | #8 | #23 | Isaac |
| M7 | Registro de eventos y resumen (`--log`/`--summary`) | Isaac | #9 | #24 | Emmanuel |
| M8 | Validación de CLI/CSV y manejo de errores sin ejecución parcial | Isaac | #10 | #25 | Emmanuel |
| M9 | Extensión: *compensation tickets* (cesión y compensación) | Emmanuel | #11 | #27 | Isaac |
| M10 | Los 7 casos mínimos del enunciado + sanitizers | Isaac | #12 | #26 | Emmanuel |
| M11 | Experimento estadístico de proporcionalidad (Casos 3/4) | Emmanuel | #13 | #29 | Isaac |
| M12 | Documentación, informe y entrega | Isaac y Emmanuel (compartido, ver nota) | #14 | #28 | — (en progreso) |

El Milestone 4 (núcleo de concurrencia, PR #19) fue codesarrollado sobre una
sola rama compartida, dividido por archivo: Isaac implementó `task.c`/`sync.c`
(el protocolo mutex/condvars), Emmanuel implementó `worker.c` (el ciclo del
hilo trabajador sobre esa interfaz), y `tests/test_concurrency.c` se escribió
en conjunto — detalle completo en la descripción del PR.

El Milestone 12 (documentación y entrega, este mismo) también es compartido,
pero no dividido por milestone: Isaac redactó un primer borrador completo
del informe (`docs/informe/`), incluidas las secciones sobre el trabajo del
otro integrante. Sobre ese borrador, cada quien revisa y completa la
sección que describe el milestone que implementó (por ejemplo, Emmanuel
verificó la Sección de Extensión y completó los resultados estadísticos del
experimento A/B que quedaban pendientes en la Sección de Resultados). Ambos
verifican independientemente la reconstrucción desde una copia limpia antes
de cortar la etiqueta `p1-entrega`.
