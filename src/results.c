#include "results.h"

#include <assert.h>

void results_context_init(ResultsContext *ctx, FILE *log_file,
                           const Task *tasks, size_t task_count, TaskStats *stats)
{
    ctx->log_file = log_file;
    ctx->tasks = tasks;
    ctx->task_count = task_count;
    ctx->stats = stats;
    ctx->total_run_units = 0;
}

void results_write_log_header(FILE *file)
{
    fprintf(file, "dispatch,winner_id,winning_ticket,active_tickets,run_units,"
                  "completed_units,state_after\n");
}

/* Busca el indice de `id` dentro de tasks[0..task_count). Siempre lo
 * encuentra: winner_id viene de un DispatchEvent real, generado por
 * scheduler_run a partir de una tarea que pertenece a este mismo arreglo. */
static size_t find_task_index(const Task *tasks, size_t task_count, uint32_t id)
{
    for (size_t i = 0; i < task_count; i++) {
        if (tasks[i].id == id) {
            return i;
        }
    }
    assert(0 && "winner_id no encontrado en tasks[]");
    return task_count;
}

void results_record_event(const DispatchEvent *event, void *ctx_ptr)
{
    ResultsContext *ctx = ctx_ptr;

    fprintf(ctx->log_file, "%llu,%u,%u,%llu,%u,%u,%s\n",
            (unsigned long long)event->dispatch, event->winner_id,
            event->winning_ticket, (unsigned long long)event->active_tickets,
            event->run_units, event->completed_units,
            task_state_name(event->state_after));

    size_t i = find_task_index(ctx->tasks, ctx->task_count, event->winner_id);
    TaskStats *s = &ctx->stats[i];
    if (s->first_dispatch == 0) {
        s->first_dispatch = event->dispatch;
    }
    s->last_dispatch = event->dispatch;
    s->run_units_sum += event->run_units;
    ctx->total_run_units += event->run_units;
}

void results_write_stopped_row(FILE *file, const Task *task, uint64_t dispatch_number)
{
    fprintf(file, "%llu,%u,0,0,0,%u,STOPPED\n",
            (unsigned long long)dispatch_number, task->id, task->completed_units);
}

void results_write_summary_header(FILE *file)
{
    fprintf(file, "id,tickets,work_units,completed_units,dispatches_won,"
                  "first_dispatch,last_dispatch,pi_final,observed_share\n");
}

static double compute_observed_share(uint64_t run_units_sum, uint64_t total_run_units)
{
    if (total_run_units == 0) {
        return 0.0;
    }
    return (double)run_units_sum / (double)total_run_units;
}

void results_write_summary_row(FILE *file, const Task *task, const TaskStats *stats,
                                uint64_t total_run_units)
{
    double observed_share = compute_observed_share(stats->run_units_sum, total_run_units);
    fprintf(file, "%u,%u,%u,%u,%u,%llu,%llu,%.10f,%.6f\n",
            task->id, task->tickets, task->work_units, task->completed_units,
            task->dispatch_count, (unsigned long long)stats->first_dispatch,
            (unsigned long long)stats->last_dispatch, task->pi_approx, observed_share);
}

void results_print_console_summary(const Task *tasks, const TaskStats *stats,
                                    size_t task_count, uint64_t total_run_units)
{
    printf("%-6s %-8s %-11s %-11s %-9s %-9s %-9s %-14s %-s\n", "id", "tickets",
           "work_units", "completado", "ganados", "primero", "ultimo", "pi_final",
           "observed_share");
    for (size_t i = 0; i < task_count; i++) {
        double observed_share = compute_observed_share(stats[i].run_units_sum, total_run_units);
        printf("%-6u %-8u %-11u %-11u %-9u %-9llu %-9llu %-14.10f %.6f\n",
               tasks[i].id, tasks[i].tickets, tasks[i].work_units,
               tasks[i].completed_units, tasks[i].dispatch_count,
               (unsigned long long)stats[i].first_dispatch,
               (unsigned long long)stats[i].last_dispatch, tasks[i].pi_approx,
               observed_share);
    }
}
