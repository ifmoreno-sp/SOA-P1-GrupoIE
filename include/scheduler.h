#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stddef.h>
#include <stdint.h>

#include "rng.h"
#include "sync.h"
#include "task.h"

/* Resultado de un despacho, para quien quiera observar/registrar el
 * progreso (consola en M5, log CSV en M7 — los nombres de campo coinciden
 * a proposito con las columnas del log que exige el enunciado). */
typedef struct {
    uint64_t dispatch;
    uint32_t winner_id;
    uint32_t winning_ticket;
    uint64_t active_tickets;
    uint32_t run_units;
    uint32_t completed_units;
    TaskState state_after;
} DispatchEvent;

/* Invocada por scheduler_run despues de cada despacho, si no es NULL.
 * `ctx` es el puntero que el llamador de scheduler_run paso en
 * observer_ctx, sin interpretar. scheduler.c no conoce CSV ni consola: es
 * responsabilidad exclusiva de quien implemente el observer. */
typedef void (*DispatchObserver)(const DispatchEvent *event, void *ctx);

/* Ejecuta el bucle principal del scheduler por loteria (M5) sobre `tasks`
 * (`task_count` tareas, ya inicializadas con task_init y en TASK_READY).
 * El hilo que llama a esta funcion actua como scheduler (no crea un hilo
 * dedicado para si mismo) — si crea un pthread por tarea internamente
 * (worker_pool_start) y los une a todos (worker_pool_join) antes de
 * retornar.
 *
 * En cada ronda: sync_select_winner decide la ganadora entre las tareas
 * TASK_READY (ponderado por boletos activos, con `rng`); sync_dispatch la
 * despacha; sync_wait_for_event espera su cesion o finalizacion. El bucle
 * termina cuando no queda ninguna tarea TASK_READY (todas TASK_FINISHED).
 *
 * Si `max_dispatches` > 0, el bucle se detiene tras exactamente esa
 * cantidad de despachos aunque queden tareas TASK_READY (modo de
 * observacion limitada, --max-dispatches): pide a los workers restantes
 * que se detengan (sync_request_stop) y los une igual, sin alterar el
 * estado de las tareas que quedaron sin terminar. 0 significa sin limite.
 *
 * Si `observer` no es NULL, se invoca con los datos de cada despacho
 * (incluido el ultimo si --max-dispatches corto la observacion).
 *
 * Precondiciones: sync ya inicializado (sync_init); rng ya inicializado
 * (rng_init); todas las tareas en tasks[] estan en TASK_READY; task_count
 * esta entre 1 y CSV_PARSER_MAX_TASKS.
 *
 * Postcondicion: al retornar, toda tarea quedo en TASK_FINISHED (si no
 * hubo limite, o si el limite no se alcanzo antes de que todas
 * terminaran) o en TASK_READY (las que quedaron pendientes si
 * max_dispatches corto la observacion). Ningun pthread creado por esta
 * llamada sigue vivo ni bloqueado.
 *
 * Retorna la cantidad total de despachos realizados. */
uint64_t scheduler_run(Sync *sync, Task *tasks, size_t task_count, Rng *rng,
                        uint64_t max_dispatches, DispatchObserver observer,
                        void *observer_ctx);

#endif /* SCHEDULER_H */
