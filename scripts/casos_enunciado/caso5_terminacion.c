/* Caso 5 del enunciado (Terminacion): "Trabajos distintos; tareas terminan
 * en momentos diferentes. Una tarea finalizada no vuelve a ganar; suma de
 * trabajo correcta; joins completos."
 *
 * Extraido de tests/test_scheduler.c (test_multiple_tasks_all_finish),
 * que ademas trae otras dos pruebas de ingenieria de Milestone 5
 * (test_single_task, test_max_dispatches_stops_and_joins_cleanly) que no
 * corresponden a este escenario del enunciado -- esas se quedan alla,
 * corridas por `make test-scheduler`. Sin framework: assert()-based con
 * contador de pasadas/fallos, al estilo del resto de tests/. */

#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include "rng.h"
#include "scheduler.h"
#include "sync.h"
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

#define MAX_EVENTS 32

typedef struct {
    DispatchEvent events[MAX_EVENTS];
    size_t count;
} EventLog;

static void record_event(const DispatchEvent *event, void *ctx)
{
    EventLog *log = ctx;
    if (log->count < MAX_EVENTS) {
        log->events[log->count] = *event;
    }
    log->count++;
}

/* Si scheduler_run se queda colgado (deadlock real), este binario nunca
 * terminaria. Mismo patron que tests/test_concurrency.c: una alarma lo
 * convierte en un fallo explicito e inmediato. */
static void on_scheduler_timeout(int sig)
{
    (void)sig;
    fprintf(stderr, "FAIL - scheduler_run no retorno a tiempo (posible deadlock)\n");
    _exit(1);
}

/* mode/quantum/slice_percent fijos en MODE_COOPERATIVE con slice_percent=100:
 * cada despacho agota el trabajo restante de una sola vez, para aislar la
 * terminacion del corte por modo (que es responsabilidad de M6/worker.c,
 * probada aparte en tests/test_execution_modes.c). */
static uint64_t scheduler_run_with_timeout(Sync *sync, Task *tasks, size_t count,
                                            Rng *rng, uint64_t max_dispatches,
                                            DispatchObserver observer, void *ctx)
{
    signal(SIGALRM, on_scheduler_timeout);
    alarm(5);
    uint64_t result = scheduler_run(sync, tasks, count, rng, MODE_COOPERATIVE, 0, 100,
                                     max_dispatches, observer, ctx);
    alarm(0);
    return result;
}

/* Varias tareas con trabajo distinto: todas deben terminar, y ninguna debe
 * ganar mas de un despacho (con el placeholder temporal del worker, cada despacho agota
 * el trabajo restante de su ganadora de una sola vez). */
static void test_multiple_tasks_all_finish(void)
{
    enum { N = 5 };
    Task tasks[N];
    const uint32_t tickets[N] = {10, 20, 30, 40, 50};
    const uint32_t work[N] = {3, 7, 1, 9, 4};
    for (int i = 0; i < N; i++) {
        task_init(&tasks[i], (uint32_t)(i + 1), tickets[i], work[i]);
    }

    Sync sync;
    check(sync_init(&sync) == 0, "sync_init exitoso (scheduler, 5 tareas)");
    Rng rng;
    check(rng_init(&rng, 777) == 0, "rng_init exitoso (scheduler, 5 tareas)");

    EventLog log = {.count = 0};
    uint64_t dispatches = scheduler_run_with_timeout(&sync, tasks, N, &rng, 0,
                                                       record_event, &log);

    check(dispatches == N,
          "N tareas (placeholder temporal del worker) toman exactamente N despachos");
    check(log.count == N, "el observer recibe exactamente N eventos");

    int seen[N + 1] = {0}; /* indexado por id (1..N) */
    int duplicate = 0;
    for (size_t i = 0; i < log.count; i++) {
        uint32_t id = log.events[i].winner_id;
        if (id >= 1 && id <= N) {
            if (seen[id]) {
                duplicate = 1;
            }
            seen[id] = 1;
        }
    }
    check(!duplicate,
          "ninguna tarea gana mas de un despacho (una FINISHED no vuelve a competir)");

    for (int i = 0; i < N; i++) {
        check(tasks[i].state == TASK_FINISHED, "cada tarea termina en TASK_FINISHED");
        check(tasks[i].completed_units == tasks[i].work_units,
              "completed_units == work_units al terminar");
        task_destroy(&tasks[i]);
    }
    sync_destroy(&sync);
}

int main(void)
{
    printf("Caso 5 -- Terminacion\n");
    test_multiple_tasks_all_finish();

    printf("\nResultado: %d pasaron, %d fallaron.\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
