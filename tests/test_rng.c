#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#include "rng.h"

/* seed=0 es el único punto fijo de xorshift32 (x=0 se queda en 0 para
 * siempre), así que rng_init debe rechazarlo explícitamente. */
static void test_seed_zero_rejected(void) {
    Rng r;
    assert(rng_init(&r, 0) == -1);
    printf("OK: seed 0 rechazada\n");
}

/* Cualquier semilla != 0 es válida, incluyendo los extremos del rango
 * representable (1 y UINT32_MAX). */
static void test_seed_nonzero_accepted(void) {
    Rng r;
    assert(rng_init(&r, 1) == 0);
    assert(rng_init(&r, 2026) == 0);
    assert(rng_init(&r, 0xFFFFFFFFu) == 0);
    printf("OK: semillas != 0 aceptadas\n");
}

/*
 * Secuencia esperada para seed=1, calculada corriendo rng_next sobre esta
 * misma implementación. Sirve como prueba de regresión: si alguien cambia
 * el orden o los montos de los shifts, esta prueba falla aunque el código
 * siga compilando.
 */
static void test_known_sequence(void) {
    static const uint32_t expected[5] = {
        270369u, 67634689u, 2647435461u, 307599695u, 2398689233u
    };
    Rng r;
    rng_init(&r, 1);
    for (int i = 0; i < 5; i++) {
        uint32_t got = rng_next(&r);
        assert(got == expected[i]);
    }
    printf("OK: secuencia conocida para seed=1\n");
}

/* El enunciado exige que la misma semilla reproduzca exactamente el mismo
 * registro de decisiones; esto lo verifica al nivel del RNG, aislado del
 * resto del programa: dos instancias independientes con la misma semilla
 * deben coincidir valor a valor. */
static void test_determinism(void) {
    Rng a, b;
    rng_init(&a, 2026);
    rng_init(&b, 2026);
    for (int i = 0; i < 1000; i++) {
        assert(rng_next(&a) == rng_next(&b));
    }
    printf("OK: misma semilla produce la misma secuencia\n");
}

/* rng_draw_ticket nunca debe devolver un boleto fuera de rango. Se prueba
 * con varios tamaños de active_tickets (incluyendo uno grande, cercano al
 * orden de magnitud donde importaría un error de límites) y muchos sorteos
 * por caso para tener chance real de atrapar un off-by-one. */
static void test_draw_ticket_range(void) {
    static const uint32_t active_tickets_cases[] = {1u, 2u, 3u, 60u, 1000000u};
    for (size_t c = 0; c < sizeof(active_tickets_cases) / sizeof(active_tickets_cases[0]); c++) {
        uint32_t active_tickets = active_tickets_cases[c];
        Rng r;
        rng_init(&r, 42u + (uint32_t)c);
        for (int i = 0; i < 10000; i++) {
            uint32_t boleto = rng_draw_ticket(&r, active_tickets);
            assert(boleto >= 1 && boleto <= active_tickets);
        }
    }
    printf("OK: boleto siempre en [1, active_tickets]\n");
}

/* Caso borde: con un solo boleto en juego, todo x % 1 da 0, así que el
 * resultado (con el +1) debe ser siempre el boleto 1, sin excepción. */
static void test_draw_ticket_single_ticket(void) {
    Rng r;
    rng_init(&r, 7);
    for (int i = 0; i < 100; i++) {
        assert(rng_draw_ticket(&r, 1) == 1);
    }
    printf("OK: active_tickets=1 siempre gana el boleto 1\n");
}

int main(void) {
    test_seed_zero_rejected();
    test_seed_nonzero_accepted();
    test_known_sequence();
    test_determinism();
    test_draw_ticket_range();
    test_draw_ticket_single_ticket();
    printf("Todas las pruebas de rng pasaron.\n");
    return 0;
}
