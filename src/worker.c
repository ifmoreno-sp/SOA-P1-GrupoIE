#include "worker.h"

#include <assert.h>

#include "workload.h"

uint32_t cooperative_slice_size(uint32_t work_units, uint32_t slice_percent)
{
    assert(work_units >= 1);
    assert(slice_percent >= 1 && slice_percent <= 100);

    /* uint64_t para el producto intermedio: work_units * slice_percent cabe
     * holgado en 64 bits (maximo realista ~10,000,000 * 100), pero seguimos
     * la misma convencion que csv_parser.c para la suma de tickets. */
    uint64_t block = ((uint64_t)work_units * slice_percent + 99) / 100;

    assert(block >= 1 && block <= work_units);
    return (uint32_t)block;
}

/* Cuantas unidades corren en esta activacion: el tamano de bloque segun el
 * modo (fijo en quantum, calculado sobre el total en cooperativo), topado
 * por lo que realmente falta. */
static uint32_t decide_slice_units(const WorkerArgs *wargs)
{
    const Task *task = wargs->task;
    uint32_t remaining = task->work_units - task->completed_units;

    uint32_t requested = (wargs->mode == MODE_COOPERATIVE)
                              ? cooperative_slice_size(task->work_units, wargs->slice_percent)
                              : wargs->quantum;

    return (requested < remaining) ? requested : remaining;
}

/* Bucle infinito del thread del worker */
void *worker_thread_main(void *arg)
{
    WorkerArgs *wargs = arg;
    Task *task = wargs->task;
    Sync *sync = wargs->sync;

    for (;;) {
        if (sync_wait_for_dispatch(sync, task) != 0) {
            /* --max-dispatches corto la observacion antes de que esta
             * tarea volviera a ser despachada: retorna sin ejecutar
             * trabajo ni cambiar su estado (queda en TASK_READY). */
            break;
        }

        uint32_t run_units = decide_slice_units(wargs);
        workload_run_units(task, run_units);

        TaskState next_state = (task->completed_units == task->work_units)
                                    ? TASK_FINISHED
                                    : TASK_READY;
        sync_finish_turn(sync, task, next_state);

        if (next_state == TASK_FINISHED) {
            break;
        }
    }

    return NULL;
}

/* Inicializa un grupo de worker threads */
int worker_pool_start(pthread_t *threads, WorkerArgs *args, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        if (pthread_create(&threads[i], NULL, worker_thread_main, &args[i]) != 0) {
            return -1;
        }
    }
    return 0;
}

/* Espera a que todos los worker threads terminen */
void worker_pool_join(pthread_t *threads, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        pthread_join(threads[i], NULL);
    }
}
