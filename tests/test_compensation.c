/* Pruebas de la extension M9 (compensation tickets): la cesion temprana
 * forzada y la actualizacion de debt/effective_tickets en worker.c, mas la
 * regla del sorteo que ya se probo por separado en test_concurrency.c
 * (test_select_winner_uses_effective_tickets).
 *
 * Mismo enfoque que test_execution_modes.c: se despacha manualmente
 * dispatch por dispatch (sync_dispatch + sync_wait_for_event, sin el bucle
 * completo del scheduler falso) para poder verificar el estado exacto de
 * debt/effective_tickets/completed_units despues de CADA activacion, no
 * solo el resultado final -- la aritmetica de compensacion es la parte
 * nueva y mas propensa a errores de este milestone. */

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

/* task_init deja el comportamiento base intacto: sin --yield-config,
 * effective_tickets arranca igual a tickets y has_yield en 0. Sin esto, el
 * resto de la extension no tendria una linea base contra la cual medir el
 * "sin cambios respecto a antes de M9" que ya verifican test_concurrency.c
 * y test_execution_modes.c (no vuelven a probarlo aca). */
static void test_task_init_leaves_compensation_disabled(void)
{
    Task task;
    task_init(&task, 1, 42, 100);

    check(task.effective_tickets == 42, "effective_tickets arranca igual a tickets");
    check(task.debt == 0, "debt arranca en 0");
    check(task.has_yield == 0, "has_yield arranca desactivado");

    task_destroy(&task);
}

/* Ejemplo exacto del documento de diseno (extension_compensation_tickets.md):
 * Q=10, tickets_base=20, cede tras 40% -> corre 4, tickets_compensados=50.
 * Se verifica dispatch por dispatch, incluyendo el ciclo completo:
 * cede (4) -> compensa (10, deuda a 0, tickets vuelven a base) -> vuelve a
 * ceder (4) -> compensa (10) -- el comportamiento "persistente" descrito en
 * el spec, no solo un evento unico. */
static void test_quantum_yield_and_compensation_cycle(void)
{
    Task task;
    task_init(&task, 1, 20, 100);
    task_set_yield_config(&task, 40);

    Sync sync;
    check(sync_init(&sync) == 0, "sync_init exitoso (compensacion, quantum)");

    pthread_t thread;
    WorkerArgs args = {.task = &task, .sync = &sync, .mode = MODE_QUANTUM, .quantum = 10};
    check(worker_pool_start(&thread, &args, 1) == 0, "worker_pool_start crea el hilo (compensacion, quantum)");

    sync_dispatch(&sync, &task, 1, 0);
    sync_wait_for_event(&sync);
    check(task.completed_units == 4, "1a activacion: cede tras 4/10 (f=0.4)");
    check(task.debt == 6, "deuda tras ceder: 10 - 4 = 6");
    check(task.effective_tickets == 50, "tickets compensados: ceil(20*10/4) = 50");
    check(task.state == TASK_READY, "sigue TASK_READY tras ceder");

    sync_dispatch(&sync, &task, 1, 0);
    sync_wait_for_event(&sync);
    check(task.completed_units == 14, "2a activacion: corre el bloque completo (10) mientras compensa");
    check(task.debt == 0, "deuda saldada: 10 >= 6 pendientes");
    check(task.effective_tickets == 20, "tickets vuelven a la base tras saldar la deuda");

    sync_dispatch(&sync, &task, 1, 0);
    sync_wait_for_event(&sync);
    check(task.completed_units == 18, "3a activacion: sin deuda, vuelve a ceder temprano (4 mas)");
    check(task.debt == 6, "el ciclo de compensacion se repite: misma deuda que la 1a vez");
    check(task.effective_tickets == 50, "mismos tickets compensados que la 1a vez");

    sync_dispatch(&sync, &task, 1, 0);
    sync_wait_for_event(&sync);
    check(task.completed_units == 28, "4a activacion: vuelve a compensar el bloque completo");
    check(task.debt == 0, "deuda saldada de nuevo");
    check(task.effective_tickets == 20, "tickets de vuelta a la base, 2do ciclo completo");

    /* Deja correr el resto para poder unir el hilo limpiamente; no se
     * afirma un conteo exacto de despachos porque el ultimo bloque queda
     * recortado por lo que falta (interaccion ya cubierta por
     * test_quantum_larger_than_work en test_execution_modes.c). */
    run_fake_scheduler(&sync, &task, 1);
    join_with_timeout(&thread, 1);

    check(task.state == TASK_FINISHED, "la tarea termina en TASK_FINISHED pese a ceder repetidamente");
    check(task.completed_units == task.work_units, "completed_units == work_units al terminar");

    task_destroy(&task);
    sync_destroy(&sync);
}

