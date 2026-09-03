#include "rng.h"

int rng_init(Rng *rng, uint32_t seed) {
    if (seed == 0) {
        return -1;
    }
    rng->state = seed;
    return 0;
}

uint32_t rng_next(Rng *rng) {
    uint32_t x = rng->state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng->state = x;
    return x;
}
