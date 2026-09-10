#ifndef SYNC_H
#define SYNC_H

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

#include "rng.h"
#include "task.h"

typedef struct {
    pthread_mutex_t mutex;         /* protege: task[i].state de todas las
                                     * tareas, dispatch_count, event_ready,
                                     * stop_requested. NO protege la
                                     * ejecucion de workload_run_units. */
    pthread_cond_t cond_scheduler; /* el scheduler espera aqui un evento */
    int event_ready;               /* predicado de cond_scheduler */
    int stop_requested;            /* ver sync_request_stop */
} Sync;

/* Resultado de un sorteo de loteria (ver sync_select_winner). */
typedef struct {
    size_t index;                    /* indice del ganador en tasks[], o
                                       * task_count si ninguna tarea esta
                                       * TASK_READY (nada que despachar). */
    uint32_t winning_ticket;         /* boleto sorteado; solo valido si
                                       * index < task_count. */
    uint64_t active_tickets;         /* suma de boletos de las TASK_READY
                                       * en el momento del sorteo; solo
                                       * valido si index < task_count. */
    uint32_t completed_units_before; /* tasks[index].completed_units en el
                                       * momento del sorteo; solo valido si
                                       * index < task_count. Util para que
                                       * el llamador calcule run_units como
                                       * completed_units_after - este valor,
                                       * una vez que sync_wait_for_event
                                       * retorne. */
} Selection;

/* Inicializa mutex, cond_scheduler, event_ready = 0 y stop_requested = 0.
 * Retorna 0 en exito, -1 si pthread_mutex_init o pthread_cond_init fallan
 * (recursos del sistema agotados). */
int sync_init(Sync *sync);

/* Precondicion: nadie esta bloqueado en sync (todos los hilos ya se unieron
 * con pthread_join). Destruye mutex y cond_scheduler.
 * No destruye los cond_worker de cada Task — eso lo hace task_destroy. */
void sync_destroy(Sync *sync);

/* Llamada por el scheduler (el test de integracion, o el bucle real de
 * main()) para despachar a la tarea ganadora.
 *
 * Precondicion: winner_index < task_count; tasks[winner_index].state == TASK_READY.
 *
 * Efecto (con sync->mutex tomado):
 *   1. assert: ninguna tasks[i].state == TASK_RUNNING (invariante de
 *      exclusion — verificacion en runtime, no solo documentacion).
 *   2. tasks[winner_index].state = TASK_RUNNING.
 *   3. tasks[winner_index].dispatch_count++.
 *   4. pthread_cond_signal(&tasks[winner_index].cond_worker) — senal
 *      dirigida, nunca pthread_cond_broadcast.
 *
 * Sincronizacion: toma y suelta sync->mutex internamente; el llamador no
 * debe tener el mutex tomado al invocarla. */
void sync_dispatch(Sync *sync, Task *tasks, size_t task_count, size_t winner_index);

/* Decide la siguiente ganadora de la loteria: suma los boletos de las
 * tareas TASK_READY, sortea un boleto en [1, active_tickets] con `rng`, y
 * localiza a su propietaria por suma acumulada. Es la unica funcion que
 * recorre tasks[] para decidir un ganador, y lo hace bajo sync->mutex, en
 * vez de que el llamador (scheduler.c) lea task.state directamente.
 *
 * Precondicion: rng inicializado (rng_init); ninguna tarea esta en
 * TASK_RUNNING (el llamador ya proceso el evento anterior via
 * sync_wait_for_event, o es la primera llamada del bucle). El llamador no
 * debe tener sync->mutex tomado al invocarla.
 *
 * Efecto: toma y suelta sync->mutex internamente. No modifica el estado de
 * ninguna tarea (eso lo hace sync_dispatch, en una llamada aparte) ni
 * avisa a ningun worker.
 *
 * Si ninguna tarea esta en TASK_READY, retorna una Selection con
 * index == task_count (sentinela: nada que despachar, el bucle del
 * scheduler debe terminar) y los demas campos sin significado. */
Selection sync_select_winner(Sync *sync, Task *tasks, size_t task_count, Rng *rng);

/* Llamada por el scheduler justo despues de sync_dispatch, para esperar a
 * que la tarea en RUNNING ceda o termine.
 *
 * Efecto: toma sync->mutex; while (!sync->event_ready)
 * pthread_cond_wait(&sync->cond_scheduler, &sync->mutex); pone
 * event_ready = 0; suelta sync->mutex.
 *
 * Postcondicion: al retornar, la tarea despachada ya actualizo su
 * task.state (a TASK_READY o TASK_FINISHED) antes de que esta funcion
 * retorne — el llamador debe leer tasks[winner_index].state para saber
 * cual de las dos paso. No hace falta que sync_wait_for_event reciba el
 * indice: como maximo una tarea esta RUNNING a la vez (invariante de
 * exclusion), el scheduler ya sabe cual fue porque el mismo la despacho. */
void sync_wait_for_event(Sync *sync);

/* Llamada por el hilo trabajador de `task` al iniciar cada vuelta de su
 * ciclo.
 *
 * Efecto: toma sync->mutex; while (task->state != TASK_RUNNING &&
 * !sync->stop_requested) pthread_cond_wait(&task->cond_worker,
 * &sync->mutex); suelta sync->mutex.
 *
 * Retorna 0 si task->state == TASK_RUNNING (despacho normal: el llamador
 * debe ejecutar su porcion de trabajo). Retorna distinto de cero si
 * sync_request_stop desperto al hilo antes de que le tocara correr
 * (--max-dispatches corto la observacion): el llamador debe retornar de
 * inmediato, sin ejecutar trabajo ni llamar sync_finish_turn — su tarea
 * permanece en TASK_READY, sin alterar el conjunto de tareas activas. */
int sync_wait_for_dispatch(Sync *sync, Task *task);

/* Le pide a todas las tareas TASK_READY que dejen de esperar y retornen sin
 * ejecutar mas trabajo, sin cambiar su estado. Uso exclusivo del modo
 * --max-dispatches, cuando el scheduler decide cortar la observacion antes
 * de que el conjunto de tareas activas cambie por si solo.
 *
 * Precondicion: ninguna tarea esta en TASK_RUNNING. El llamador no debe
 * tener sync->mutex tomado al invocarla.
 *
 * Efecto: toma sync->mutex; sync->stop_requested = 1; por cada tarea con
 * estado TASK_READY, pthread_cond_signal(&tasks[i].cond_worker) (despierta
 * a su worker, bloqueado en sync_wait_for_dispatch); suelta sync->mutex.
 * No modifica task->state: el conjunto de tareas activas queda intacto,
 * tal como exige el enunciado para --max-dispatches. */
void sync_request_stop(Sync *sync, Task *tasks, size_t task_count);

/* Llamada por el hilo trabajador de `task` cuando termina su porcion de
 * trabajo de esta activacion (siempre FUERA del mutex mientras corria el
 * trabajo real).
 *
 * Precondicion: next_state es TASK_READY o TASK_FINISHED (nunca
 * TASK_RUNNING — eso solo lo pone sync_dispatch).
 *
 * Efecto: toma sync->mutex; task->state = next_state; sync->event_ready = 1;
 * pthread_cond_signal(&sync->cond_scheduler); suelta sync->mutex. */
void sync_finish_turn(Sync *sync, Task *task, TaskState next_state);

#endif /* SYNC_H */
