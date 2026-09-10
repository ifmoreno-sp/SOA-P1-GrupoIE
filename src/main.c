#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "cli.h"
#include "csv_parser.h"
#include "rng.h"
#include "scheduler.h"
#include "sync.h"
#include "task.h"

/* Observer de scheduler_run: imprime cada despacho a consola a medida que
 * ocurre. El resumen final legible y el log CSV en archivo se agregan
 * aparte — este observer es deliberadamente minimo. */
static void print_dispatch(const DispatchEvent *event, void *ctx)
{
    (void)ctx;
    printf("despacho %-6llu tarea=%-4u boleto=%u/%llu unidades=%-8u completado=%-8u estado=%s\n",
           (unsigned long long)event->dispatch, event->winner_id,
           event->winning_ticket, (unsigned long long)event->active_tickets,
           event->run_units, event->completed_units,
           task_state_name(event->state_after));
}

int main(int argc, char *argv[])
{
    CliOptions opts;
    char errbuf[CLI_ERRBUF_SIZE];

    if (cli_parse(argc, argv, &opts, errbuf) != 0) {
        fprintf(stderr, "error: %s\n%s\n", errbuf, cli_usage());
        return EXIT_FAILURE;
    }

    Task *tasks = NULL;
    size_t task_count = 0;
    char csv_errbuf[CSV_PARSER_ERRBUF_SIZE];

    if (csv_parser_load(opts.input_path, &tasks, &task_count, csv_errbuf) != 0) {
        fprintf(stderr, "error: %s\n", csv_errbuf);
        return EXIT_FAILURE;
    }

    uint64_t total_tickets = 0;
    uint64_t total_work = 0;
    for (size_t i = 0; i < task_count; i++) {
        total_tickets += tasks[i].tickets;
        total_work += tasks[i].work_units;
    }

    printf("entrada: %s\n", opts.input_path);
    printf("modo: %s", opts.mode == MODE_QUANTUM ? "quantum" : "cooperative");
    if (opts.mode == MODE_QUANTUM) {
        printf(" (Q=%u)\n", opts.quantum);
    } else {
        printf(" (P=%u%%)\n", opts.slice_percent);
    }
    printf("seed: %u\n", opts.seed);
    printf("log: %s\n", opts.log_path);
    printf("summary: %s\n",
           opts.summary_path != NULL ? opts.summary_path : "(no solicitado)");
    if (opts.has_max_dispatches) {
        printf("max-dispatches: %llu\n",
               (unsigned long long)opts.max_dispatches);
    }

    printf("\ntareas: %zu | boletos activos: %llu | trabajo total: %llu\n\n",
           task_count, (unsigned long long)total_tickets,
           (unsigned long long)total_work);

    Rng rng;
    /* cli_parse ya garantiza opts.seed != 0. */
    assert(rng_init(&rng, opts.seed) == 0);

    Sync sync;
    assert(sync_init(&sync) == 0);

    uint64_t max_dispatches = opts.has_max_dispatches ? opts.max_dispatches : 0;
    uint64_t total_dispatches = scheduler_run(&sync, tasks, task_count, &rng,
                                               max_dispatches, print_dispatch, NULL);

    printf("\ntotal de despachos: %llu\n", (unsigned long long)total_dispatches);
    printf("%-8s %-10s %-12s %-12s %-10s\n", "id", "tickets", "work_units",
           "completado", "estado");
    for (size_t i = 0; i < task_count; i++) {
        printf("%-8u %-10u %-12u %-12u %-10s\n", tasks[i].id, tasks[i].tickets,
               tasks[i].work_units, tasks[i].completed_units,
               task_state_name(tasks[i].state));
    }

    sync_destroy(&sync);
    for (size_t i = 0; i < task_count; i++) {
        task_destroy(&tasks[i]);
    }
    free(tasks);

    return EXIT_SUCCESS;
}
