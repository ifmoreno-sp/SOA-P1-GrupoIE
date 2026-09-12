# Casos mínimos de funcionamiento (Milestone 10)

Tabla de los 7 casos mínimos exigidos por el enunciado ("Pruebas Mínimas
de Funcionamiento"), con comando, resultado esperado, resultado obtenido y
conclusión — corresponde al punto 3 de "Evidencia y Análisis Requeridos".

Los 7 casos son invocables desde `scripts/casos_enunciado/`. Los Casos 2,
3, 4 y 7 tienen su prueba completa ahí (no estaban cubiertos por nada
existente). Los Casos 1, 5 y 6 ya estaban cubiertos por pruebas de
desarrollador de milestones anteriores (`tests/`) — en vez de duplicar esa
lógica, `scripts/casos_enunciado/` tiene un punto de entrada delgado para
cada uno (`caso1_validacion.sh`, `caso5_terminacion.sh`,
`caso6_modos.sh`) que solo invoca la prueba real.

Para reproducir todo: `make casos-enunciado` (Casos 2/3/4/7 con
ASan+UBSan) y `make casos-enunciado-tsan` (Caso 7 con ThreadSanitizer,
nativo — ver nota de entorno al final).

## Caso 1 — Validación

**Configuración del enunciado:** 4 tareas; tickets cero; id duplicado;
archivo incompleto.

**Comando:** `make test` (corre `scripts/casos_enunciado/caso1_validacion.sh`).

**Resultado esperado:** rechazo con código no cero y sin crear ejecución
parcial.

**Resultado obtenido:** 28/28 casos pasan — cubre tickets en cero, id
duplicado, archivo incompleto, columna adicional, valor faltante, valor no
numérico, menos de 5 tareas, overflow de boletos, archivo inexistente,
toda la validación de CLI, rutas inválidas de `--log`/`--summary`, y
ausencia explícita de archivos parciales tras un error (verificado en el
PR #25, Milestone 8).

**Conclusión:** cumple. Ningún caso de entrada inválida produce ejecución
parcial ni archivos de salida a medio escribir.

## Caso 2 — Reproducibilidad

**Configuración del enunciado:** 5 tareas, 1 000 unidades cada una,
boletos 10/20/30/40/50; quantum 10; seed 2026.

**Comando:** `bash scripts/casos_enunciado/caso2_reproducibilidad.sh`
(equivalente a correr dos veces: `./lottery_scheduler --input ... --mode
quantum --quantum 10 --seed 2026 --log <archivo>`).

**Resultado esperado:** dos ejecuciones producen logs idénticos y todas
las tareas terminan.

**Resultado obtenido:**
```
Caso 2 -- Reproducibilidad
  ok    - ambas ejecuciones producen el mismo log de eventos, byte a byte
  ok    - las 5 tareas llegan a completed_units == work_units
Resultado: Caso 2 CUMPLE
```

**Conclusión:** cumple. El log de eventos es idéntico byte a byte entre
corridas con la misma semilla, y las 5 tareas alcanzan `TASK_FINISHED`.

## Caso 3 — Igualdad

**Configuración del enunciado:** 5 tareas, 100 boletos y 10 000 000
unidades cada una; 10 000 despachos; 30 semillas.

**Comando:** `python3 scripts/casos_enunciado/caso3_igualdad.py`.

**Resultado esperado:** media de share cercana a 0.20; desviación y error
reportados.

**Resultado obtenido** (30 semillas, quantum=1000):
```
id    objetivo    media       desv.std    error abs.
1     0.200000    0.199303    0.003660    0.000697
2     0.200000    0.200730    0.004186    0.000730
3     0.200000    0.199740    0.003769    0.000260
4     0.200000    0.200463    0.003250    0.000463
5     0.200000    0.199763    0.003909    0.000237

Error absoluto medio entre tareas: 0.000477 (tolerancia: 0.02)
Resultado: Caso 3 CUMPLE
```

**Conclusión:** cumple, con margen amplio (error medio ~0.0005, muy por
debajo de la tolerancia de 0.02).

## Caso 4 — Proporcionalidad

**Configuración del enunciado:** 10 000 000 unidades por tarea; boletos
10/20/30/40/50; 10 000 despachos; 30 semillas.

**Comando:** `python3 scripts/casos_enunciado/caso4_proporcionalidad.py`.

**Resultado esperado:** shares objetivo 1/15, 2/15, 3/15, 4/15 y 5/15;
error absoluto medio ≤ 0.02 por tarea o análisis de la causa.

**Resultado obtenido** (30 semillas, quantum=1000):
```
id    objetivo    media       desv.std    error abs.
1     0.066667    0.066950    0.003043    0.000283
2     0.133333    0.132570    0.003962    0.000763
3     0.200000    0.201243    0.004087    0.001243
4     0.266667    0.267900    0.003743    0.001233
5     0.333333    0.331337    0.005054    0.001997

Error absoluto medio entre tareas: 0.001104 (tolerancia: 0.02)
Resultado: Caso 4 CUMPLE
```

**Conclusión:** cumple. Los shares observados convergen a los objetivos
1/15…5/15 (boletos más altos → share más alto), confirmando que el sorteo
pondera correctamente por boletos dentro de una ventana truncada donde
ninguna tarea termina su trabajo. Ver también
`milestone7_notas_tecnicas.md` (repo de conocimiento) sobre por qué esta
ponderación solo se observa con `--max-dispatches` y trabajo grande —
dejar correr hasta que todas terminen produce `observed_share` uniforme
(1/N) sin importar los boletos.

## Caso 5 — Terminación

**Configuración del enunciado:** trabajos distintos; tareas terminan en
momentos diferentes.

**Comando:** `bash scripts/casos_enunciado/caso5_terminacion.sh` (invoca
`make test-scheduler` → `tests/test_scheduler.c`,
`test_multiple_tasks_all_finish`).

**Resultado esperado:** una tarea finalizada no vuelve a ganar; suma de
trabajo correcta; joins completos.

**Resultado obtenido:** 35/35 checks en `test_scheduler.c`, incluyendo
explícitamente: *"ninguna tarea gana más de un despacho (una FINISHED no
vuelve a competir)"* y *"completed_units == work_units al terminar"* para
5 tareas con `work_units` distintos (3, 7, 1, 9, 4). El *join* de los 5
hilos se verifica con timeout vía `alarm()` (sin deadlock).

**Conclusión:** cumple.

## Caso 6 — Modos

**Configuración del enunciado:** misma entrada y semilla en cooperativo y
quantum discreto.

**Comando:** `bash scripts/casos_enunciado/caso6_modos.sh` (invoca
`make test-modes` → `tests/test_execution_modes.c`,
`test_modes_produce_same_result`).

**Resultado esperado:** mismo trabajo final y π; comparación de despachos
y costo de coordinación.

**Resultado obtenido:** 46/46 checks en `test_execution_modes.c`,
incluyendo la verificación directa `coop.pi_approx == quant.pi_approx`
(bit a bit) entre ambos modos con la misma entrada/semilla.

**Conclusión:** cumple.

## Caso 7 — Estrés

**Configuración del enunciado:** 25 tareas, parámetros válidos variados y
varias semillas.

**Comando:** `make casos-enunciado` (Caso 7 con ASan+UBSan) y
`make casos-enunciado-tsan` (con ThreadSanitizer) —
(`scripts/casos_enunciado/caso7_estres.sh`).

**Resultado esperado:** sin deadlock, fuga, acceso inválido, data race ni
comportamiento indefinido.

**Resultado obtenido:** 25 tareas con tickets/`work_units` variados
(`scripts/casos_enunciado/data/caso7_estres.csv`), 5 semillas en ambos
modos + el caso más exigente de quantum (`Q=1`, una unidad por
activación) + un caso con `--max-dispatches` (ejercita las filas
`STOPPED` bajo estrés) — 12/12 combinaciones, sin hallazgos, verificado en
tres variantes:
- ASan+UBSan nativo en macOS.
- ThreadSanitizer nativo en macOS.
- ASan+**LeakSanitizer** real dentro de Docker (Ubuntu 24.04) — LeakSanitizer
  no funciona en macOS (ver `entorno_desarrollo.md` del repo de
  conocimiento), así que esta es la única de las tres que certifica
  ausencia de fugas de memoria de forma confiable.

**Conclusión:** cumple en las tres variantes.

## Nota sobre el entorno de verificación

`casos-enunciado` (Caso 7 con ASan+UBSan) corre igual de bien nativo en
macOS o dentro de Docker. `casos-enunciado-tsan` debe correrse **nativo**,
no dentro del contenedor Docker en Apple Silicon: ThreadSanitizer falla
ahí por un problema conocido de la instrumentación bajo emulación
(Rosetta/QEMU), no relacionado con el código del proyecto. Para verificar
ausencia de fugas de memoria de forma confiable, `casos-enunciado` debe
correrse dentro de Docker (`LeakSanitizer` no funciona en macOS). Detalle
completo en `entorno_desarrollo.md` del repo de conocimiento.
