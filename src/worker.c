#include "worker.h"

#include "workload.h"

void *worker_thread_main(void *arg)
{
    WorkerArgs *wargs = arg;
    Task *task = wargs->task;
    Sync *sync = wargs->sync;

    for (;;) {
        sync_wait_for_dispatch(sync, task);

        /* Placeholder de M4: correr todo el trabajo restante de una sola
         * activacion. M6 reemplaza esto por el corte real segun el modo
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
