/* Pruebas de los modos de ejecucion (M6): el corte cooperativo/quantum
 * dentro del ciclo del worker. El scheduler real ya existe (scheduler.c,
 * M5), pero aqui se usa a proposito un "scheduler falso" que despacha en
 * orden fijo por indice, nunca por loteria -- para aislar el corte por
 * modo de la logica de sorteo, igual que test_concurrency.c hace con el
 * nucleo de concurrencia.
 *
 * Dos niveles de prueba (el tercero, la equivalencia funcional entre
 * modos que pide el Caso 6 del enunciado, vive en
 * scripts/casos_enunciado/caso6_modos.c):
 *   1. cooperative_slice_size directo, sin hilos: la aritmetica del ceil.
 *   2. El comportamiento observable de cada modo con hilos reales, mas
 *      casos borde de quantum (Q mayor/igual/menor al trabajo) y la
 *      verificacion directa de que una tarea vuelve a TASK_READY a mitad
 *      de camino, no solo inferida del dispatch_count final. */

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

/* Q mayor que work_units: min(Q, restante) debe tomar "restante", asi que
 * la tarea termina en una sola activacion pese a estar en modo quantum
 * (no por casualidad heredada de cooperativo). */
static void test_quantum_larger_than_work(void)
{
    Task task;
    task_init(&task, 1, 10, 5);
    uint32_t dispatches = run_one_task(&task, MODE_QUANTUM, 1000, 0);

    check(dispatches == 1, "Q=1000 sobre 5 unidades: 1 sola activacion");
    check(task.state == TASK_FINISHED, "termina en TASK_FINISHED (Q > work_units)");
    check(task.completed_units == task.work_units, "completed_units == work_units (Q > work_units)");
    task_destroy(&task);
}

/* Q exactamente igual al trabajo total: tambien 1 activacion, sin sobrar
 * ni faltar nada (el borde entre "una sola vez" y "necesita una segunda"). */
static void test_quantum_exact_multiple(void)
{
    Task task;
    task_init(&task, 1, 10, 20);
    uint32_t dispatches = run_one_task(&task, MODE_QUANTUM, 20, 0);

    check(dispatches == 1, "Q=20 sobre 20 unidades: exactamente 1 activacion");
    check(task.completed_units == task.work_units, "completed_units == work_units (Q == work_units)");
    task_destroy(&task);
}

/* Q=1: el peor caso de overhead, una activacion por unidad. */
static void test_quantum_minimum(void)
{
    Task task;
    task_init(&task, 1, 10, 6);
    uint32_t dispatches = run_one_task(&task, MODE_QUANTUM, 1, 0);

    check(dispatches == 6, "Q=1 sobre 6 unidades: 6 activaciones (una por unidad)");
    check(task.completed_units == task.work_units, "completed_units == work_units (Q=1)");
    task_destroy(&task);
}

/* Despacha manualmente UNA sola vez (sin dejar que el scheduler falso siga
 * el ciclo hasta el final) y verifica el estado justo despues: con Q menor
 * al trabajo total, la tarea debe quedar en TASK_READY, no en
 * TASK_FINISHED. Las demas pruebas de este archivo solo infieren la
 * expropiacion del dispatch_count final; esta la observa directamente. */
static void test_quantum_yields_to_ready_after_one_dispatch(void)
{
    Task task;
    task_init(&task, 1, 10, 10);

    Sync sync;
    check(sync_init(&sync) == 0, "sync_init exitoso (expropiacion intermedia)");

    pthread_t thread;
    WorkerArgs args = {.task = &task, .sync = &sync, .mode = MODE_QUANTUM, .quantum = 4};
    check(worker_pool_start(&thread, &args, 1) == 0,
          "worker_pool_start crea el hilo (expropiacion intermedia)");

    /* Un solo despacho manual, sin el bucle de run_fake_scheduler. */
    sync_dispatch(&sync, &task, 1, 0);
    sync_wait_for_event(&sync);

    check(task.state == TASK_READY,
          "tras UNA activacion con Q=4 < work_units=10, la tarea vuelve a TASK_READY (no FINISHED)");
    check(task.completed_units == 4, "completed_units == 4 tras la primera activacion (Q=4)");
    check(task.dispatch_count == 1, "dispatch_count == 1 tras la primera activacion");

    /* Se deja terminar el ciclo para poder unir el hilo limpiamente. */
    run_fake_scheduler(&sync, &task, 1);
    join_with_timeout(&thread, 1);

    check(task.state == TASK_FINISHED, "la tarea termina en TASK_FINISHED al final");
    check(task.dispatch_count == 3, "Q=4 sobre 10 unidades: 3 activaciones en total (4+4+2)");

    task_destroy(&task);
    sync_destroy(&sync);
}

int main(void)
{
    printf("Pruebas de los modos de ejecucion:\n");
    test_cooperative_slice_size();
    test_quantum_mode_dispatches();
    test_cooperative_mode_dispatches();
    test_multiple_tasks_quantum();
    test_quantum_larger_than_work();
    test_quantum_exact_multiple();
    test_quantum_minimum();
    test_quantum_yields_to_ready_after_one_dispatch();

    printf("\nResultado: %d pasaron, %d fallaron.\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
