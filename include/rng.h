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

#endif
