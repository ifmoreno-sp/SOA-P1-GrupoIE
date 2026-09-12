#ifndef WORKER_H
#define WORKER_H

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

#include "cli.h"
#include "sync.h"
#include "task.h"

/* Argumentos de un hilo trabajador: la tarea que le pertenece, el struct
 * de sincronizacion compartido con el scheduler, y el modo de ejecucion
 * (M6) que decide cuantas unidades corre por activacion. quantum y
 * slice_percent son los mismos para todas las tareas (vienen de la CLI);
 * cada worker solo usa el campo que corresponde a wargs->mode. */
typedef struct {
    Task *task;
    Sync *sync;
    SchedulerMode mode;
    uint32_t quantum;        /* valido solo si mode == MODE_QUANTUM */
    uint32_t slice_percent;  /* valido solo si mode == MODE_COOPERATIVE, 1..100 */
} WorkerArgs;

/* Tamano del bloque cooperativo para una tarea: ceil(work_units *
 * slice_percent / 100), calculado sobre el work_units TOTAL de la tarea
 * (no el restante) -- por eso da el mismo resultado en cada activacion,
 * sin importar cuanto se haya avanzado ya.
 *
 * Aritmetica entera exacta, sin punto flotante: el enunciado exige que la
 * misma entrada/semilla/modo/parametros produzca siempre el mismo
 * resultado, y `double` introduciria redondeo dependiente del compilador.
 * El ceil se logra con la identidad ceil(a/b) == (a + b - 1) / b sobre
 * division entera (aca b = 100). El producto work_units * slice_percent
 * se computa en uint64_t para no desbordar uint32_t antes de dividir
 * (mismo criterio que csv_parser.c usa para la suma de tickets).
 *
 * Precondicion: work_units >= 1, 1 <= slice_percent <= 100. Con esas
 * precondiciones el resultado siempre es >= 1 (verificado con assert, no
 * hace falta un caso especial: ceil(work_units * 1 / 100) >= 1 para todo
 * work_units >= 1) y nunca mayor a work_units.
 *
 * Publica (no estatica) para poder probarla directo con la aritmetica,
 * sin necesidad de hilos ni del protocolo de sincronizacion. */
uint32_t cooperative_slice_size(uint32_t work_units, uint32_t slice_percent);

/* Punto de entrada de pthread_create para el hilo trabajador de una tarea.
 * arg debe apuntar a un WorkerArgs valido durante toda la vida del hilo.
 *
 * Ciclo: espera ser despachado (sync_wait_for_dispatch), calcula cuantas
 * unidades le tocan esta activacion segun wargs->mode (cooperative_slice_size
 * o wargs->quantum, lo que sea menor al trabajo restante), ejecuta ese
 * trabajo (workload_run_units), avisa al scheduler (sync_finish_turn) y
 * retorna si la tarea quedo FINISHED, o vuelve a esperar si quedo READY
 * (expropiacion simulada en quantum, cesion voluntaria en cooperativo —
 * mecanicamente identicas: solo cambia como se calculo el tamano del
 * bloque).
 *
 * Extension M9 (compensation tickets): si task->has_yield esta activo
 * (--yield-config), el bloque anterior puede recortarse aun mas por cesion
 * temprana forzada, y task->debt/effective_tickets se actualizan segun
 * corresponda -- ver decide_run_units_and_update_compensation en worker.c.
 * Con has_yield == 0 (el caso por defecto) el ciclo es identico al de
 * antes de M9. Con has_yield activo pero task->compensate == 0
 * (--disable-compensation, el "control" del experimento A/B), la tarea
 * cede la misma fraccion en cada activacion para siempre, sin que
 * debt/effective_tickets se muevan jamas de su valor base.
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
