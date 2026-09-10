/* Pruebas de los modos de ejecucion (M6): el corte cooperativo/quantum
 * dentro del ciclo del worker. Igual que en test_concurrency.c (M4), aqui
 * no existe scheduler real todavia (es M5), asi que se usa un "scheduler
 * falso" que despacha en orden fijo por indice, nunca por loteria.
 *
 * Dos niveles de prueba:
 *   1. cooperative_slice_size directo, sin hilos: la aritmetica del ceil.
 *   2. El comportamiento observable de cada modo con hilos reales, mas la
 *      equivalencia funcional entre ambos (caso de prueba 6 del enunciado). */

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
 * vez de dejar make test esperando para siempre. */
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

/* La aritmetica del bloque cooperativo, sin hilos de por medio. El
 * enunciado exige ceil(work_units * P / 100) con minimo una unidad. */
static void test_cooperative_slice_size(void)
{
    check(cooperative_slice_size(100, 10) == 10, "ceil(100*10/100) = 10");
    check(cooperative_slice_size(100, 100) == 100, "P=100 corre todo de una vez");
    check(cooperative_slice_size(1, 1) == 1, "work_units=1, P=1 da 1 (nunca 0)");
    check(cooperative_slice_size(3, 50) == 2, "ceil(3*50/100) = ceil(1.5) = 2, redondea hacia arriba");
    check(cooperative_slice_size(10, 33) == 4, "ceil(10*33/100) = ceil(3.3) = 4");
    check(cooperative_slice_size(1000, 1) == 10, "ceil(1000*1/100) = 10");
    /* Caso grande del experimento de proporcionalidad del enunciado. */
    check(cooperative_slice_size(10000000, 10) == 1000000, "10M unidades al 10% da 1M por bloque");
}

/* En modo quantum el bloque es Q fijo, igual para toda tarea: una tarea de
 * 10 unidades con Q=3 necesita ceil(10/3) = 4 activaciones. */
static void test_quantum_mode_dispatches(void)
{
    Task task;
    task_init(&task, 1, 10, 10);
    uint32_t dispatches = run_one_task(&task, MODE_QUANTUM, 3, 0);

    check(dispatches == 4, "quantum Q=3 sobre 10 unidades: 4 activaciones (3+3+3+1)");
    check(task.state == TASK_FINISHED, "la tarea termina en TASK_FINISHED (quantum)");
    check(task.completed_units == task.work_units, "completed_units == work_units (quantum)");
    task_destroy(&task);
}

/* En cooperativo el bloque es proporcional al total de la tarea: al 25%,
 * cualquier tarea necesita exactamente 4 activaciones sin importar su
 * tamano. Esta es la diferencia de fondo con quantum. */
static void test_cooperative_mode_dispatches(void)
{
    Task small;
    Task large;
    task_init(&small, 1, 10, 8);
    task_init(&large, 2, 10, 400);

    uint32_t small_dispatches = run_one_task(&small, MODE_COOPERATIVE, 0, 25);
    uint32_t large_dispatches = run_one_task(&large, MODE_COOPERATIVE, 0, 25);

    check(small_dispatches == 4, "cooperativo 25% sobre 8 unidades: 4 activaciones");
    check(large_dispatches == 4, "cooperativo 25% sobre 400 unidades: tambien 4 activaciones");
    check(small_dispatches == large_dispatches,
          "el numero de activaciones no depende del tamano de la tarea (cooperativo)");
    check(small.completed_units == small.work_units && large.completed_units == large.work_units,
          "ambas tareas completan todo su trabajo (cooperativo)");

    task_destroy(&small);
    task_destroy(&large);
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

/* Varias tareas compitiendo, para confirmar que el corte por modo tambien
 * funciona cuando el scheduler alterna entre tareas distintas. */
static void test_multiple_tasks_quantum(void)
{
    enum { N = 3 };
    Task tasks[N];
    task_init(&tasks[0], 1, 10, 7);
    task_init(&tasks[1], 2, 20, 4);
    task_init(&tasks[2], 3, 30, 10);

    Sync sync;
    check(sync_init(&sync) == 0, "sync_init exitoso (3 tareas, quantum)");

    pthread_t threads[N];
    WorkerArgs args[N];
    for (int i = 0; i < N; i++) {
        args[i].task = &tasks[i];
        args[i].sync = &sync;
        args[i].mode = MODE_QUANTUM;
        args[i].quantum = 2;
        args[i].slice_percent = 0;
    }
    check(worker_pool_start(threads, args, N) == 0, "worker_pool_start crea los 3 hilos (quantum)");

    run_fake_scheduler(&sync, tasks, N);
    join_with_timeout(threads, N);

    /* Q=2: la de 7 unidades necesita 4 activaciones (2+2+2+1), la de 4
     * necesita 2, y la de 10 necesita 5. */
    check(tasks[0].dispatch_count == 4, "7 unidades con Q=2: 4 activaciones");
    check(tasks[1].dispatch_count == 2, "4 unidades con Q=2: 2 activaciones");
    check(tasks[2].dispatch_count == 5, "10 unidades con Q=2: 5 activaciones");

    for (int i = 0; i < N; i++) {
        check(tasks[i].state == TASK_FINISHED, "cada tarea termina (multiples tareas, quantum)");
        check(tasks[i].completed_units == tasks[i].work_units,
              "cada tarea completa su trabajo (multiples tareas, quantum)");
        task_destroy(&tasks[i]);
    }
    sync_destroy(&sync);
}

int main(void)
{
    printf("Pruebas de los modos de ejecucion:\n");
    test_cooperative_slice_size();
    test_quantum_mode_dispatches();
    test_cooperative_mode_dispatches();
    test_modes_produce_same_result();
    test_multiple_tasks_quantum();

    printf("\nResultado: %d pasaron, %d fallaron.\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
