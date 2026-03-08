#include "generator.h"
#include <stdint.h>

int main(void)
{
    Generator g;
    setupGenerator(&g, MC_1_20, 0);
    applySeed(&g, DIM_OVERWORLD, (uint64_t)12345);

    int id = getBiomeAt(&g, 1, 0, 63, 0);
    return (id < 0) ? 1 : 0;
}
