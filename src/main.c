#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "cli.h"
#include "csv_parser.h"
#include "results.h"
#include "rng.h"
#include "scheduler.h"
#include "sync.h"
#include "task.h"

/* Abre `path` en modo escritura. Si falla, imprime un mensaje claro
 * identificando cual bandera fallo (--log o --summary) y retorna NULL —
 * el llamador decide como limpiar antes de terminar (no se llama exit()
 * aqui para no saltarse la liberacion de lo que ya se haya reservado). */
static FILE *open_output(const char *path, const char *label)
{
    FILE *file = fopen(path, "w");
    if (file == NULL) {
        fprintf(stderr, "error: no se pudo abrir %s '%s' para escritura\n", label, path);
    }
    return file;
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

    /* Ambos archivos de salida se abren ANTES de correr el scheduler (no
     * solo --log): si --summary apunta a una ruta invalida, el programa
     * debe fallar aqui, no despues de haber ejecutado toda la simulacion
     * para nada. Cualquier fallo libera lo que ya se haya reservado antes
     * de retornar -- no se usa exit() para no saltarse esa limpieza. */
    FILE *log_file = open_output(opts.log_path, "--log");
    if (log_file == NULL) {
        free(tasks);
        return EXIT_FAILURE;
    }

    FILE *summary_file = NULL;
    if (opts.summary_path != NULL) {
        summary_file = open_output(opts.summary_path, "--summary");
        if (summary_file == NULL) {
            fclose(log_file);
            free(tasks);
            return EXIT_FAILURE;
        }
    }

    Rng rng;
    /* cli_parse ya garantiza opts.seed != 0. */
    assert(rng_init(&rng, opts.seed) == 0);

    Sync sync;
    assert(sync_init(&sync) == 0);

    results_write_log_header(log_file);

    TaskStats stats[CSV_PARSER_MAX_TASKS] = {0};
    ResultsContext results;
    results_context_init(&results, log_file, tasks, task_count, stats);

    uint64_t max_dispatches = opts.has_max_dispatches ? opts.max_dispatches : 0;
    uint64_t total_dispatches = scheduler_run(&sync, tasks, task_count, &rng,
                                               opts.mode, opts.quantum, opts.slice_percent,
                                               max_dispatches, results_record_event, &results);

    /* Tareas que quedaron TASK_READY sin terminar: solo pasa si
     * --max-dispatches corto la observacion (ver postcondicion de
     * scheduler_run). Se documentan con una fila STOPPED aparte (excepto
     * la ganadora del ultimo despacho real, que ya tiene su propia fila
     * para ese mismo numero de despacho -- ver results_write_stopped_rows). */
    results_write_stopped_rows(log_file, &results, tasks, task_count, total_dispatches);
    fclose(log_file);

    if (summary_file != NULL) {
        results_write_summary_header(summary_file);
        for (size_t i = 0; i < task_count; i++) {
            results_write_summary_row(summary_file, &tasks[i], &stats[i],
                                       results.total_run_units);
        }
        fclose(summary_file);
    }

    printf("total de despachos: %llu\n\n", (unsigned long long)total_dispatches);
    results_print_console_summary(tasks, stats, task_count, results.total_run_units);

    sync_destroy(&sync);
    for (size_t i = 0; i < task_count; i++) {
        task_destroy(&tasks[i]);
    }
    free(tasks);

    return EXIT_SUCCESS;
}
