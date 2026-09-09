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

## Compilación y pruebas

```bash
make all    # compila lottery_scheduler
make test   # corre todas las suites de pruebas
make asan   # make test con AddressSanitizer + UndefinedBehaviorSanitizer
make tsan   # make test con ThreadSanitizer
make clean  # limpia binarios y objetos, incluidos los de asan/tsan
```

`asan` y `tsan` no se combinan en una sola ejecución (son instrumentaciones
incompatibles entre sí): cada uno reconstruye y corre la suite completa por
separado, en su propio directorio de build.

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

### Quién puede leer el estado de una tarea sin el mutex (`sync_select_winner`)

El bucle del scheduler (M5, [`src/scheduler.c`](src/scheduler.c)) necesita
recorrer todas las tareas en cada ronda para sumar los boletos de las que
están `TASK_READY` y localizar a la ganadora del sorteo. Durante el
Milestone 4, esa misma necesidad se resolvió provisionalmente con un
recorrido *sin* sostener el mutex en el "scheduler falso" de las pruebas de
integración — seguro en la práctica (ninguna tarea está `RUNNING` en ese
punto del protocolo), pero inconsistente con la regla que el propio diseño
de M4 estableció: *"ningún código nuevo debe leer los campos de una `Task`
sin sostener `sync->mutex`, salvo el propio hilo trabajador"*.

**Decisión: agregar `sync_select_winner` a `sync.c`, que hace el sorteo
completo (sumar boletos, sortear con el RNG, localizar ganadora) bajo el
mutex, en una sola función.** El scheduler ya no recorre `tasks[]`
directamente — solo llama a esta función y usa su resultado. Esto mantiene
la regla sin excepciones, en vez de documentar el recorrido sin mutex como
un caso especial aceptado.

Por qué no la segunda opción (documentar la excepción): la razón por la que
es seguro hoy ("nunca se lee mientras hay una tarea `RUNNING`") es una
invariante *implícita* del orden de llamadas del bucle, no algo que el
compilador o un sanitizer puedan verificar. Dejarla como excepción
documentada habría significado que cualquier código futuro (la extensión de
M9, o un log de progreso en vivo) que necesitara leer el estado de una
tarea tendría que redescubrir y respetar esa misma regla no escrita.
Centralizarla en una función de `sync.c` la convierte en parte del
protocolo verificable, no en una convención que hay que recordar.

