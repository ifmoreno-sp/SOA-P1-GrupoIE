/* Pruebas del modulo de carga de trabajo (workload_run_units). No hay forma
 * de ejercitar esta pieza por la interfaz de linea de comandos todavia (no
 * existe scheduler hasta milestones posteriores), asi que se prueba
 * llamando directamente a la funcion. Sin framework: assert()-based, con un
 * contador de pasadas/fallos al estilo de tests/test_input_validation.sh. */

#include <stdio.h>

#include "task.h"
#include "workload.h"

/* Valor de referencia de pi para comparar la convergencia de la serie sin
 * depender de M_PI (extension no-POSIX de math.h, no expuesta bajo
 * -std=c17 con _POSIX_C_SOURCE). */
#define PI_REF 3.14159265358979323846

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

/* Corre 10 unidades de una sola vez contra correr 2, luego 3, luego 5
 * (dos reanudaciones intermedias) y compara el estado final campo a campo.
 * Si el estado no fuera realmente reanudado (por ejemplo, si se reiniciara
 * la serie en cada llamada), los resultados divergirian. */
static void test_resumption_matches_one_shot(void)
{
    Task oneshot;
    Task resumed;
    task_init(&oneshot, 1, 10, 100);
    task_init(&resumed, 1, 10, 100);

    workload_run_units(&oneshot, 10);

    workload_run_units(&resumed, 2);
    workload_run_units(&resumed, 3);
    workload_run_units(&resumed, 5);

    check(oneshot.pi_index == resumed.pi_index,
          "reanudacion (2+3+5): mismo pi_index que 10 de una vez");
    check(oneshot.completed_units == resumed.completed_units,
          "reanudacion (2+3+5): mismo completed_units que 10 de una vez");
    check(oneshot.term == resumed.term,
          "reanudacion (2+3+5): mismo term que 10 de una vez");
    check(oneshot.pi_approx == resumed.pi_approx,
          "reanudacion (2+3+5): mismo pi_approx que 10 de una vez");
}

/* Cada unidad ejecutada debe acercar pi_approx a pi sin nunca superarlo:
 * term > 0 en todo j >= 1, asi que la serie es una suma de terminos
 * positivos que converge a pi por abajo. No se exige ninguna velocidad de
 * convergencia particular (el enunciado aclara que no se califica). */
static void test_monotonic_bounded_by_pi(void)
{
    Task task;
    task_init(&task, 1, 10, 1000);

    double previous = task.pi_approx;
    int monotonic = 1;
    int bounded = 1;

    for (int i = 0; i < 1000; i++) {
        workload_run_units(&task, 1);
        if (task.pi_approx <= previous) {
            monotonic = 0;
        }
        if (task.pi_approx > PI_REF) {
            bounded = 0;
        }
        previous = task.pi_approx;
    }

    check(monotonic, "pi_approx crece estrictamente en cada unidad");
    check(bounded, "pi_approx nunca supera el valor de referencia de pi");
}

/* Reparte work_units en llamadas de tamano desigual y confirma que
 * completed_units termina exactamente en work_units, sin de mas ni de
 * menos. */
static void test_completed_units_accounting(void)
{
    Task task;
    task_init(&task, 1, 10, 1000);

    uint32_t chunks[] = {1, 99, 400, 250, 250};
    for (size_t i = 0; i < sizeof(chunks) / sizeof(chunks[0]); i++) {
        workload_run_units(&task, chunks[i]);
    }

    check(task.completed_units == task.work_units,
          "completed_units llega exactamente a work_units tras varias llamadas desiguales");
    check(task.pi_index == task.work_units,
          "pi_index avanza uno a uno con completed_units");
}

int main(void)
{
    printf("Pruebas del modulo de carga de trabajo (pi):\n");
    test_resumption_matches_one_shot();
    test_monotonic_bounded_by_pi();
    test_completed_units_accounting();

    printf("\nResultado: %d pasaron, %d fallaron.\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
