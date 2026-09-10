#include "worker.h"

#include "workload.h"

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

        /* Placeholder temporal: corre todo el trabajo restante de una sola
         * activacion. Se reemplazara por el corte real segun el modo
         * (cooperativo/quantum). */
        uint32_t remaining = task->work_units - task->completed_units;
        workload_run_units(task, remaining);

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

int worker_pool_start(pthread_t *threads, WorkerArgs *args, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        if (pthread_create(&threads[i], NULL, worker_thread_main, &args[i]) != 0) {
            return -1;
        }
    }
    return 0;
}

void worker_pool_join(pthread_t *threads, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        pthread_join(threads[i], NULL);
    }
}
