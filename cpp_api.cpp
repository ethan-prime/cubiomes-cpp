#include "cpp_api.hpp"

#include <cstdlib>
#include <stdexcept>

namespace cubiomes::cpp {

BiomeGenerator::BiomeGenerator(int mc, uint32_t flags)
{
    setupGenerator(&generator_, mc, flags);
}

void BiomeGenerator::reset(int mc, uint32_t flags)
{
    setupGenerator(&generator_, mc, flags);
}

void BiomeGenerator::apply_seed(Dimension dim, uint64_t seed)
{
    ::applySeed(&generator_, static_cast<int>(dim), seed);
}

int BiomeGenerator::biome_at(int scale, int x, int y, int z) const
{
    return ::getBiomeAt(&generator_, scale, x, y, z);
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

    const int sy = r.sy <= 0 ? 1 : r.sy;
    r.sy = sy;

    int *cache = ::allocCache(&generator_, r);
    if (!cache) {
        throw std::bad_alloc();
    }

    const int err = ::genBiomes(&generator_, cache, r);
    if (err != 0) {
        std::free(cache);
        throw std::runtime_error("genBiomes failed");
    }

    const size_t count = static_cast<size_t>(r.sx) *
        static_cast<size_t>(r.sz) *
        static_cast<size_t>(sy);
    std::vector<int> out(cache, cache + count);
    std::free(cache);
    return out;
}

const ::Generator& BiomeGenerator::c_generator() const noexcept
{
    return generator_;
}

::Generator& BiomeGenerator::c_generator() noexcept
{
    return generator_;
}

} // namespace cubiomes::cpp
