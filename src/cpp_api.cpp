#include "cpp_api.hpp"

#include <stdexcept>
#include <utility>

namespace cubiomes::cpp {

BiomeGenerator::BiomeGenerator(int mc, uint32_t flags) : engine_(mc, flags)
{
}

void BiomeGenerator::reset(int mc, uint32_t flags)
{
    engine_.reset(mc, flags);
}

void BiomeGenerator::apply_seed(Dimension dim, uint64_t seed)
{
    engine_.apply_seed(static_cast<std::int32_t>(dim), seed);
}

int BiomeGenerator::biome_at(int scale, int x, int y, int z) const
{
    return engine_.biome_at(scale, x, y, z);
}

int BiomeGenerator::biome_at_block(int x, int y, int z) const
{
    return biome_at(1, x, y, z);
}

std::vector<int> BiomeGenerator::generate(Range r) const
{
    if (r.sx <= 0 || r.sz <= 0) {
        throw std::invalid_argument("Range dimensions must be positive");
    }
    const auto result = engine_.generate(r);
    if (result.status != 0) {
        throw std::runtime_error("generate_biomes failed");
    }
    return std::move(result.biomes);
}

const ::Generator& BiomeGenerator::c_generator() const noexcept
{
    return engine_.c_generator();
}

::Generator& BiomeGenerator::c_generator() noexcept
{
    return engine_.c_generator();
}

} // namespace cubiomes::cpp
