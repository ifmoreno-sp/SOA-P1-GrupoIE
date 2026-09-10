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

/* Valor absoluto local para no depender de libm (fabs) por una sola
 * comparacion. */
static double abs_diff(double a, double b)
{
    double d = a - b;
    return d < 0.0 ? -d : d;
}

/* Compara pi_approx contra un valor de referencia calculado por un camino
 * independiente de la recurrencia multiplicativa que usa workload_run_units
 * (a diferencia de las pruebas anteriores, que solo verifican propiedades
 * como monotonia sin fijar un valor esperado).
 *
 * El termino j de esta serie tiene una forma cerrada equivalente a la
 * recurrencia: a_j = C(2j,j) / (4^j * (2j+1)), con pi_approx = 2 + 2*sum(a_j).
 * Es el mismo termino de la serie de Taylor de arcsin(1) evaluada en 1.
 *
 * Caso N=1, verificable a mano: a_1 = C(2,1)/(4*3) = 2/12 = 1/6, entonces
 * pi_approx = 2 + 2*(1/6) = 7/3.
 *
 * Caso N=10: 2.8001699635058102, calculado aparte evaluando la formula
 * cerrada con la misma aritmetica de punto flotante (sin usar la
 * recurrencia bajo prueba), y verificado independiente de este repo. */
static void test_matches_known_value(void)
{
    Task task;
    task_init(&task, 1, 10, 100);

    workload_run_units(&task, 1);
    double expected_1 = 2.0 + 2.0 * (1.0 / 6.0);
    check(abs_diff(task.pi_approx, expected_1) < 1e-12,
          "pi_approx tras 1 unidad coincide con el valor calculado a mano (7/3)");

    workload_run_units(&task, 9);
    double expected_10 = 2.8001699635058102;
    check(abs_diff(task.pi_approx, expected_10) < 1e-12,
          "pi_approx tras 10 unidades coincide con el valor de referencia (formula cerrada)");
}

int main(void)
{
    printf("Pruebas del modulo de carga de trabajo (pi):\n");
    test_resumption_matches_one_shot();
    test_monotonic_bounded_by_pi();
    test_completed_units_accounting();
    test_matches_known_value();

    printf("\nResultado: %d pasaron, %d fallaron.\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
