#include "cpp_api.hpp"
#include "generator.hpp"
#include "layers.hpp"
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

    {
        constexpr std::uint64_t world_seed = 123456789ULL;
        const auto sha_cpp = cubiomes::cpp::voronoi_sha(world_seed);
        const auto sha_legacy = cubiomes::legacy::getVoronoiSHA(world_seed);
        if (sha_cpp != sha_legacy) {
            return 1;
        }

        const auto cell = cubiomes::cpp::voronoi_access_3d(sha_cpp, 11, 63, -9);
        const cubiomes::cpp::VoronoiMapper mapper{sha_cpp};
        const auto cell_from_mapper = mapper.access_3d(11, 63, -9);
        int lx4 = 0;
        int ly4 = 0;
        int lz4 = 0;
        cubiomes::legacy::voronoiAccess3D(sha_cpp, 11, 63, -9, &lx4, &ly4, &lz4);
        if (cell.x4 != lx4 || cell.y4 != ly4 || cell.z4 != lz4) {
            return 1;
        }
        if (cell_from_mapper.x4 != lx4 || cell_from_mapper.y4 != ly4 || cell_from_mapper.z4 != lz4) {
            return 1;
        }

        constexpr int pw = 3;
        constexpr int ph = 3;
        constexpr int w = 8;
        constexpr int h = 8;
        std::vector<int> src_legacy{
            plains, plains, desert,
            plains, forest, desert,
            taiga, taiga, snowy_tundra
        };
        std::vector<int> out_legacy(static_cast<size_t>(w) * static_cast<size_t>(h), 0);
        cubiomes::legacy::mapVoronoiPlane(
            sha_cpp,
            out_legacy.data(),
            src_legacy.data(),
            0, 0, w, h, 64,
            0, 0, pw, ph
        );

        std::vector<std::int32_t> src_cpp(src_legacy.begin(), src_legacy.end());
        std::vector<std::int32_t> out_cpp(static_cast<size_t>(w) * static_cast<size_t>(h), 0);
        if (cubiomes::cpp::map_voronoi_plane(
                sha_cpp,
                out_cpp,
                src_cpp,
                0, 0, w, h, 64,
                0, 0, pw, ph) != 0) {
            return 1;
        }
        std::vector<std::int32_t> out_cpp_mapper(static_cast<size_t>(w) * static_cast<size_t>(h), 0);
        if (mapper.map_plane(out_cpp_mapper, src_cpp, 0, 0, w, h, 64, 0, 0, pw, ph) != 0) {
            return 1;
        }

        for (size_t i = 0; i < out_legacy.size(); ++i) {
            if (out_cpp[i] != out_legacy[i]) {
                return 1;
            }
            if (out_cpp_mapper[i] != out_legacy[i]) {
                return 1;
            }
        }
    }

    return 0;
}
