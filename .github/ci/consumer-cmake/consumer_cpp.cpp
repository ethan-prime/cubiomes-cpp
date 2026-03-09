#include "generator.hpp"
#include <cstdint>

int main()
{
    Generator g;
    setupGenerator(&g, MC_1_20, 0);
    applySeed(&g, DIM_OVERWORLD, static_cast<uint64_t>(54321));
    return getBiomeAt(&g, 1, 0, 63, 0) < 0;
}
