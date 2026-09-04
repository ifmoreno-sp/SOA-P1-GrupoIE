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

