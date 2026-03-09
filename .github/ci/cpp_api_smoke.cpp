#include "generator.hpp"
#include <cstdint>

int main()
{
    Generator g;
    setupGenerator(&g, MC_1_20, 0);
    applySeed(&g, DIM_OVERWORLD, static_cast<uint64_t>(12345));

    const int id = getBiomeAt(&g, 1, 0, 63, 0);
    return (id < 0) ? 1 : 0;
}
