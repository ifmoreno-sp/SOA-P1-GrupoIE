#include "scheduler.h"

#include <assert.h>

#include "csv_parser.h"
#include "worker.h"

uint64_t scheduler_run(Sync *sync, Task *tasks, size_t task_count, Rng *rng,
                        SchedulerMode mode, uint32_t quantum, uint32_t slice_percent,
                        uint64_t max_dispatches, DispatchObserver observer,
                        void *observer_ctx)
{
    assert(task_count > 0 && task_count <= CSV_PARSER_MAX_TASKS);
    assert(mode == MODE_COOPERATIVE ? (slice_percent >= 1 && slice_percent <= 100)
                                     : quantum >= 1);

    pthread_t threads[CSV_PARSER_MAX_TASKS];
    WorkerArgs args[CSV_PARSER_MAX_TASKS];
    for (size_t i = 0; i < task_count; i++) {
        args[i].task = &tasks[i];
        args[i].sync = sync;
        args[i].mode = mode;
        args[i].quantum = quantum;
        args[i].slice_percent = slice_percent;
    }

    /* pthread_create solo falla por agotamiento de recursos del sistema:
     * no aplica al tamano de este proyecto (<=25 hilos). Mismo criterio
     * que task_init/sync_init (ver sus comentarios). */
    assert(worker_pool_start(threads, args, task_count) == 0);

    uint64_t dispatch_count = 0;
    for (;;) {
        if (max_dispatches > 0 && dispatch_count >= max_dispatches) {
            sync_request_stop(sync, tasks, task_count);
            break;
        }

        Selection sel = sync_select_winner(sync, tasks, task_count, rng);
        if (sel.index == task_count) {
            break; /* ninguna tarea TASK_READY: todas terminaron */
        }

        sync_dispatch(sync, tasks, task_count, sel.index);
        sync_wait_for_event(sync);
        dispatch_count++;

        if (observer != NULL) {
            /* Seguro leer tasks[sel.index] sin el mutex aqui: postcondicion
             * documentada de sync_wait_for_event (el estado ya quedo
             * actualizado antes de que retorne). */
            Task *winner = &tasks[sel.index];
            DispatchEvent event = {
                .dispatch = dispatch_count,
                .winner_id = winner->id,
                .winning_ticket = sel.winning_ticket,
                .active_tickets = sel.active_tickets,
                .run_units = winner->completed_units - sel.completed_units_before,
                .completed_units = winner->completed_units,
                .state_after = winner->state,
            };
            observer(&event, observer_ctx);
        }
    }

    worker_pool_join(threads, task_count);
    return dispatch_count;
}
