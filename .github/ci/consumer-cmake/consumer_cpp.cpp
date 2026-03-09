#include "generator.hpp"
#include <cstdint>

int main()
{
    auto gen = cubiomes::cpp::GeneratorEngine(MC_1_20, 0U);
    gen.apply_seed(DIM_OVERWORLD, static_cast<std::uint64_t>(54321));
    return gen.biome_at(1, 0, 63, 0) < 0;
}
