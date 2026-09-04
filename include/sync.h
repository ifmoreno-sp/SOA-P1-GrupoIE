#ifndef SYNC_H
#define SYNC_H

#include <pthread.h>
#include <stddef.h>

#include "task.h"

typedef struct {
    pthread_mutex_t mutex;         /* protege: task[i].state de todas las
                                     * tareas, dispatch_count, event_ready.
                                     * NO protege la ejecucion de workload_run_units. */
    pthread_cond_t cond_scheduler; /* el scheduler espera aqui un evento */
    int event_ready;               /* predicado de cond_scheduler */
} Sync;

/* Inicializa mutex y cond_scheduler, event_ready = 0.
 * Retorna 0 en exito, -1 si pthread_mutex_init o pthread_cond_init fallan
 * (recursos del sistema agotados). */
int sync_init(Sync *sync);

/* Precondicion: nadie esta bloqueado en sync (todos los hilos ya se unieron
 * con pthread_join). Destruye mutex y cond_scheduler.
 * No destruye los cond_worker de cada Task — eso lo hace task_destroy. */
void sync_destroy(Sync *sync);

/* Llamada por el scheduler (en M4, el test de integracion; en M5, el
 * bucle real de main()) para despachar a la tarea ganadora.
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
 * ciclo. Efecto: toma sync->mutex; while (task->state != TASK_RUNNING)
 * pthread_cond_wait(&task->cond_worker, &sync->mutex); suelta sync->mutex. */
void sync_wait_for_dispatch(Sync *sync, Task *task);

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
