#ifndef TASK_H
#define TASK_H

#include <pthread.h>
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

    /* El hilo trabajador de ESTA tarea espera aqui hasta que su estado pase
     * a TASK_RUNNING (ver sync.h). Protegida por el mutex de Sync, no por un
     * lock propio. */
    pthread_cond_t cond_worker;
} Task;

/* Inicializa una tarea recien leida del CSV: estado TASK_READY, progreso y
 * despachos en cero, y la serie de pi en su punto de partida
 * (term = 1, pi_approx = 2, indice = 0). Ademas inicializa cond_worker
 * (pthread_cond_init). Se asume que no falla (assert(rc == 0)):
 * pthread_cond_init solo falla por agotamiento de recursos del sistema, no
 * aplica al tamano de este proyecto (<=25 tareas).
 * Precondicion: task != NULL. */
void task_init(Task *task, uint32_t id, uint32_t tickets, uint32_t work_units);

/* Libera cond_worker (pthread_cond_destroy).
 * Precondicion: task fue inicializado con task_init y su hilo trabajador ya
 * termino (pthread_join ya se hizo). */
void task_destroy(Task *task);

/* Nombre legible del estado, para logs y resumenes. */
const char *task_state_name(TaskState state);

#endif /* TASK_H */
