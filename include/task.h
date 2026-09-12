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

    /* Extension M9 (compensation tickets). `tickets` de arriba es siempre el
     * valor base; el sorteo (sync.c) usa `effective_tickets` en su lugar,
     * que solo diverge de `tickets` mientras esta tarea esta compensando una
     * cesion temprana (ver worker.h). `debt` son las unidades que le faltan
     * para terminar de "pagar" esa cesion antes de volver a `tickets`.
     * `has_yield`/`yield_percent` son la configuracion opcional (activada
     * solo via --yield-config, ver cli.h) que hace que esta tarea ceda
     * temprano cada vez que gana y no tiene deuda pendiente; en el
     * comportamiento base (sin --yield-config) has_yield es 0 y estos tres
     * campos nunca se usan, dejando el sorteo identico a antes de M9. */
    uint32_t effective_tickets;
    uint32_t debt;
    int has_yield;
    uint32_t yield_percent; /* valido solo si has_yield != 0, en [1, 99] */

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

/* Activa la cesion temprana forzada (extension M9) para esta tarea: cuando
 * gane el sorteo sin deuda pendiente, cedera tras correr solo
 * yield_percent% de su bloque decidido, en vez de agotarlo. Deshabilitada
 * por defecto (task_init deja has_yield en 0); esta funcion es la unica
 * forma de activarla, y solo la usa main.c para las tareas listadas en
 * --yield-config. No toca tickets ni effective_tickets: eso lo actualiza
 * worker.c en la primera activacion de esta tarea.
 * Precondicion: yield_percent en [1, 99]. */
void task_set_yield_config(Task *task, uint32_t yield_percent);

/* Libera cond_worker (pthread_cond_destroy).
 * Precondicion: task fue inicializado con task_init y su hilo trabajador ya
 * termino (pthread_join ya se hizo). */
void task_destroy(Task *task);

/* Nombre legible del estado, para logs y resumenes. */
const char *task_state_name(TaskState state);

#endif /* TASK_H */
