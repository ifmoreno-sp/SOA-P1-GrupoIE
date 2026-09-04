/* Prueba de integracion del nucleo de concurrencia (M4): task.c + sync.c +
 * worker.c + workload.c trabajando juntos con hilos reales. Todavia no
 * existe scheduler (es M5), asi que aqui actuamos como "scheduler falso":
 * despachamos en un orden fijo por indice, nunca por loteria. Sin
 * framework: assert()-based con contador de pasadas/fallos, al estilo de
 * tests/test_workload.c.
 *
 * El invariante de exclusion (a lo sumo una tarea RUNNING a la vez) no
 * necesita una prueba aparte: sync_dispatch lo verifica con assert() en
 * cada llamada, y si se violara este binario abortaria en vez de terminar
 * limpio. Correr este binario bajo ThreadSanitizer (make test-concurrency
 * con -fsanitize=thread agregado a mano, o ver el comando manual mas abajo)
 * es la forma real de detectar un acceso no protegido que el diseno no
 * haya previsto. */

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

/* Despacha, en cada ronda, la primera tarea READY por indice (nunca por
 * loteria) hasta que no quede ninguna. Sustituye al scheduler real de M5. */
static void run_fake_scheduler(Sync *sync, Task *tasks, size_t count)
{
    for (;;) {
        size_t winner = count; /* sentinela: ninguna READY encontrada */
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
    check(tasks[0].dispatch_count == 1, "una sola activacion basta (placeholder de M4)");

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
        check(tasks[i].dispatch_count == 1, "una sola activacion basta (placeholder de M4)");
        task_destroy(&tasks[i]);
    }
    sync_destroy(&sync);
}

int main(void)
{
    printf("Pruebas de integracion del nucleo de concurrencia:\n");
    test_single_task();
    test_three_tasks();

    printf("\nResultado: %d pasaron, %d fallaron.\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
