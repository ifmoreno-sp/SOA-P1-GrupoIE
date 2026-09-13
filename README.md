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

## Decisiones de diseño

### Sesgo de módulo en el sorteo de boletos (`rng_draw_ticket`)

El boleto ganador se calcula como `(rng_next(rng) % active_tickets) + 1`
(ver [`src/rng.c`](src/rng.c)). La operación módulo introduce un sesgo leve
cuando `active_tickets` no divide exacto el rango del generador (`uint32_t`,
hasta 2³²−1): los primeros `r = (2³²−1) mod active_tickets` valores de
boleto ocurren una vez más que el resto a lo largo del ciclo completo del
generador.

**Decisión: se acepta el sesgo y se documenta, sin corregirlo con
rejection sampling.**

Por qué:
- `active_tickets` es órdenes de magnitud menor que el rango del generador.
  Por ejemplo, con `active_tickets = 150` (boletos 10/20/30/40/50), el sesgo
  relativo entre el boleto más favorecido y el resto es de
  `≈ 3.5 × 10⁻⁸` — siete órdenes de magnitud por debajo del error absoluto
  que el propio enunciado tolera en el experimento de proporcionalidad
  (`≤ 0.02`). No es una fuente plausible de desviación en los resultados.
- Corregirlo con rejection sampling (resamplear cuando el valor cae en el
  rango que produciría sesgo) agrega una rama de reintento al RNG —
  superficie extra de bugs por un beneficio indetectable en este caso de
  uso.
