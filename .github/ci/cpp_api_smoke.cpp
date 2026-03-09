#include "generator.hpp"
#include <cstdint>

int main()
{
    auto engine = cubiomes::cpp::GeneratorEngine(MC_1_20, 0U);
    engine.apply_seed(DIM_OVERWORLD, static_cast<std::uint64_t>(12345));
    const auto id = engine.biome_at(1, 0, 63, 0);
    return (id < 0) ? 1 : 0;
}
