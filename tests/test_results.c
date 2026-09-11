/* Pruebas del modulo de registro y resultados (M7): results_record_event,
 * results_write_stopped_row, results_write_summary_row y el calculo de
 * observed_share. No pasa por scheduler_run ni por hilos -- se construyen
 * DispatchEvent a mano, igual que test_workload.c prueba workload_run_units
 * llamandola directo. Sin framework: assert()-based con contador de
 * pasadas/fallos.
 *
 * Se usa open_memstream (POSIX) para capturar lo que se escribe en un
 * FILE* dentro de un buffer en memoria, y comparar el CSV resultante
 * como string -- evita tocar el sistema de archivos en pruebas unitarias. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "results.h"
#include "task.h"

static int passed = 0;
static int failed = 0;

static void check(int condition, const char *description)
{
    if (condition) {
        printf("  ok   - %s\n", description);
        passed++;
    } else {
        printf("  FAIL - %s\n", description);
        failed++;
    }
}

static void test_log_header(void)
{
    char *buf = NULL;
    size_t size = 0;
    FILE *mem = open_memstream(&buf, &size);

    results_write_log_header(mem);
    fclose(mem);

    check(strcmp(buf, "dispatch,winner_id,winning_ticket,active_tickets,run_units,"
                       "completed_units,state_after\n") == 0,
          "el encabezado del log tiene las 7 columnas exactas del enunciado");
    free(buf);
}

static void test_record_event_writes_row_and_accumulates_stats(void)
{
    Task tasks[2];
    task_init(&tasks[0], 10, 20, 100);
    task_init(&tasks[1], 11, 30, 100);

    TaskStats stats[2] = {0};
    char *buf = NULL;
    size_t size = 0;
    FILE *mem = open_memstream(&buf, &size);

    ResultsContext ctx;
    results_context_init(&ctx, mem, tasks, 2, stats);

    DispatchEvent ev1 = {.dispatch = 1,
                         .winner_id = 11,
                         .winning_ticket = 25,
                         .active_tickets = 50,
                         .run_units = 40,
                         .completed_units = 40,
                         .state_after = TASK_READY};
    results_record_event(&ev1, &ctx);

    DispatchEvent ev2 = {.dispatch = 2,
                         .winner_id = 11,
                         .winning_ticket = 5,
                         .active_tickets = 50,
                         .run_units = 60,
                         .completed_units = 100,
                         .state_after = TASK_FINISHED};
    results_record_event(&ev2, &ctx);

    fclose(mem);

    check(strcmp(buf, "1,11,25,50,40,40,READY\n"
                       "2,11,5,50,60,100,FINISHED\n") == 0,
          "cada evento produce exactamente una fila con los valores dados");

    check(stats[1].first_dispatch == 1, "primer despacho de la tarea 11 es 1");
    check(stats[1].last_dispatch == 2, "ultimo despacho de la tarea 11 es 2");
    check(stats[1].run_units_sum == 100, "suma de run_units de la tarea 11 es 40+60=100");
    check(stats[0].first_dispatch == 0 && stats[0].last_dispatch == 0 &&
              stats[0].run_units_sum == 0,
          "la tarea 10 (nunca gano) queda en cero");
    check(ctx.total_run_units == 100, "total_run_units acumula sobre todas las tareas");

    free(buf);
    task_destroy(&tasks[0]);
    task_destroy(&tasks[1]);
}

/* Regresion: la ganadora del ultimo despacho real (que sigue TASK_READY
 * porque no termino en esa activacion) no debe recibir ADEMAS una fila
 * STOPPED -- ya tiene su propia fila real para ese mismo dispatch_number.
 * Sin la exclusion por ctx->last_winner_id, esta tarea aparecia dos veces
 * para el mismo numero de despacho (bug encontrado corriendo el binario
 * real con --max-dispatches 1). */
