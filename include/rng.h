#ifndef RNG_H
#define RNG_H

#include <stdint.h>

/* Estado del generador xorshift32. Un valor por instancia, sin estado global. */
typedef struct {
    uint32_t state;
} Rng;

/*
 * Inicializa rng con la semilla dada.
 *
 * Precondición: seed != 0 (con semilla 0, xorshift32 queda atascado
 * generando siempre 0).
 * Efecto sobre estado compartido: ninguno; rng es privado a quien lo posea.
 * Sincronización: esta función no es thread-safe por sí misma. Si el mismo
 * Rng se comparte entre hilos, el llamador debe serializar el acceso (en
 * este proyecto, solo el scheduler sortea boletos, protegido por el mutex
 * global).
 *
 * Retorna 0 en éxito, o -1 si seed == 0 (rng no se modifica).
 */
int rng_init(Rng *rng, uint32_t seed);

/*
 * Avanza el estado con la secuencia exacta que exige el enunciado
 * (x ^= x << 13; x ^= x >> 17; x ^= x << 5;) y retorna el nuevo valor.
 *
 * Precondición: rng fue inicializado con rng_init.
 * Efecto: muta rng->state.
 * Sincronización: igual que rng_init.
 */
uint32_t rng_next(Rng *rng);

/*
 * Sortea el boleto ganador en [1, active_tickets]: (rng_next(rng) %
 * active_tickets) + 1.
 *
 * Precondición: active_tickets >= 1, rng inicializado.
 * Efecto: avanza rng->state (vía rng_next).
 * Sincronización: igual que rng_init.
 *
 * Nota de sesgo: la operación módulo introduce un sesgo leve hacia los
 * boletos bajos cuando active_tickets no divide exactamente 2^32. Se
 * acepta y documenta (no se corrige con rejection sampling): el sesgo es
 * despreciable porque active_tickets es órdenes de magnitud menor que el
 * rango de x (uint32_t, hasta 2^32).
 */
uint32_t rng_draw_ticket(Rng *rng, uint32_t active_tickets);

#endif
