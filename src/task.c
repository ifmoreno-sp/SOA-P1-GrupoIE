#include "task.h"

#include <assert.h>

/* Inicializa una tarea con los valores especificados. */
void task_init(Task *task, uint32_t id, uint32_t tickets, uint32_t work_units)
{
    task->id = id;
    task->tickets = tickets;
    task->work_units = work_units;
    task->completed_units = 0;
    task->dispatch_count = 0;
    task->state = TASK_READY;
    task->term = 1.0;
    task->pi_approx = 2.0;
    task->pi_index = 0;

    task->effective_tickets = tickets;
    task->debt = 0;
    task->has_yield = 0;
    task->yield_percent = 0;
    task->compensate = 1;

    int rc = pthread_cond_init(&task->cond_worker, NULL);
    assert(rc == 0);
    (void)rc;
}

void task_set_yield_config(Task *task, uint32_t yield_percent)
{
    assert(yield_percent >= 1 && yield_percent <= 99);
    task->has_yield = 1;
    task->yield_percent = yield_percent;
}

void task_disable_compensation(Task *task)
{
    task->compensate = 0;
}

/* Libera los recursos de sincronizacion de la tarea. */
void task_destroy(Task *task)
{
    pthread_cond_destroy(&task->cond_worker);
}

/* Devuelve el nombre de un estado de tarea. */
const char *task_state_name(TaskState state)
{
    switch (state) {
    case TASK_READY:
        return "READY";
    case TASK_RUNNING:
        return "RUNNING";
    case TASK_FINISHED:
        return "FINISHED";
    }
    return "UNKNOWN";
}
