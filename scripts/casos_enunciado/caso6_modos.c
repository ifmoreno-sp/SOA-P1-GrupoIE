/* Caso 6 del enunciado (Modos): "Misma entrada y semilla en cooperativo y
 * quantum discreto. Mismo trabajo final y pi; comparacion de despachos y
 * costo de coordinacion."
 *
 * Extraido de tests/test_execution_modes.c (test_modes_produce_same_result),
 * que ademas trae otras ocho pruebas de ingenieria de Milestone 6
 * (aritmetica de cooperative_slice_size, casos borde de quantum, etc.) que
 * no corresponden a este escenario del enunciado -- esas se quedan alla,
 * corridas por `make test-modes`. Mismo "scheduler falso" (despacha en
 * orden fijo por indice, sin loteria) para aislar el corte por modo de la
 * logica de sorteo. Sin framework: assert()-based con contador de
 * pasadas/fallos, al estilo del resto de tests/. */

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include "sync.h"
#include "task.h"
#include "worker.h"

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

/* Misma proteccion que en test_concurrency.c: si el protocolo tuviera un
 * bug que cuelgue el join, la alarma lo convierte en un fallo explicito en
 * vez de dejar make casos-enunciado esperando para siempre. */
static void on_join_timeout(int sig)
{
    (void)sig;
    _exit(1);
}

static void join_with_timeout(pthread_t *threads, size_t count)
{
    signal(SIGALRM, on_join_timeout);
    alarm(10);
    worker_pool_join(threads, count);
    alarm(0);
}

static void run_fake_scheduler(Sync *sync, Task *tasks, size_t count)
{
    for (;;) {
        size_t winner = count;
        for (size_t i = 0; i < count; i++) {
            if (tasks[i].state == TASK_READY) {
                winner = i;
                break;
            }
        }
        if (winner == count) {
            return;
        }
        sync_dispatch(sync, tasks, count, winner);
        sync_wait_for_event(sync);
    }
}

/* Corre una tarea hasta el final con el modo indicado y devuelve cuantos
 * despachos hicieron falta. Deja la tarea lista para inspeccionar. */
static uint32_t run_one_task(Task *task, SchedulerMode mode, uint32_t quantum,
                             uint32_t slice_percent)
{
    Sync sync;
    if (sync_init(&sync) != 0) {
        return 0;
    }

    pthread_t thread;
    WorkerArgs args = {.task = task,
                       .sync = &sync,
                       .mode = mode,
                       .quantum = quantum,
                       .slice_percent = slice_percent};
    if (worker_pool_start(&thread, &args, 1) != 0) {
        sync_destroy(&sync);
        return 0;
    }

    run_fake_scheduler(&sync, task, 1);
    join_with_timeout(&thread, 1);
    sync_destroy(&sync);

    return task->dispatch_count;
}

/* Caso de prueba 6 del enunciado: misma entrada en ambos modos debe dar el
 * mismo trabajo final y el mismo pi, aunque difiera el numero de despachos
 * (el costo de coordinacion). El resultado de pi se compara bit a bit: la
 * serie es determinista y las mismas N unidades en distinto orden de corte
 * producen exactamente la misma secuencia de operaciones. */
static void test_modes_produce_same_result(void)
{
    Task coop;
    Task quant;
    task_init(&coop, 1, 10, 60);
    task_init(&quant, 1, 10, 60);

    uint32_t coop_dispatches = run_one_task(&coop, MODE_COOPERATIVE, 0, 10);
    uint32_t quant_dispatches = run_one_task(&quant, MODE_QUANTUM, 25, 0);

    check(coop.completed_units == quant.completed_units,
          "ambos modos completan el mismo trabajo total");
    check(coop.pi_approx == quant.pi_approx,
          "ambos modos producen el mismo pi final, bit a bit");
    check(coop.pi_index == quant.pi_index,
          "ambos modos avanzan la serie hasta el mismo indice");
    check(coop.state == TASK_FINISHED && quant.state == TASK_FINISHED,
          "ambos modos terminan la tarea");
    /* 60 unidades: cooperativo al 10% da bloques de 6 (10 activaciones);
     * quantum Q=25 da 25+25+10 (3 activaciones). Mismo resultado, distinto
     * costo de coordinacion. */
    check(coop_dispatches == 10, "cooperativo 10% sobre 60 unidades: 10 activaciones");
    check(quant_dispatches == 3, "quantum Q=25 sobre 60 unidades: 3 activaciones");
    check(coop_dispatches != quant_dispatches,
          "el overhead de despachos si difiere entre modos");

    task_destroy(&coop);
    task_destroy(&quant);
}

int main(void)
{
    printf("Caso 6 -- Modos\n");
    test_modes_produce_same_result();

    printf("\nResultado: %d pasaron, %d fallaron.\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
