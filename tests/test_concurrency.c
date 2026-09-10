/* Prueba de integracion del nucleo de concurrencia: task.c + sync.c +
 * worker.c + workload.c trabajando juntos con hilos reales. El scheduler
 * real vive en scheduler.c (ver tests/test_scheduler.c); aqui actuamos
 * como "scheduler falso": despachamos en un orden fijo por indice, nunca
 * por loteria, para aislar el nucleo de concurrencia de la logica de
 * sorteo. Sin framework: assert()-based con contador de pasadas/fallos, al
 * estilo de tests/test_workload.c.
 *
 * El invariante de exclusion (a lo sumo una tarea RUNNING a la vez) no
 * necesita una prueba aparte: sync_dispatch lo verifica con assert() en
 * cada llamada, y si se violara este binario abortaria en vez de terminar
 * limpio. Correr este binario bajo ThreadSanitizer (make tsan) es la forma
 * real de detectar un acceso no protegido que el diseno no haya previsto. */

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include "rng.h"
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

/* Despacha, en cada ronda, la primera tarea READY por indice (nunca por
 * loteria) hasta que no quede ninguna. No es la plantilla del scheduler
 * real (ver sync_select_winner en sync.h), pero sigue sirviendo para
 * probar el nucleo de concurrencia en aislamiento de la logica de sorteo.
 * El recorrido de tasks[] se hace bajo sync->mutex directamente: ningun
 * codigo debe leer task.state sin sostener el mutex, ni siquiera este
 * scheduler de prueba. */
static void run_fake_scheduler(Sync *sync, Task *tasks, size_t count)
{
    for (;;) {
        pthread_mutex_lock(&sync->mutex);
        size_t winner = count; /* sentinela: ninguna READY encontrada */
        for (size_t i = 0; i < count; i++) {
            if (tasks[i].state == TASK_READY) {
                winner = i;
                break;
            }
        }
        pthread_mutex_unlock(&sync->mutex);

        if (winner == count) {
            return;
        }
        sync_dispatch(sync, tasks, count, winner);
        sync_wait_for_event(sync);
    }
}

/* Si worker_pool_join se queda colgado (deadlock real), este binario nunca
 * terminaria y make test se quedaria esperando para siempre sin decir por
 * que. La alarma lo convierte en un fallo explicito e inmediato en vez de
 * un cuelgue silencioso. */
static void on_join_timeout(int sig)
{
    (void)sig;
    fprintf(stderr, "FAIL - worker_pool_join no retorno a tiempo (posible deadlock)\n");
    _exit(1);
}

static void join_with_timeout(pthread_t *threads, size_t count)
{
    signal(SIGALRM, on_join_timeout);
    alarm(5);
    worker_pool_join(threads, count);
    alarm(0);
}

/* Caso simple: una sola tarea. Sirve para confirmar que el ciclo se
 * comporta bien en el caso degenerado antes de complicarlo con varias. */
static void test_single_task(void)
{
    Task tasks[1];
    task_init(&tasks[0], 1, 10, 4);

    Sync sync;
    check(sync_init(&sync) == 0, "sync_init exitoso (1 tarea)");

    pthread_t threads[1];
    WorkerArgs args[1] = {{.task = &tasks[0], .sync = &sync}};
    check(worker_pool_start(threads, args, 1) == 0, "worker_pool_start crea el hilo (1 tarea)");

    run_fake_scheduler(&sync, tasks, 1);
    join_with_timeout(threads, 1);

    check(tasks[0].state == TASK_FINISHED, "la unica tarea termina en TASK_FINISHED");
    check(tasks[0].completed_units == tasks[0].work_units, "completed_units == work_units (1 tarea)");
    check(tasks[0].dispatch_count == 1, "una sola activacion basta (placeholder temporal del worker)");

    task_destroy(&tasks[0]);
    sync_destroy(&sync);
}

/* Camino feliz con 3 tareas READY, work_units distintos. Cubre despacho,
 * espera, reanudacion del ciclo del worker, y terminacion limpia de todas. */
static void test_three_tasks(void)
{
    enum { N = 3 };
    Task tasks[N];
    task_init(&tasks[0], 1, 10, 3);
    task_init(&tasks[1], 2, 20, 5);
    task_init(&tasks[2], 3, 30, 1);

    Sync sync;
    check(sync_init(&sync) == 0, "sync_init exitoso (3 tareas)");

    pthread_t threads[N];
    WorkerArgs args[N];
    for (int i = 0; i < N; i++) {
        args[i].task = &tasks[i];
        args[i].sync = &sync;
    }
    check(worker_pool_start(threads, args, N) == 0, "worker_pool_start crea los 3 hilos");

    run_fake_scheduler(&sync, tasks, N);
    join_with_timeout(threads, N);
    check(1, "worker_pool_join retorna sin bloquear (no deadlock)");

    for (int i = 0; i < N; i++) {
        check(tasks[i].state == TASK_FINISHED, "cada tarea termina en TASK_FINISHED");
        check(tasks[i].completed_units == tasks[i].work_units, "completed_units == work_units al terminar");
        check(tasks[i].dispatch_count == 1, "una sola activacion basta (placeholder temporal del worker)");
        task_destroy(&tasks[i]);
    }
    sync_destroy(&sync);
}

