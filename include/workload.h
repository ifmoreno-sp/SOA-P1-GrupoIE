#ifndef WORKLOAD_H
#define WORKLOAD_H

#include <stdint.h>

#include "task.h"

/* Ejecuta `units` iteraciones de la serie de Taylor de arcsin(1) sobre
 * `task`, reanudando desde su estado guardado (term, pi_approx, pi_index)
 * en vez de reiniciar la serie. Actualiza term, pi_approx y pi_index, y
 * suma `units` a task->completed_units.
 *
 * Precondicion: task != NULL y units <= task->work_units - task->completed_units.
 * Quien decide cuantas unidades corresponden a una activacion (modo
 * cooperativo, quantum, o la extension de compensation tickets) es
 * responsabilidad del llamador; esta funcion solo ejecuta el computo.
 *
 * No usa temporizadores ni espera de ningun tipo: es computo puro y
 * reentrante en el sentido de que dos tareas distintas pueden tener cada
 * una su propio estado avanzando de forma independiente. No es segura de
 * llamar concurrentemente sobre el mismo `task` sin sincronizacion externa,
 * pero el diseno del scheduler (ver milestones de concurrencia) garantiza
 * que como maximo una tarea corre a la vez. */
void workload_run_units(Task *task, uint32_t units);

#endif /* WORKLOAD_H */
