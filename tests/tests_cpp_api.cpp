#include "cpp_api.hpp"
#include "biomenoise.hpp"
#include "finders.hpp"
#include "generator.hpp"
#include "layers.hpp"
#include "noise.hpp"
#include "rng.hpp"
#include "util.hpp"
#include "legacy_generator.hpp"
#include "legacy_util.hpp"

#include <array>
#include <cassert>
#include <cstdlib>
#include <stdexcept>
#include <string_view>
#include <vector>

int main()
{
    using cubiomes::cpp::BiomeGenerator;
    using cubiomes::cpp::Dimension;
    {
        constexpr std::uint64_t ws = 123456789ULL;
        constexpr std::uint64_t ls = cubiomes::cpp::mc_layer_salt(2000ULL);
        static_assert(cubiomes::cpp::mc_start_seed(ws, ls) != 0ULL);
        static_assert(cubiomes::cpp::mc_chunk_seed(cubiomes::cpp::mc_start_seed(ws, ls), 10, -5) != 0ULL);
    }

    BiomeGenerator cppg(MC_1_20, 0);
    cppg.apply_seed(Dimension::Overworld, 262);
    assert(cppg.biome_at_block(0, 63, 0) == mushroom_fields);
    assert(cubiomes::legacy::str2mc("1.21.2") == MC_1_21_3);
    assert(cubiomes::legacy::str2mc("Beta 1.8") == MC_B1_8);
    assert(cubiomes::legacy::str2mc(nullptr) == MC_UNDEF);
    assert(std::string_view(cubiomes::legacy::mc2str(MC_1_20)) == "1.20");
    assert(std::string_view(cubiomes::legacy::mc2str(MC_UNDEF)) == "?");
    {
        using cubiomes::cpp::Biome;
        using cubiomes::cpp::DimensionKind;
        using cubiomes::cpp::Version;
        static_assert(cubiomes::cpp::to_raw(Version::V1_20) == MC_1_20);
        static_assert(cubiomes::cpp::to_raw(Biome::Plains) == plains);
        assert(cubiomes::cpp::biome_exists(Version::V1_20, Biome::Plains));
        assert(cubiomes::cpp::is_overworld(Version::V1_20, Biome::Plains));
        assert(cubiomes::cpp::dimension_kind(Biome::EndHighlands) == DimensionKind::End);
    }
    {
        EndNoise en{};
        setEndSeed(&en, MC_1_20, 262ULL);
        const auto end_biomes = cubiomes::cpp::map_end_biome(en, 0, 0, 4, 4);
        if (end_biomes.size() != 16) {
            return 1;
        }
        const auto end_scaled = cubiomes::cpp::map_end(en, 0, 0, 4, 4);
        if (end_scaled.size() != 16) {
            return 1;
        }
    }
    {
        std::uint64_t seed = 12345;
        PerlinNoise p{};
        cubiomes::cpp::perlin_init(p, seed);
        const auto s = cubiomes::cpp::sample_simplex_2d(p, 1.0, 2.0);
        if (s != s) {
            return 1;
        }
    }
    {
        std::uint64_t legacy_seed = 42ULL;
        cubiomes::cpp::JavaRng rng{42ULL};
        setSeed(&legacy_seed, 42ULL);
        for (int i = 0; i < 16; ++i) {
            if (rng.next_int(31) != nextInt(&legacy_seed, 31)) {
                return 1;
            }
        }
        rng.skip(4);
        skipNextN(&legacy_seed, 4);
        if (rng.next_long() != nextLong(&legacy_seed)) {
            return 1;
        }
    }
    {
        Xoroshiro legacy{};
        xSetSeed(&legacy, 987654321ULL);
        cubiomes::cpp::XoroshiroRng rng{987654321ULL};
        for (int i = 0; i < 16; ++i) {
            if (rng.next_int(97U) != xNextInt(&legacy, 97U)) {
                return 1;
            }
        }
        if (rng.next_double() != xNextDouble(&legacy)) {
            return 1;
        }
    }

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

    {
        const auto config = cubiomes::cpp::structure_config(Village, MC_1_20);
        if (!config.has_value()) {
            return 1;
        }
        StructureConfig legacy_config{};
        if (cubiomes::legacy::getStructureConfig(Village, MC_1_20, &legacy_config) == 0) {
            return 1;
        }
        if (config->salt != legacy_config.salt || config->regionSize != legacy_config.regionSize) {
            return 1;
        }

        const auto structure_pos = cubiomes::cpp::structure_position(Village, MC_1_20, 262ULL, 0, 0);
        if (!structure_pos.has_value()) {
            return 1;
        }
        Pos legacy_pos{};
        if (cubiomes::legacy::getStructurePos(Village, MC_1_20, 262ULL, 0, 0, &legacy_pos) == 0) {
            return 1;
        }
        if (structure_pos->x != legacy_pos.x || structure_pos->z != legacy_pos.z) {
            return 1;
        }
        std::array<Pos, 4> legacy_mineshafts{};
        const auto legacy_mineshaft_count = cubiomes::legacy::getMineshafts(MC_1_20, 262ULL, 0, 0, 2, 2, legacy_mineshafts.data(), static_cast<int>(legacy_mineshafts.size()));
        const auto cpp_mineshafts = cubiomes::cpp::mineshafts(MC_1_20, 262ULL, 0, 0, 2, 2);
        if (cpp_mineshafts.size() != static_cast<std::size_t>(legacy_mineshaft_count)) {
            return 1;
        }
        for (std::size_t i = 0; i < cpp_mineshafts.size(); ++i) {
            if (cpp_mineshafts[i].x != legacy_mineshafts[i].x || cpp_mineshafts[i].z != legacy_mineshafts[i].z) {
                return 1;
            }
        }

        cubiomes::cpp::StrongholdFinder strongholds{MC_1_20, 262ULL};
        StrongholdIter stronghold_iter{};
        const auto legacy_first = cubiomes::legacy::initFirstStronghold(&stronghold_iter, MC_1_20, 262ULL);
        const auto first_approx = strongholds.initial_approximation();
        if (first_approx.x != legacy_first.x || first_approx.z != legacy_first.z) {
            return 1;
        }
        const auto remaining_cpp = strongholds.next(&cg_cpp);
        const auto remaining_legacy = cubiomes::legacy::nextStronghold(&stronghold_iter, &cg_cpp);
        if (remaining_cpp != remaining_legacy) {
            return 1;
        }
        if (strongholds.state().pos.x != stronghold_iter.pos.x || strongholds.state().pos.z != stronghold_iter.pos.z) {
            return 1;
        }

        const auto spawn_estimate = cubiomes::cpp::estimate_spawn(cg_cpp);
        std::uint64_t legacy_rng{};
        const auto legacy_spawn_estimate = cubiomes::legacy::estimateSpawn(&cg_cpp, &legacy_rng);
        if (spawn_estimate.position.x != legacy_spawn_estimate.x ||
            spawn_estimate.position.z != legacy_spawn_estimate.z ||
            spawn_estimate.rng != legacy_rng) {
            return 1;
        }
        const auto spawn_pos_cpp = cubiomes::cpp::spawn(cg_cpp);
        const auto spawn_pos_legacy = cubiomes::legacy::getSpawn(&cg_cpp);
        if (spawn_pos_cpp.x != spawn_pos_legacy.x || spawn_pos_cpp.z != spawn_pos_legacy.z) {
            return 1;
        }
        const auto located_cpp = cubiomes::cpp::locate_biome(cg_cpp, 0, 63, 0, 64, (1ULL << plains) | (1ULL << forest), 0, 262ULL);
        std::uint64_t locate_rng = 262ULL;
        int legacy_passes = 0;
        const auto located_legacy = cubiomes::legacy::locateBiome(&cg_cpp, 0, 63, 0, 64, (1ULL << plains) | (1ULL << forest), 0, &locate_rng, &legacy_passes);
        if (located_cpp.position.x != located_legacy.x ||
            located_cpp.position.z != located_legacy.z ||
            located_cpp.passes != legacy_passes) {
            return 1;
        }

        const auto builder_filter = cubiomes::cpp::BiomeFilterBuilder{MC_1_20}.require(plains).match_any(forest).build();
        BiomeFilter legacy_filter{};
        const int required[]{plains};
        const int match_any[]{forest};
        cubiomes::legacy::setupBiomeFilter(&legacy_filter, MC_1_20, 0, required, 1, nullptr, 0, match_any, 1);
        if (builder_filter.biomeToFind != legacy_filter.biomeToFind ||
            builder_filter.biomeToPick != legacy_filter.biomeToPick) {
            return 1;
        }

        const Range biome_range{4, -8, -8, 16, 16, 63, 1};
        auto cpp_biome_check = cubiomes::cpp::check_for_biomes(cg_cpp, biome_range, DIM_OVERWORLD, 262ULL, builder_filter);
        std::vector<int> legacy_cache(cubiomes::cpp::min_cache_size(cg_cpp, biome_range.scale, biome_range.sx, biome_range.sy, biome_range.sz));
        const auto legacy_status = cubiomes::legacy::checkForBiomes(&cg_cpp, legacy_cache.data(), biome_range, DIM_OVERWORLD, 262ULL, &legacy_filter, nullptr);
        if (cpp_biome_check.status != legacy_status) {
            return 1;
        }
        if (legacy_status == 1) {
            if (cpp_biome_check.biomes.size() != static_cast<std::size_t>(biome_range.sx * biome_range.sz * biome_range.sy)) {
                return 1;
            }
            for (std::size_t i = 0; i < cpp_biome_check.biomes.size(); ++i) {
                if (cpp_biome_check.biomes[i] != legacy_cache[i]) {
                    return 1;
                }
            }
        }

        std::array<Piece, END_CITY_PIECES_MAX> legacy_end_city{};
        const auto legacy_end_city_count = cubiomes::legacy::getEndCityPieces(legacy_end_city.data(), 262ULL, 10, 10);
        const auto cpp_end_city = cubiomes::cpp::end_city_pieces(262ULL, 10, 10);
        if (cpp_end_city.size() != static_cast<std::size_t>(legacy_end_city_count)) {
            return 1;
        }
        if (!cpp_end_city.empty()) {
            if (cpp_end_city.front().type != legacy_end_city.front().type ||
                cpp_end_city.front().pos.x != legacy_end_city.front().pos.x ||
                cpp_end_city.front().pos.z != legacy_end_city.front().pos.z) {
                return 1;
            }
        }

        std::array<Piece, 128> legacy_fortress{};
        const auto legacy_fortress_count = cubiomes::legacy::getFortressPieces(legacy_fortress.data(), static_cast<int>(legacy_fortress.size()), MC_1_20, 262ULL, 3, 4);
        const auto cpp_fortress = cubiomes::cpp::fortress_pieces(static_cast<int>(legacy_fortress.size()), MC_1_20, 262ULL, 3, 4);
        if (cpp_fortress.size() != static_cast<std::size_t>(legacy_fortress_count)) {
            return 1;
        }
        if (!cpp_fortress.empty()) {
            if (cpp_fortress.front().type != legacy_fortress.front().type ||
                cpp_fortress.front().pos.x != legacy_fortress.front().pos.x ||
                cpp_fortress.front().pos.z != legacy_fortress.front().pos.z) {
                return 1;
            }
        }

        std::array<int, HOUSE_NUM> legacy_houses{};
        const auto legacy_house_rng = cubiomes::legacy::getHouseList(legacy_houses.data(), 262ULL, 1, 2);
        const auto cpp_houses = cubiomes::cpp::house_list(262ULL, 1, 2);
        if (cpp_houses.rng != legacy_house_rng) {
            return 1;
        }
        for (std::size_t i = 0; i < cpp_houses.houses.size(); ++i) {
            if (cpp_houses.houses[i] != legacy_houses[i]) {
                return 1;
            }
        }

        std::array<Pos, 20> legacy_gateways{};
        cubiomes::legacy::getFixedEndGateways(MC_1_20, 262ULL, legacy_gateways.data());
        const auto cpp_gateways = cubiomes::cpp::fixed_end_gateways(MC_1_20, 262ULL);
        for (std::size_t i = 0; i < cpp_gateways.size(); ++i) {
            if (cpp_gateways[i].x != legacy_gateways[i].x || cpp_gateways[i].z != legacy_gateways[i].z) {
                return 1;
            }
        }

        constexpr std::array<std::uint64_t, 4> seeds{262ULL, 1ULL, 0x123456789abcdef0ULL, 999999937ULL};
        for (const auto seed : seeds) {
            std::array<int, HOUSE_NUM> legacy_rolls{};
            const auto legacy_rng2 = cubiomes::legacy::getHouseList(legacy_rolls.data(), seed, -7, 11);
            const auto cpp_rolls = cubiomes::cpp::house_list(seed, -7, 11);
            if (cpp_rolls.rng != legacy_rng2) {
                return 1;
            }
            for (std::size_t i = 0; i < cpp_rolls.houses.size(); ++i) {
                if (cpp_rolls.houses[i] != legacy_rolls[i]) {
                    return 1;
                }
            }
            if (cpp_rolls.houses[HouseSmall] < 2 || cpp_rolls.houses[HouseSmall] > 4) return 1;
            if (cpp_rolls.houses[Church] < 0 || cpp_rolls.houses[Church] > 1) return 1;
            if (cpp_rolls.houses[Library] < 0 || cpp_rolls.houses[Library] > 2) return 1;
            if (cpp_rolls.houses[WoodHut] < 2 || cpp_rolls.houses[WoodHut] > 5) return 1;
            if (cpp_rolls.houses[Butcher] < 0 || cpp_rolls.houses[Butcher] > 2) return 1;
            if (cpp_rolls.houses[FarmLarge] < 1 || cpp_rolls.houses[FarmLarge] > 4) return 1;
            if (cpp_rolls.houses[FarmSmall] < 2 || cpp_rolls.houses[FarmSmall] > 4) return 1;
            if (cpp_rolls.houses[Blacksmith] < 0 || cpp_rolls.houses[Blacksmith] > 1) return 1;
            if (cpp_rolls.houses[HouseLarge] < 0 || cpp_rolls.houses[HouseLarge] > 3) return 1;
        }

        for (const auto seed : seeds) {
            std::array<Pos, 20> legacy_order{};
            cubiomes::legacy::getFixedEndGateways(MC_1_20, seed, legacy_order.data());
            const auto cpp_order = cubiomes::cpp::fixed_end_gateways(MC_1_20, seed);
            for (std::size_t i = 0; i < cpp_order.size(); ++i) {
                if (cpp_order[i].x != legacy_order[i].x || cpp_order[i].z != legacy_order[i].z) {
                    return 1;
                }
            }
            for (std::size_t i = 0; i < cpp_order.size(); ++i) {
                for (std::size_t j = i + 1; j < cpp_order.size(); ++j) {
                    if (cpp_order[i].x == cpp_order[j].x && cpp_order[i].z == cpp_order[j].z) {
                        return 1;
                    }
                }
            }
        }

        {
            StructureVariant sv{};
            if (getVariant(&sv, Monument, MC_1_20, 262ULL, 128, 256, plains) != 1) return 1;
            if (sv.x != -29 || sv.z != -29 || sv.sx != 58 || sv.sz != 58) return 1;
        }

        {
            StructureVariant sv{};
            if (getVariant(&sv, Village, MC_1_20, 262ULL, 128, 256, plains) != 1) return 1;
            if (sv.start > 4) return 1;
            if (sv.sy <= 0 || sv.sx <= 0 || sv.sz <= 0) return 1;
        }

        {
            StructureVariant sv{};
            if (getVariant(&sv, Bastion, MC_1_20, 262ULL, 128, 256, nether_wastes) != 1) return 1;
            if (sv.start > 3) return 1;
            if (sv.sy <= 0 || sv.sx <= 0 || sv.sz <= 0) return 1;
        }

        {
            StructureVariant sv{};
            if (getVariant(&sv, Igloo, MC_1_20, 262ULL, 128, 256, snowy_tundra) != 1) return 1;
            if (sv.sy != 5) return 1;
            if (sv.sx != 7 && sv.sx != 8) return 1;
            if (sv.sz != 7 && sv.sz != 8) return 1;
            if (sv.size < 4 || sv.size > 11) return 1;
        }

        {
            StructureVariant sv{};
            if (getVariant(&sv, Desert_Pyramid, MC_1_19, 262ULL, 128, 256, desert) != 1) return 1;
            if (sv.sx != 21 || sv.sy != 15 || sv.sz != 21) return 1;
        }

        {
            StructureVariant sv{};
            if (getVariant(&sv, Jungle_Temple, MC_1_20, 262ULL, 128, 256, jungle) != 1) return 1;
            if (sv.sy != 10) return 1;
            const bool ok = (sv.sx == 12 && sv.sz == 15) || (sv.sx == 15 && sv.sz == 12);
            if (!ok) return 1;
        }

        {
            StructureVariant sv{};
            if (getVariant(&sv, Trial_Chambers, MC_1_21, 262ULL, 128, 256, plains) != 1) return 1;
            if (sv.start > 1) return 1;
            if (sv.sx != 19 || sv.sy != 20 || sv.sz != 19) return 1;
            if (sv.y < -40 || sv.y > -20) return 1;
        }

        {
            StructureVariant sv{};
            if (getVariant(&sv, Ancient_City, MC_1_20, 262ULL, 128, 256, deep_dark) != 1) return 1;
            if (sv.start < 1 || sv.start > 3) return 1;
            if (sv.sy != 31 || sv.y != -27) return 1;
        }

        {
            bool observed = false;
            for (std::uint64_t probe_seed = 1; probe_seed < 5000; ++probe_seed) {
                StructureVariant sv{};
                if (getVariant(&sv, Geode, MC_1_20, probe_seed, 128, 256, plains) != 1) {
                    continue;
                }
                observed = true;
                if (sv.size < 3 || sv.size > 4) return 1;
                if (sv.y < -53 || sv.y > 35) return 1;
                break;
            }
            if (!observed) return 1;
        }

        {
            StructureVariant sv{};
            if (getVariant(&sv, Ruined_Portal, MC_1_20, 262ULL, 128, 256, mangrove_swamp) != 1) return 1;
            if (sv.biome != swamp) return 1;
            if (sv.start < 1 || sv.start > 10) return 1;
            if (sv.giant && sv.start > 3) return 1;
        }
    }

    return 0;
}