/* Misma mecanica que la prueba anterior, pero en modo cooperativo: confirma
 * que la compensacion se mide contra el bloque que decide el modo en esa
 * activacion (cooperative_slice_size, ya recortado por lo que falta), no
 * contra un quantum fijo -- el generalizacion que motivo el diseno de esta
 * extension sobre M6. Elegido para que la deuda quede exactamente en 0 al
 * mismo tiempo que la tarea termina (sin fragmento residual). */
static void test_cooperative_yield_and_compensation_cycle(void)
{
    Task task;
    task_init(&task, 1, 30, 20);
    task_set_yield_config(&task, 25);

    Sync sync;
    check(sync_init(&sync) == 0, "sync_init exitoso (compensacion, cooperativo)");

    pthread_t thread;
    WorkerArgs args = {.task = &task, .sync = &sync, .mode = MODE_COOPERATIVE, .slice_percent = 50};
    check(worker_pool_start(&thread, &args, 1) == 0,
          "worker_pool_start crea el hilo (compensacion, cooperativo)");

    /* Bloque cooperativo = ceil(20*50/100) = 10 unidades por activacion
     * normal. */
    sync_dispatch(&sync, &task, 1, 0);
    sync_wait_for_event(&sync);
    check(task.completed_units == 3, "1a activacion: cede tras ceil(10*25/100)=3 de 10");
    check(task.debt == 7, "deuda tras ceder: 10 - 3 = 7");
    check(task.effective_tickets == 100, "tickets compensados: ceil(30*10/3) = 100");

    sync_dispatch(&sync, &task, 1, 0);
    sync_wait_for_event(&sync);
    check(task.completed_units == 13, "2a activacion: corre el bloque completo (10) mientras compensa");
    check(task.debt == 0, "deuda saldada: 10 >= 7 pendientes");
    check(task.effective_tickets == 30, "tickets vuelven a la base");

    /* Restan 7 unidades: el bloque cooperativo recortado por lo que falta
     * es min(10, 7) = 7, no 10 -- por eso la fraccion se mide contra 7. */
    sync_dispatch(&sync, &task, 1, 0);
    sync_wait_for_event(&sync);
    check(task.completed_units == 15, "3a activacion: cede tras ceil(7*25/100)=2 de 7 (bloque recortado)");
    check(task.debt == 5, "deuda tras ceder: 7 - 2 = 5");
    check(task.effective_tickets == 105, "tickets compensados: ceil(30*7/2) = 105");

    sync_dispatch(&sync, &task, 1, 0);
    sync_wait_for_event(&sync);
    check(task.completed_units == 20, "4a activacion: corre las 5 restantes, deuda llega a 0 justo al terminar");
    check(task.debt == 0, "deuda saldada exactamente al completar el trabajo");
    check(task.effective_tickets == 30, "tickets de vuelta a la base al finalizar");
    check(task.state == TASK_FINISHED, "la tarea termina en TASK_FINISHED");

    join_with_timeout(&thread, 1);
    task_destroy(&task);
    sync_destroy(&sync);
}

/* Caso borde: bloque tan chico que ceil(block * yield_percent / 100) no
 * deja una cesion real (el redondeo lo iguala al bloque completo). Debe
 * correr normal, sin activar compensacion -- no hay nada que ceder. */
static void test_yield_skipped_when_block_too_small(void)
{
    Task task;
    task_init(&task, 1, 10, 5);
    task_set_yield_config(&task, 99); /* ceil(1*99/100) = 1 == bloque: sin cesion real */

    Sync sync;
    check(sync_init(&sync) == 0, "sync_init exitoso (bloque minimo)");

    pthread_t thread;
    WorkerArgs args = {.task = &task, .sync = &sync, .mode = MODE_QUANTUM, .quantum = 1};
    check(worker_pool_start(&thread, &args, 1) == 0, "worker_pool_start crea el hilo (bloque minimo)");

    sync_dispatch(&sync, &task, 1, 0);
    sync_wait_for_event(&sync);
    check(task.completed_units == 1, "con Q=1 corre la unica unidad posible, sin ceder");
    check(task.debt == 0, "no se activa compensacion cuando el bloque no admite cesion real");
    check(task.effective_tickets == 10, "effective_tickets se mantiene en la base");

    run_fake_scheduler(&sync, &task, 1);
    join_with_timeout(&thread, 1);

    check(task.state == TASK_FINISHED, "la tarea termina normalmente");
    task_destroy(&task);
    sync_destroy(&sync);
}

int main(void)
{
    printf("Pruebas de la extension M9 (compensation tickets):\n");
    test_task_init_leaves_compensation_disabled();
    test_quantum_yield_and_compensation_cycle();
    test_cooperative_yield_and_compensation_cycle();
    test_yield_skipped_when_block_too_small();

    printf("\nResultado: %d pasaron, %d fallaron.\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
