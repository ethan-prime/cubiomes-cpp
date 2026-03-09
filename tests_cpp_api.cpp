#include "cpp_api.hpp"
#include "generator.hpp"
#include "util.hpp"

#include <cassert>
#include <cstdlib>
#include <stdexcept>
#include <string_view>
#include <vector>

int main()
{
    using cubiomes::cpp::BiomeGenerator;
    using cubiomes::cpp::Dimension;

    BiomeGenerator cppg(MC_1_20, 0);
    cppg.apply_seed(Dimension::Overworld, 262);
    assert(cppg.biome_at_block(0, 63, 0) == mushroom_fields);
    assert(cubiomes::legacy::str2mc("1.21.2") == MC_1_21_3);
    assert(cubiomes::legacy::str2mc("Beta 1.8") == MC_B1_8);
    assert(cubiomes::legacy::str2mc(nullptr) == MC_UNDEF);
    assert(std::string_view(cubiomes::legacy::mc2str(MC_1_20)) == "1.20");
    assert(std::string_view(cubiomes::legacy::mc2str(MC_UNDEF)) == "?");

    ::Generator cg;
    cubiomes::legacy::setupGenerator(&cg, MC_1_20, 0);
    cubiomes::legacy::applySeed(&cg, DIM_OVERWORLD, 262);

    Range r = {4, -8, -8, 16, 16, 15, 1};
    std::vector<int> from_cpp = cppg.generate(r);

    int *cache = cubiomes::legacy::allocCache(&cg, r);
    assert(cache != nullptr);
    if (cubiomes::legacy::genBiomes(&cg, cache, r) != 0) {
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

    ::Generator cg_cpp{};
    cubiomes::cpp::setup_generator(cg_cpp, MC_1_20, 0);
    cubiomes::cpp::apply_seed(cg_cpp, DIM_OVERWORLD, 262);
    const auto generated = cubiomes::cpp::generate_biomes(cg_cpp, r);
    if (generated.status != 0) {
        return 1;
    }
    if (generated.biomes.size() != n) {
        return 1;
    }
    for (size_t i = 0; i < n; ++i) {
        if (generated.biomes[i] != static_cast<std::int32_t>(from_cpp[i])) {
            return 1;
        }
    }
    if (cubiomes::cpp::biome_at(cg_cpp, 1, 0, 63, 0) != mushroom_fields) {
        return 1;
    }

    cubiomes::cpp::GeneratorEngine engine{MC_1_20, 0};
    engine.apply_seed(DIM_OVERWORLD, 262);
    const auto generated2 = engine.generate(r);
    if (generated2.status != 0 || generated2.biomes.size() != n) {
        return 1;
    }
    std::vector<std::int32_t> out(n, 0);
    if (engine.generate_into(r, out) != 0) {
        return 1;
    }
    for (size_t i = 0; i < n; ++i) {
        if (out[i] != generated2.biomes[i]) {
            return 1;
        }
    }

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
