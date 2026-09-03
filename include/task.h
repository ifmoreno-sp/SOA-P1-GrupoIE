#ifndef TASK_H
#define TASK_H

#include <stdint.h>

/* Estados posibles de una tarea. Solo una tarea puede estar en TASK_RUNNING
 * en un instante logico dado; una TASK_FINISHED no vuelve a competir. */
typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_FINISHED
} TaskState;

typedef struct {
    uint32_t id;
    uint32_t tickets;
    uint32_t work_units;
    uint32_t completed_units;
    uint32_t dispatch_count;
    TaskState state;

    /* Estado privado de la aproximacion incremental de pi (serie de Taylor de
     * arcsin(1)). Una activacion reanuda desde aqui, nunca reinicia la serie. */
    double term;
    double pi_approx;
    uint64_t pi_index;
} Task;

/* Inicializa una tarea recien leida del CSV: estado TASK_READY, progreso y
 * despachos en cero, y la serie de pi en su punto de partida
 * (term = 1, pi_approx = 2, indice = 0).
 * Precondicion: task != NULL. */
void task_init(Task *task, uint32_t id, uint32_t tickets, uint32_t work_units);

/* Nombre legible del estado, para logs y resumenes. */
const char *task_state_name(TaskState state);

#endif /* TASK_H */