/* --- Pruebas de sync_select_winner: el sorteo bajo mutex --- */

/* Con una sola tarea TASK_READY, siempre debe ganar ella. */
static void test_select_winner_single_ready(void)
{
    Task tasks[1];
    task_init(&tasks[0], 7, 42, 100);

    Sync sync;
    check(sync_init(&sync) == 0, "sync_init exitoso (select_winner, 1 tarea)");
    Rng rng;
    check(rng_init(&rng, 2026) == 0, "rng_init exitoso (select_winner, 1 tarea)");

    Selection sel = sync_select_winner(&sync, tasks, 1, &rng);
    check(sel.index == 0, "unica tarea READY siempre gana");
    check(sel.active_tickets == 42, "active_tickets es el total de la unica tarea");
    check(sel.winning_ticket >= 1 && sel.winning_ticket <= 42,
          "winning_ticket cae en [1, active_tickets]");

    task_destroy(&tasks[0]);
    sync_destroy(&sync);
}

/* Sin ninguna tarea TASK_READY, debe retornar el sentinela (index ==
 * task_count). */
static void test_select_winner_no_ready_tasks(void)
{
    Task tasks[2];
    task_init(&tasks[0], 1, 10, 5);
    task_init(&tasks[1], 2, 10, 5);
    tasks[0].state = TASK_FINISHED;
    tasks[1].state = TASK_FINISHED;

    Sync sync;
    check(sync_init(&sync) == 0, "sync_init exitoso (select_winner, sin READY)");
    Rng rng;
    check(rng_init(&rng, 2026) == 0, "rng_init exitoso (select_winner, sin READY)");

    Selection sel = sync_select_winner(&sync, tasks, 2, &rng);
    check(sel.index == 2, "sin tareas READY, retorna el sentinela task_count");

    task_destroy(&tasks[0]);
    task_destroy(&tasks[1]);
    sync_destroy(&sync);
}

/* Con tareas FINISHED intercaladas, sync_select_winner debe ignorarlas por
 * completo: ni suman boletos ni pueden ganar. */
static void test_select_winner_skips_non_ready(void)
{
    Task tasks[3];
    task_init(&tasks[0], 1, 1000, 5); /* muchos boletos, pero FINISHED */
    task_init(&tasks[1], 2, 10, 5);   /* la unica READY */
    task_init(&tasks[2], 3, 1000, 5); /* muchos boletos, pero FINISHED */
    tasks[0].state = TASK_FINISHED;
    tasks[2].state = TASK_FINISHED;

    Sync sync;
    check(sync_init(&sync) == 0, "sync_init exitoso (select_winner, mezcla de estados)");
    Rng rng;
    check(rng_init(&rng, 99) == 0, "rng_init exitoso (select_winner, mezcla de estados)");

    Selection sel = sync_select_winner(&sync, tasks, 3, &rng);
    check(sel.index == 1, "la unica tarea READY gana, sin importar boletos de las FINISHED");
    check(sel.active_tickets == 10, "active_tickets ignora boletos de tareas no-READY");

    for (int i = 0; i < 3; i++) {
        task_destroy(&tasks[i]);
    }
    sync_destroy(&sync);
}

/* Prueba estadistica: con boletos muy desiguales (10 vs 90 de 100
 * activos), la tarea con mas boletos debe ganar el sorteo bastante mas
 * seguido a largo plazo. Se usa una banda amplia (no un valor exacto)
 * porque el sesgo leve de la operacion modulo en rng_draw_ticket (ver
 * rng.c) y el tamano finito de la muestra hacen fragil pedir precision
 * exacta. */
static void test_select_winner_weighted_distribution(void)
{
    enum { TRIALS = 4000 };
    int wins_high_tickets = 0;

    for (uint32_t seed = 1; seed <= TRIALS; seed++) {
        Task tasks[2];
        task_init(&tasks[0], 1, 10, 5); /* pocos boletos */
        task_init(&tasks[1], 2, 90, 5); /* muchos boletos */

        Sync sync;
        sync_init(&sync);
        Rng rng;
        rng_init(&rng, seed);

        Selection sel = sync_select_winner(&sync, tasks, 2, &rng);
        if (sel.index == 1) {
            wins_high_tickets++;
        }

        task_destroy(&tasks[0]);
        task_destroy(&tasks[1]);
        sync_destroy(&sync);
    }

    double ratio = (double)wins_high_tickets / TRIALS;
    check(ratio > 0.85 && ratio < 0.95,
          "tarea con 90% de los boletos gana aproximadamente 90% de los sorteos (4000 semillas)");
}

int main(void)
{
    printf("Pruebas de integracion del nucleo de concurrencia:\n");
    test_single_task();
    test_three_tasks();

    printf("\nPruebas de sync_select_winner:\n");
    test_select_winner_single_ready();
    test_select_winner_no_ready_tasks();
    test_select_winner_skips_non_ready();
    test_select_winner_weighted_distribution();

    printf("\nResultado: %d pasaron, %d fallaron.\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
