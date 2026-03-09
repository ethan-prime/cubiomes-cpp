#include "cpp_api.hpp"

#include <cassert>
#include <cstdlib>
#include <stdexcept>
#include <vector>

int main()
{
    using cubiomes::cpp::BiomeGenerator;
    using cubiomes::cpp::Dimension;

    BiomeGenerator cppg(MC_1_20, 0);
    cppg.apply_seed(Dimension::Overworld, 262);
    assert(cppg.biome_at_block(0, 63, 0) == mushroom_fields);

    ::Generator cg;
    setupGenerator(&cg, MC_1_20, 0);
    applySeed(&cg, DIM_OVERWORLD, 262);

    Range r = {4, -8, -8, 16, 16, 15, 1};
    std::vector<int> from_cpp = cppg.generate(r);

    int *cache = allocCache(&cg, r);
    assert(cache != nullptr);
    if (genBiomes(&cg, cache, r) != 0) {
        std::free(cache);
        return 1;
    }

    const size_t n = static_cast<size_t>(r.sx) *
        static_cast<size_t>(r.sz) *
        static_cast<size_t>(r.sy);
    assert(from_cpp.size() == n);
    for (size_t i = 0; i < n; i++) {
        assert(from_cpp[i] == cache[i]);
    }
    std::free(cache);

    bool threw = false;
    try {
        Range bad = {4, 0, 0, 0, 1, 0, 1};
        (void) cppg.generate(bad);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    if (!threw) {
        return 1;
    }

    return 0;
}