static void test_stopped_rows_excludes_last_winner(void)
{
    Task tasks[3];
    task_init(&tasks[0], 1, 10, 1000); /* nunca gano: debe recibir STOPPED */
    task_init(&tasks[1], 2, 20, 1000); /* ganadora del ultimo despacho: NO debe recibir STOPPED */
    task_init(&tasks[2], 3, 30, 1000);
    tasks[2].state = TASK_FINISHED; /* ya termino: tampoco debe recibir STOPPED */

    TaskStats stats[3] = {0};
    char *log_buf = NULL;
    size_t log_size = 0;
    FILE *log_mem = open_memstream(&log_buf, &log_size);

    ResultsContext ctx;
    results_context_init(&ctx, log_mem, tasks, 3, stats);

    DispatchEvent ev = {.dispatch = 5,
                        .winner_id = 2,
                        .winning_ticket = 15,
                        .active_tickets = 30,
                        .run_units = 10,
                        .completed_units = 10,
                        .state_after = TASK_READY};
    results_record_event(&ev, &ctx);
    tasks[1].state = TASK_READY; /* resultado real de esa activacion: sigue READY */

    char *stopped_buf = NULL;
    size_t stopped_size = 0;
    FILE *stopped_mem = open_memstream(&stopped_buf, &stopped_size);
    results_write_stopped_rows(stopped_mem, &ctx, tasks, 3, 5);
    fclose(stopped_mem);
    fclose(log_mem);

    check(strcmp(stopped_buf, "5,1,0,0,0,0,STOPPED\n") == 0,
          "solo la tarea que nunca gano recibe fila STOPPED (no la ganadora del ultimo "
          "despacho, no la ya FINISHED)");

    free(log_buf);
    free(stopped_buf);
    task_destroy(&tasks[0]);
    task_destroy(&tasks[1]);
    task_destroy(&tasks[2]);
}

static void test_stopped_row(void)
{
    Task task;
    task_init(&task, 7, 15, 1000);
    task.completed_units = 250;

    char *buf = NULL;
    size_t size = 0;
    FILE *mem = open_memstream(&buf, &size);

    results_write_stopped_row(mem, &task, 42);
    fclose(mem);

    check(strcmp(buf, "42,7,0,0,0,250,STOPPED\n") == 0,
          "fila STOPPED: winning_ticket y active_tickets en 0, completed_units real");

    free(buf);
    task_destroy(&task);
}

static double parse_double_field(const char *csv_row, int field_index)
{
    const char *p = csv_row;
    for (int i = 0; i < field_index; i++) {
        p = strchr(p, ',');
        check(p != NULL, "campo esperado presente en la fila de resumen");
        p++;
    }
    return atof(p);
}

static void test_summary_row_and_observed_share(void)
{
    Task task;
    task_init(&task, 3, 20, 1000);
    task.completed_units = 400;
    task.dispatch_count = 5;
    task.pi_approx = 3.14159;

    TaskStats stats = {.first_dispatch = 2, .last_dispatch = 9, .run_units_sum = 400};

    char *buf = NULL;
    size_t size = 0;
    FILE *mem = open_memstream(&buf, &size);
    results_write_summary_row(mem, &task, &stats, 1600);
    fclose(mem);

    check(strncmp(buf, "3,20,1000,400,5,2,9,", strlen("3,20,1000,400,5,2,9,")) == 0,
          "resumen: id,tickets,work_units,completed_units,dispatches_won,"
          "first_dispatch,last_dispatch coinciden");

    double observed_share = parse_double_field(buf, 8);
    check(observed_share > 0.2499 && observed_share < 0.2501,
          "observed_share == 400/1600 == 0.25");

    free(buf);
    task_destroy(&task);

    /* Caso degenerado: total_run_units == 0 no debe dividir entre cero. */
    Task task2;
    task_init(&task2, 4, 20, 1000);
    TaskStats stats2 = {0};
    buf = NULL;
    size = 0;
    mem = open_memstream(&buf, &size);
    results_write_summary_row(mem, &task2, &stats2, 0);
    fclose(mem);

    double share_zero = parse_double_field(buf, 8);
    check(share_zero == 0.0, "observed_share es 0.0 cuando total_run_units es 0");

    free(buf);
    task_destroy(&task2);
}

int main(void)
{
    printf("Pruebas del modulo de registro y resultados:\n");
    test_log_header();
    test_record_event_writes_row_and_accumulates_stats();
    test_stopped_rows_excludes_last_winner();
    test_stopped_row();
    test_summary_row_and_observed_share();

    printf("\nResultado: %d pasaron, %d fallaron.\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
