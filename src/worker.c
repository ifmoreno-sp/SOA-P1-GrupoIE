#include "worker.h"
#include <assert.h>
#include "workload.h"

uint32_t cooperative_slice_size(uint32_t work_units, uint32_t slice_percent)
{
    assert(work_units >= 1);
    assert(slice_percent >= 1 && slice_percent <= 100);

    /* ceil(a/b) == (a+b-1)/b en enteros; ver worker.h para el por que de
     * esta forma y del uint64_t intermedio. */
    uint64_t block = ((uint64_t)work_units * slice_percent + 99) / 100;

    assert(block >= 1 && block <= work_units);
    return (uint32_t)block;
}

/* Tamano de bloque que decide el modo para esta activacion (fijo en
 * quantum, calculado sobre el total en cooperativo), SIN topar por lo que
 * realmente falta. Separado de decide_slice_units porque el calculo de
 * compensation tickets (M9) necesita el bloque "completo" que el modo le
 * asigno, no el recortado por cercania al final de la tarea. */
static uint32_t decide_mode_block(const WorkerArgs *wargs)
{
    const Task *task = wargs->task;
    return (wargs->mode == MODE_COOPERATIVE)
               ? cooperative_slice_size(task->work_units, wargs->slice_percent)
               : wargs->quantum;
}

/* Cuantas unidades corren en esta activacion: el bloque del modo, topado
 * por lo que realmente falta. */
static uint32_t decide_slice_units(const WorkerArgs *wargs)
{
    const Task *task = wargs->task;
    uint32_t remaining = task->work_units - task->completed_units;
    uint32_t requested = decide_mode_block(wargs);
    return (requested < remaining) ? requested : remaining;
}

/* M9 (compensation tickets): decide cuantas unidades corre esta activacion
 * y, si task->has_yield esta activo, actualiza task->debt/effective_tickets.
 * Sin --yield-config (has_yield == 0) es identica a decide_slice_units y no
 * toca ningun campo de compensacion: comportamiento identico a antes de M9.
 *
 * Con has_yield activo:
 *   - Sin deuda pendiente (debt == 0): intenta ceder temprano, corriendo
 *     solo ceil(block * yield_percent / 100) unidades del bloque que el
 *     modo asigno esta vez (topado por lo que falta). Si el redondeo no
 *     deja una cesion real (bloque tan chico que ese calculo iguala al
 *     bloque completo -- solo pasa cerca del final de la tarea), corre el
 *     bloque completo sin activar compensacion: no hay nada que compensar.
 *     Si si cede temprano: debt = block - unidades_corridas,
 *     effective_tickets = ceil(tickets_base * block / unidades_corridas)
 *     (ver worker.h para la justificacion de la aritmetica entera).
 *   - Con deuda pendiente (debt > 0) Y compensando (task->compensate != 0):
 *     corre el bloque completo (no vuelve a ceder temprano mientras
 *     compensa) y descuenta lo corrido de la deuda. Si la deuda llega a 0,
 *     effective_tickets vuelve a tickets_base -- la proxima vez que gane,
 *     si sigue sin deuda, vuelve a ceder temprano: el ciclo se repite
 *     durante toda la corrida, tal como describe el escenario de la
 *     extension (un "servidor" que cede por I/O una y otra vez).
 *
 * task->compensate == 0 (task_disable_compensation, el "control" del
 * experimento A/B): debt nunca se fija y effective_tickets nunca se toca,
 * asi que la rama de "con deuda pendiente" de arriba nunca se activa -- la
 * tarea cede la MISMA fraccion en cada activacion, para siempre, sin
 * ningun tickets_compensados. Aisla el efecto de ceder temprano (por si
 * solo) del efecto de la compensacion. */
static uint32_t decide_run_units_and_update_compensation(WorkerArgs *wargs)
{
    Task *task = wargs->task;
    uint32_t block = decide_slice_units(wargs);

    if (!task->has_yield) {
        return block;
    }

    if (task->compensate && task->debt > 0) {
        if (block >= task->debt) {
            task->debt = 0;
            task->effective_tickets = task->tickets;
        } else {
            task->debt -= block;
        }
        return block;
    }

    uint64_t raw = ((uint64_t)block * task->yield_percent + 99) / 100;
    if (raw >= block) {
        /* No hay cesion real posible con este bloque: corre normal. */
        return block;
    }
    uint32_t run_now = (uint32_t)raw;

    if (task->compensate) {
        task->debt = block - run_now;
        uint64_t compensated = ((uint64_t)task->tickets * block + run_now - 1) / run_now;
        assert(compensated <= UINT32_MAX);
        task->effective_tickets = (uint32_t)compensated;
    }
    return run_now;
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

        uint32_t run_units = decide_run_units_and_update_compensation(wargs);
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
