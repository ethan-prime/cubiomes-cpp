#include "generator.hpp"
#include <stdint.h>

int main(void)
{
    Generator g;
    setupGenerator(&g, MC_1_20, 0);
    applySeed(&g, DIM_OVERWORLD, (uint64_t)12345);
    return getBiomeAt(&g, 1, 0, 63, 0) < 0;
}
