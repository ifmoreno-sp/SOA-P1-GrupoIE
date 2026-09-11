/* Pruebas del bucle del scheduler: task.c + sync.c + worker.c +
 * workload.c + rng.c + scheduler.c trabajando juntos con hilos reales, a
 * traves de la interfaz publica de scheduler_run directamente (sin pasar
 * por la CLI, que se prueba aparte en tests/test_input_validation.sh). Sin
 * framework: assert()-based con contador de pasadas/fallos, al estilo de
 * tests/test_concurrency.c.
 *
 * El invariante de exclusion y el sorteo ponderado ya tienen sus propias
 * pruebas en tests/test_concurrency.c (sync_dispatch, sync_select_winner);
 * aqui se prueba el bucle completo: terminacion normal, que ninguna tarea
 * gane dos veces, y que --max-dispatches corte la observacion sin dejar
 * hilos bloqueados. */

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
 * reproduce exactamente el comportamiento que estas pruebas ya esperaban
 * antes de M6 (cada despacho agota el trabajo restante de una sola vez),
 * ya que decidir el corte real por modo es responsabilidad de M6/worker.c,
 * no de este archivo. */
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

/* Caso simple: una sola tarea. Confirma el camino feliz completo, incluido
 * lo que recibe el observer. */
static void test_single_task(void)
{
    Task tasks[1];
    task_init(&tasks[0], 1, 10, 7);

    Sync sync;
    check(sync_init(&sync) == 0, "sync_init exitoso (scheduler, 1 tarea)");
    Rng rng;
    check(rng_init(&rng, 2026) == 0, "rng_init exitoso (scheduler, 1 tarea)");

    EventLog log = {.count = 0};
    uint64_t dispatches = scheduler_run_with_timeout(&sync, tasks, 1, &rng, 0,
                                                       record_event, &log);

    check(dispatches == 1, "una sola tarea toma exactamente 1 despacho");
    check(log.count == 1, "el observer recibe exactamente 1 evento");
    check(tasks[0].state == TASK_FINISHED, "la tarea termina en TASK_FINISHED");
    check(tasks[0].completed_units == tasks[0].work_units,
          "completed_units == work_units al terminar");

    if (log.count >= 1) {
        DispatchEvent ev = log.events[0];
        check(ev.dispatch == 1, "el evento reporta dispatch == 1");
        check(ev.winner_id == tasks[0].id, "el evento reporta el id correcto");
        check(ev.run_units == tasks[0].work_units,
              "run_units == work_units (unica activacion, placeholder temporal del worker)");
        check(ev.completed_units == tasks[0].work_units,
              "completed_units del evento coincide con el final");
        check(ev.state_after == TASK_FINISHED, "state_after == TASK_FINISHED");
        check(ev.active_tickets == 10, "active_tickets == boletos de la unica tarea");
        check(ev.winning_ticket >= 1 && ev.winning_ticket <= 10,
              "winning_ticket cae en [1, active_tickets]");
    }

    task_destroy(&tasks[0]);
    sync_destroy(&sync);
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

/* --max-dispatches: el bucle debe detenerse tras exactamente el limite
 * pedido, sin alterar el estado de las tareas no despachadas, y sin dejar
 * ningun worker bloqueado (scheduler_run_with_timeout lo verifica). */
static void test_max_dispatches_stops_and_joins_cleanly(void)
{
    enum { N = 5 };
    Task tasks[N];
    for (int i = 0; i < N; i++) {
        task_init(&tasks[i], (uint32_t)(i + 1), 10, 100);
    }

    Sync sync;
    check(sync_init(&sync) == 0, "sync_init exitoso (scheduler, max-dispatches)");
    Rng rng;
    check(rng_init(&rng, 555) == 0, "rng_init exitoso (scheduler, max-dispatches)");

    EventLog log = {.count = 0};
    uint64_t dispatches = scheduler_run_with_timeout(&sync, tasks, N, &rng, 2,
                                                       record_event, &log);

    check(dispatches == 2, "el bucle se detiene tras exactamente max_dispatches despachos");
    check(log.count == 2, "el observer recibe exactamente max_dispatches eventos");
    check(1, "scheduler_run retorna sin bloquear (no deadlock tras --max-dispatches)");

    int finished_count = 0;
    int ready_count = 0;
    for (int i = 0; i < N; i++) {
        if (tasks[i].state == TASK_FINISHED) {
            finished_count++;
        }
        if (tasks[i].state == TASK_READY) {
            ready_count++;
        }
        task_destroy(&tasks[i]);
    }
    check(finished_count == 2, "exactamente las tareas despachadas quedan TASK_FINISHED");
    check(ready_count == N - 2,
          "las tareas no despachadas quedan TASK_READY, sin alterar el conjunto activo");

    sync_destroy(&sync);
}

int main(void)
{
    printf("Pruebas del bucle del scheduler:\n");
    test_single_task();
    test_multiple_tasks_all_finish();
    test_max_dispatches_stops_and_joins_cleanly();

    printf("\nResultado: %d pasaron, %d fallaron.\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
