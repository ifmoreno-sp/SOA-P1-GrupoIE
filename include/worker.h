#ifndef WORKER_H
#define WORKER_H

#include <pthread.h>
#include <stddef.h>

#include "sync.h"
#include "task.h"

/* Argumentos de un hilo trabajador: la tarea que le pertenece y el struct
 * de sincronizacion compartido con el scheduler. */
typedef struct {
    Task *task;
    Sync *sync;
} WorkerArgs;

/* Punto de entrada de pthread_create para el hilo trabajador de una tarea.
 * arg debe apuntar a un WorkerArgs valido durante toda la vida del hilo.
 *
 * Ciclo: espera ser despachado (sync_wait_for_dispatch), ejecuta su
 * trabajo (workload_run_units — en M4, siempre todo el trabajo restante
 * de una sola vez; M6 reemplaza esto por la logica real de modos), avisa
 * al scheduler (sync_finish_turn) y retorna si la tarea quedo FINISHED, o
 * vuelve a esperar si quedo READY.
 *
 * No usa sleep/usleep. No llama funciones pthread_mutex_ ni pthread_cond_
 * de forma directa: todo pasa por las funciones de sync.h. */
void *worker_thread_main(void *arg);

/* Crea un hilo por cada una de las `count` entradas de `args`, guardando
 * el pthread_t resultante en threads[i]. Precondicion: threads y args
 * tienen capacidad para `count` elementos.
 * Retorna 0 si las `count` se crearon correctamente, -1 si alguna
 * pthread_create falla (los hilos ya creados antes del fallo siguen
 * vivos; esta funcion no los une ni los cancela). */
int worker_pool_start(pthread_t *threads, WorkerArgs *args, size_t count);

/* Espera (pthread_join) a los `count` hilos en threads, en orden. Bloquea
 * hasta que todos terminen. */
void worker_pool_join(pthread_t *threads, size_t count);

#endif /* WORKER_H */
