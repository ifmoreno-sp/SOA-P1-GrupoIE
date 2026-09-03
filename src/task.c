#include "task.h"

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
