#include "finders.hpp"
#include "biomes.hpp"
#include "legacy_generator.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <climits>
#include <cfloat>
#include <cmath>

#define PI 3.14159265358979323846

namespace {

auto get_house_list_impl(std::span<int, 9> out, std::uint64_t seed, int chunk_x, int chunk_z) -> std::uint64_t;
auto get_fixed_end_gateways_impl(int mc, std::uint64_t seed, std::span<Pos, 20> out) -> void;
auto get_linked_gateway_chunk_impl(
    const EndNoise *en,
    const SurfaceNoise *sn,
    std::uint64_t seed,
    Pos src,
    Pos *dst
) -> Pos;
auto get_linked_gateway_pos_impl(const EndNoise *en, const SurfaceNoise *sn, std::uint64_t seed, Pos src) -> Pos;
auto get_variant_impl(
    StructureVariant *r,
    int struct_type,
    int mc,
    std::uint64_t seed,
    int x,
    int z,
    int biome_id
) -> int;

} // namespace

namespace cubiomes::cpp {

auto structure_config(std::int32_t structure_type, std::int32_t mc) -> std::optional<StructureConfig>
{
    StructureConfig config{};
    if (!getStructureConfig(structure_type, mc, &config)) {
        return std::nullopt;
    }
    return config;
}

auto structure_position(
    std::int32_t structure_type,
    std::int32_t mc,
    std::uint64_t seed,
    std::int32_t reg_x,
    std::int32_t reg_z
) -> std::optional<Pos>
{
    Pos position{};
    if (!getStructurePos(structure_type, mc, seed, reg_x, reg_z, &position)) {
        return std::nullopt;
    }
    return position;
}

auto mineshafts(
    std::int32_t mc,
    std::uint64_t seed,
    std::int32_t chunk_x,
    std::int32_t chunk_z,
    std::int32_t chunk_w,
    std::int32_t chunk_h
) -> std::vector<Pos>
{
    const auto capacity = static_cast<std::size_t>(std::max(chunk_w, 0) * std::max(chunk_h, 0));
    std::vector<Pos> positions(capacity);
    const auto found = getMineshafts(
        mc,
        seed,
        chunk_x,
        chunk_z,
        chunk_w,
        chunk_h,
        positions.data(),
        static_cast<int>(positions.size())
    );
    positions.resize(static_cast<std::size_t>(std::max(found, 0)));
    return positions;
}

auto locate_biome(
    const Generator &g,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z,
    std::int32_t radius,
    std::uint64_t valid_b,
    std::uint64_t valid_m,
    std::uint64_t rng
) -> LocateBiomeResult
{
    auto passes = 0;
    auto rng_copy = rng;
    const auto position = locateBiome(&g, x, y, z, radius, valid_b, valid_m, &rng_copy, &passes);
    return LocateBiomeResult{
        .position = position,
        .passes = passes,
    };
}

auto estimate_spawn(const Generator &g) -> SpawnEstimateResult
{
    std::uint64_t rng{};
    const auto position = estimateSpawn(&g, &rng);
    return SpawnEstimateResult{
        .position = position,
        .rng = rng,
    };
}

auto spawn(const Generator &g) -> Pos
{
    return getSpawn(&g);
}

StrongholdFinder::StrongholdFinder(std::int32_t mc, std::uint64_t s48)
{
    (void)initFirstStronghold(&iter_, mc, s48);
}

auto StrongholdFinder::initial_approximation() const noexcept -> Pos
{
    return iter_.nextapprox;
}

auto StrongholdFinder::state() const noexcept -> const StrongholdIter&
{
    return iter_;
}

auto StrongholdFinder::next(const Generator *g) -> std::int32_t
{
    return nextStronghold(&iter_, g);
}

BiomeFilterBuilder::BiomeFilterBuilder(std::int32_t mc, std::uint32_t flags) : mc_(mc), flags_(flags) {}

auto BiomeFilterBuilder::require(std::int32_t biome_id) -> BiomeFilterBuilder&
{
    required_.push_back(biome_id);
    return *this;
}

auto BiomeFilterBuilder::exclude(std::int32_t biome_id) -> BiomeFilterBuilder&
{
    excluded_.push_back(biome_id);
    return *this;
}

auto BiomeFilterBuilder::match_any(std::int32_t biome_id) -> BiomeFilterBuilder&
{
    match_any_.push_back(biome_id);
    return *this;
}

auto BiomeFilterBuilder::build() const -> BiomeFilter
{
    BiomeFilter filter{};
    setupBiomeFilter(
        &filter,
        mc_,
        flags_,
        required_.empty() ? nullptr : required_.data(),
        static_cast<int>(required_.size()),
        excluded_.empty() ? nullptr : excluded_.data(),
        static_cast<int>(excluded_.size()),
        match_any_.empty() ? nullptr : match_any_.data(),
        static_cast<int>(match_any_.size())
    );
    return filter;
}

auto check_for_biomes(
    Generator &g,
    Range r,
    std::int32_t dim,
    std::uint64_t seed,
    const BiomeFilter &filter
) -> CheckBiomesResult
{
    CheckBiomesResult result{};
    const auto cache_len = getMinCacheSize(&g, r.scale, r.sx, r.sy, r.sz);
    if (cache_len == 0) {
        result.status = -1;
        return result;
    }

    std::vector<int> cache(cache_len, 0);
    result.status = checkForBiomes(&g, cache.data(), r, dim, seed, &filter, nullptr);
    if (result.status != 1) {
        return result;
    }

    const auto count = static_cast<std::size_t>(r.sx) *
        static_cast<std::size_t>(r.sz) *
        static_cast<std::size_t>(r.sy == 0 ? 1 : r.sy);
    result.biomes.resize(count);
    std::transform(cache.begin(), cache.begin() + static_cast<std::ptrdiff_t>(count), result.biomes.begin(), [](int biome) {
        return static_cast<std::int32_t>(biome);
    });
    return result;
}

auto parameter_range(
    const DoublePerlinNoise &parameter,
    std::int32_t x,
    std::int32_t z,
    std::int32_t w,
    std::int32_t h
) -> ParameterRangeResult
{
    ParameterRangeResult result{};
    result.status = getParaRange(&parameter, &result.minimum, &result.maximum, x, z, w, h, nullptr, nullptr);
    return result;
}

auto end_city_pieces(std::uint64_t seed, std::int32_t chunk_x, std::int32_t chunk_z) -> std::vector<Piece>
{
    std::vector<Piece> pieces(END_CITY_PIECES_MAX);
    const auto count = getEndCityPieces(pieces.data(), seed, chunk_x, chunk_z);
    pieces.resize(static_cast<std::size_t>(std::max(count, 0)));
    return pieces;
}

auto fortress_pieces(
    std::int32_t n,
    std::int32_t mc,
    std::uint64_t seed,
    std::int32_t chunk_x,
    std::int32_t chunk_z
) -> std::vector<Piece>
{
    std::vector<Piece> pieces(static_cast<std::size_t>(std::max(n, 0)));
    const auto count = getFortressPieces(pieces.data(), n, mc, seed, chunk_x, chunk_z);
    pieces.resize(static_cast<std::size_t>(std::max(count, 0)));
    return pieces;
}

auto fixed_end_gateways(std::int32_t mc, std::uint64_t seed) -> std::array<Pos, 20>
{
    std::array<Pos, 20> positions{};
    get_fixed_end_gateways_impl(mc, seed, positions);
    return positions;
}

auto house_list(std::uint64_t seed, std::int32_t chunk_x, std::int32_t chunk_z) -> HouseListResult
{
    HouseListResult result{};
    std::array<int, 9> tmp{};
    result.rng = get_house_list_impl(tmp, seed, chunk_x, chunk_z);
    std::transform(tmp.begin(), tmp.end(), result.houses.begin(), [](int value) {
        return static_cast<std::int32_t>(value);
    });
    return result;
}

} // namespace cubiomes::cpp


//==============================================================================
// Finding Structure Positions
//==============================================================================


void setAttemptSeed(uint64_t *s, int cx, int cz)
{
    *s ^= (uint64_t)(cx >> 4) ^ ( (uint64_t)(cz >> 4) << 4 );
    setSeed(s, *s);
    next(s, 31);
}

uint64_t getPopulationSeed(int mc, uint64_t ws, int x, int z)
{
    Xoroshiro xr;
    uint64_t s;
    uint64_t a, b;

    if (mc >= MC_1_18)
    {
        xSetSeed(&xr, ws);
        a = xNextLongJ(&xr);
        b = xNextLongJ(&xr);
    }
    else
    {
        setSeed(&s, ws);
        a = nextLong(&s);
        b = nextLong(&s);
    }
    if (mc >= MC_1_13)
    {
        a |= 1; b |= 1;
    }
    else
    {
        a = (int64_t)a / 2 * 2 + 1;
        b = (int64_t)b / 2 * 2 + 1;
    }
    return (x * a + z * b) ^ ws;
}


int getStructureConfig(int structureType, int mc, StructureConfig *sconf)
{
    static const StructureConfig
    // for desert pyramids, jungle temples, witch huts and igloos prior to 1.13
    s_feature               = { 14357617, 32, 24, Feature,          0,0},
    s_igloo_112             = { 14357617, 32, 24, Igloo,            0,0},
    s_swamp_hut_112         = { 14357617, 32, 24, Swamp_Hut,        0,0},
    s_desert_pyramid_112    = { 14357617, 32, 24, Desert_Pyramid,   0,0},
    s_jungle_temple_112     = { 14357617, 32, 24, Jungle_Pyramid,   0,0},
    // ocean features before 1.16
    s_ocean_ruin_115        = { 14357621, 16,  8, Ocean_Ruin,       0,0},
    s_shipwreck_115         = {165745295, 16,  8, Shipwreck,        0,0},
    // 1.13 separated feature seeds by type
    s_desert_pyramid        = { 14357617, 32, 24, Desert_Pyramid,   0,0},
    s_igloo                 = { 14357618, 32, 24, Igloo,            0,0},
    s_jungle_temple         = { 14357619, 32, 24, Jungle_Pyramid,   0,0},
    s_swamp_hut             = { 14357620, 32, 24, Swamp_Hut,        0,0},
    s_outpost               = {165745296, 32, 24, Outpost,          0,0},
    s_village_117           = { 10387312, 32, 24, Village,          0,0},
    s_village               = { 10387312, 34, 26, Village,          0,0},
    s_ocean_ruin            = { 14357621, 20, 12, Ocean_Ruin,       0,0},
    s_shipwreck             = {165745295, 24, 20, Shipwreck,        0,0},
    s_monument              = { 10387313, 32, 27, Monument,         0,0},
    s_mansion               = { 10387319, 80, 60, Mansion,          0,0},
    s_ruined_portal         = { 34222645, 40, 25, Ruined_Portal,    0,0},
    s_ruined_portal_n       = { 34222645, 40, 25, Ruined_Portal,    DIM_NETHER,0},
    s_ruined_portal_n_117   = { 34222645, 25, 15, Ruined_Portal_N,  DIM_NETHER,0},
    s_ancient_city          = { 20083232, 24, 16, Ancient_City,     0,0},
    s_trail_ruins           = { 83469867, 34, 26, Trail_Ruins,      0,0},
    s_trial_chambers        = { 94251327, 34, 22, Trial_Chambers,   0,0},
    s_treasure              = { 10387320,  1,  1, Treasure,         0,0},
    s_mineshaft             = {        0,  1,  1, Mineshaft,        0,0},
    s_desert_well_115       = {    30010,  1,  1, Desert_Well,      0, 1.f/1000},
    s_desert_well_117       = {    40013,  1,  1, Desert_Well,      0, 1.f/1000},
    s_desert_well           = {    40002,  1,  1, Desert_Well,      0, 1.f/1000},
    s_geode_117             = {    20000,  1,  1, Geode,            0, 1.f/24},
    s_geode                 = {    20002,  1,  1, Geode,            0, 1.f/24},
    // nether and end structures
    s_fortress_115          = {        0, 16,  8, Fortress,         DIM_NETHER,0},
    s_fortress              = { 30084232, 27, 23, Fortress,         DIM_NETHER,0},
    s_bastion               = { 30084232, 27, 23, Bastion,          DIM_NETHER,0},
    s_end_city              = { 10387313, 20,  9, End_City,         DIM_END,0},
    // for the scattered return gateways
    s_end_gateway_115       = {    30000,  1,  1, End_Gateway,      DIM_END, 700},
    s_end_gateway_116       = {    40013,  1,  1, End_Gateway,      DIM_END, 700},
    s_end_gateway_117       = {    40013,  1,  1, End_Gateway,      DIM_END, 1.f/700},
    s_end_gateway           = {    40000,  1,  1, End_Gateway,      DIM_END, 1.f/700},
    s_end_island_116        = {        0,  1,  1, End_Island,       DIM_END, 14},
    s_end_island            = {        0,  1,  1, End_Island,       DIM_END, 1.f/14}
    ;

    switch (structureType)
    {
    case Feature:
        *sconf = s_feature;
        return mc <= MC_1_12;
    case Desert_Pyramid:
        *sconf = mc <= MC_1_12 ? s_desert_pyramid_112 : s_desert_pyramid;
        return mc >= MC_1_3;
    case Jungle_Pyramid:
        *sconf = mc <= MC_1_12 ? s_jungle_temple_112 : s_jungle_temple;
        return mc >= MC_1_3;
    case Swamp_Hut:
        *sconf = mc <= MC_1_12 ? s_swamp_hut_112 : s_swamp_hut;
        return mc >= MC_1_4;
    case Igloo:
        *sconf = mc <= MC_1_12 ? s_igloo_112 : s_igloo;
        return mc >= MC_1_9;
    case Village:
        *sconf = mc <= MC_1_17 ? s_village_117 : s_village;
        return mc >= MC_B1_8;
    case Ocean_Ruin:
        *sconf = mc <= MC_1_15 ? s_ocean_ruin_115 : s_ocean_ruin;
        return mc >= MC_1_13;
    case Shipwreck:
        *sconf = mc <= MC_1_15 ? s_shipwreck_115 : s_shipwreck;
        return mc >= MC_1_13;
    case Ruined_Portal:
        *sconf = s_ruined_portal;
        return mc >= MC_1_16_1;
    case Ruined_Portal_N:
        *sconf = mc <= MC_1_17 ? s_ruined_portal_n_117 : s_ruined_portal_n;
        return mc >= MC_1_16_1;
    case Monument:
        *sconf = s_monument;
        return mc >= MC_1_8;
    case End_City:
        *sconf = s_end_city;
        return mc >= MC_1_9;
    case Mansion:
        *sconf = s_mansion;
        return mc >= MC_1_11;
    case Outpost:
        *sconf = s_outpost;
        return mc >= MC_1_14;
    case Ancient_City:
        *sconf = s_ancient_city;
        return mc >= MC_1_19_2;
    case Treasure:
        *sconf = s_treasure;
        return mc >= MC_1_13;
    case Mineshaft:
        *sconf = s_mineshaft;
        return mc >= MC_B1_8;
    case Fortress:
        *sconf = mc <= MC_1_15 ? s_fortress_115 : s_fortress;
        return mc >= MC_1_0;
    case Bastion:
        *sconf = s_bastion;
        return mc >= MC_1_16_1;
    case End_Gateway:
        if      (mc <= MC_1_15) *sconf = s_end_gateway_115;
        else if (mc <= MC_1_16) *sconf = s_end_gateway_116;
        else if (mc <= MC_1_17) *sconf = s_end_gateway_117;
        else                    *sconf = s_end_gateway;
        // 1.11 and 1.12 generate gateways using a random source that passed
        // the block filling, making them much more difficult to predict
        return mc >= MC_1_13;
    case End_Island:
        if      (mc <= MC_1_16) *sconf = s_end_island_116;
        else                    *sconf = s_end_island;
        return mc >= MC_1_13; // we only support decorator features for 1.13+
    case Desert_Well:
        if      (mc <= MC_1_15) *sconf = s_desert_well_115;
        else if (mc <= MC_1_17) *sconf = s_desert_well_117;
        else                    *sconf = s_desert_well;
        // wells were introduced in 1.2, but we only support decorator features
        // for 1.13+
        return mc >= MC_1_13;
    case Geode:
        *sconf = mc <= MC_1_17 ? s_geode_117 : s_geode;
        return mc >= MC_1_17;
    case Trail_Ruins:
        *sconf = s_trail_ruins;
        return mc >= MC_1_20;
    case Trial_Chambers:
        *sconf = s_trial_chambers;
        return mc >= MC_1_21_1;
    default:
        *sconf = StructureConfig{};
        return 0;
    }
}


// like getFeaturePos(), but modifies the rng seed
static inline
auto get_reg_pos_impl(uint64_t seed, int rx, int rz, StructureConfig sc) -> std::pair<Pos, uint64_t>
{
    auto s = seed;
    setSeed(&s, rx*341873128712ULL + rz*132897987541ULL + s + sc.salt);
    Pos p{};
    p.x = ((uint64_t)rx * sc.regionSize + nextInt(&s, sc.chunkRange)) << 4;
    p.z = ((uint64_t)rz * sc.regionSize + nextInt(&s, sc.chunkRange)) << 4;
    return {p, s};
}

auto list_mineshafts_impl(int mc, uint64_t seed, int cx0, int cz0, int cx1, int cz1) -> std::vector<Pos>
{
    std::vector<Pos> positions{};
    const auto width = std::max(0, cx1 - cx0 + 1);
    const auto height = std::max(0, cz1 - cz0 + 1);
    positions.reserve(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));

    uint64_t s{};
    setSeed(&s, seed);
    const auto a = nextLong(&s);
    const auto b = nextLong(&s);

    for (int i = cx0; i <= cx1; i++)
    {
        const uint64_t aix = static_cast<uint64_t>(i) * a ^ seed;
        for (int j = cz0; j <= cz1; j++)
        {
            setSeed(&s, aix ^ static_cast<uint64_t>(j) * b);
            auto add = false;
            if (mc >= MC_1_13)
            {
                add = unlikely(nextDouble(&s) < 0.004);
            }
            else
            {
                skipNextN(&s, 1);
                if unlikely(nextDouble(&s) < 0.004)
                {
                    auto d = i;
                    if (-i > d) d = -i;
                    if (+j > d) d = +j;
                    if (-j > d) d = -j;
                    add = (d >= 80 || nextInt(&s, 80) < d);
                }
            }
            if (add) {
                positions.push_back(Pos{.x = i * 16, .z = j * 16});
            }
        }
    }
    return positions;
}

namespace {

struct StructurePosResult
{
    Pos pos{};
    int valid{};
};

auto get_structure_pos_impl(int structureType, int mc, uint64_t seed, int regX, int regZ) -> StructurePosResult
{
    StructureConfig sconf;
#if STRUCT_CONFIG_OVERRIDE
    if (!getStructureConfig_override(structureType, mc, &sconf))
#else
    if (!getStructureConfig(structureType, mc, &sconf))
#endif
    {
        return {};
    }

    StructurePosResult result{};
    switch (structureType)
    {
    case Feature:
    case Desert_Pyramid:
    case Jungle_Pyramid:
    case Swamp_Hut:
    case Igloo:
    case Village:
    case Ocean_Ruin:
    case Shipwreck:
    case Ruined_Portal:
    case Ruined_Portal_N:
    case Ancient_City:
    case Trail_Ruins:
    case Trial_Chambers:
        result.pos = getFeaturePos(sconf, seed, regX, regZ);
        result.valid = 1;
        return result;

    case Monument:
    case Mansion:
        result.pos = getLargeStructurePos(sconf, seed, regX, regZ);
        result.valid = 1;
        return result;

    case End_City:
        result.pos = getLargeStructurePos(sconf, seed, regX, regZ);
        result.valid = (result.pos.x*(int64_t)result.pos.x + result.pos.z*(int64_t)result.pos.z) >= 1008*1008LL;
        return result;

    case Outpost:
        result.pos = getFeaturePos(sconf, seed, regX, regZ);
        setAttemptSeed(&seed, (result.pos.x) >> 4, (result.pos.z) >> 4);
        result.valid = nextInt(&seed, 5) == 0;
        return result;

    case Treasure:
        result.pos.x = regX * 16 + 9;
        result.pos.z = regZ * 16 + 9;
        seed = regX*341873128712ULL + regZ*132897987541ULL + seed + sconf.salt;
        setSeed(&seed, seed);
        result.valid = nextFloat(&seed) < 0.01;
        return result;

    case Mineshaft:
    {
        const auto positions = list_mineshafts_impl(mc, seed, regX, regZ, regX, regZ);
        if (!positions.empty())
        {
            result.pos = positions.front();
            result.valid = 1;
        }
        return result;
    }

    case Fortress:
        if (mc >= MC_1_18) {
            result.pos = getFeaturePos(sconf, seed, regX, regZ);
            result.valid = 1; // fortresses gen where bastions don't (biome dependent)
            return result;
        } else if (mc >= MC_1_16_1) {
            auto [pos, next_seed] = get_reg_pos_impl(seed, regX, regZ, sconf);
            result.pos = pos;
            result.valid = nextInt(&next_seed, 5) < 2;
            return result;
        } else {
            setAttemptSeed(&seed, regX * 16, regZ * 16);
            result.valid = nextInt(&seed, 3) == 0;
            result.pos.x = (regX * 16 + nextInt(&seed, 8) + 4) * 16;
            result.pos.z = (regZ * 16 + nextInt(&seed, 8) + 4) * 16;
            return result;
        }

    case Bastion:
        if (mc >= MC_1_18) {
            result.pos = getFeaturePos(sconf, seed, regX, regZ);
            seed = chunkGenerateRnd(seed, result.pos.x >> 4, result.pos.z >> 4);
            result.valid = nextInt(&seed, 5) >= 2;
            return result;
        } else {
            auto [pos, next_seed] = get_reg_pos_impl(seed, regX, regZ, sconf);
            result.pos = pos;
            result.valid = nextInt(&next_seed, 5) >= 2;
            return result;
        }

    case End_Gateway:
    case End_Island:
    case Desert_Well:
    case Geode:
        // decorator features
        result.pos.x = regX * 16;
        result.pos.z = regZ * 16;
        seed = getPopulationSeed(mc, seed, result.pos.x, result.pos.z);
        if (mc >= MC_1_18)
        {
            Xoroshiro xr;
            xSetSeed(&xr, seed + sconf.salt);
            if (xNextFloat(&xr) >= sconf.rarity)
                return {};
            result.pos.x += xNextIntJ(&xr, 16);
            result.pos.z += xNextIntJ(&xr, 16);
        }
        else
        {
            setSeed(&seed, seed + sconf.salt);
            if (sconf.rarity < 1.0) {
                if (nextFloat(&seed) >= sconf.rarity)
                    return {};
            } else {
                if (nextInt(&seed, (int)sconf.rarity) != 0)
                    return {};
            }
            result.pos.x += nextInt(&seed, 16);
            result.pos.z += nextInt(&seed, 16);
        }
        result.valid = 1;
        return result;

    default:
        fprintf(stderr,
                "ERR getStructurePos: unsupported structure type %d\n", structureType);
        exit(-1);
    }
    return {};
}

} // namespace

int getStructurePos(int structureType, int mc, uint64_t seed, int regX, int regZ, Pos *pos)
{
    const auto result = get_structure_pos_impl(structureType, mc, seed, regX, regZ);
    if (pos != nullptr) {
        *pos = result.pos;
    }
    return result.valid;
}


int getMineshafts(int mc, uint64_t seed, int cx0, int cz0, int cx1, int cz1,
        Pos *out, int nout)
{
    const auto positions = list_mineshafts_impl(mc, seed, cx0, cz0, cx1, cz1);
    if (out != nullptr && nout > 0)
    {
        const auto to_copy = std::min(static_cast<std::size_t>(nout), positions.size());
        std::copy_n(positions.begin(), static_cast<std::ptrdiff_t>(to_copy), out);
    }
    return static_cast<int>(positions.size());
}

namespace {

struct EndIslandsResult
{
    std::array<EndIsland, 2> islands{};
    int count{};
};

auto get_end_islands_impl(int mc, uint64_t seed, int chunk_x, int chunk_z) -> EndIslandsResult
{
    StructureConfig sconf{};
    if (!getStructureConfig(End_Island, mc, &sconf))
        return {};

    const auto x = chunk_x * 16;
    const auto z = chunk_z * 16;
    uint64_t rng = getPopulationSeed(mc, seed, x, z);
    Xoroshiro xr{};
    float r = 0.0f;
    EndIslandsResult result{};

    if (mc <= MC_1_16)
    {
        setSeed(&rng, rng + sconf.salt);
        if (nextInt(&rng, static_cast<int>(sconf.rarity)) != 0)
            return {};
        result.islands[0].x = nextInt(&rng, 16) + x;
        result.islands[0].y = nextInt(&rng, 16) + 55;
        result.islands[0].z = nextInt(&rng, 16) + z;
        if (nextInt(&rng, 4) != 0)
        {
            result.islands[0].r = nextInt(&rng, 3) + 4;
            result.count = 1;
            return result;
        }
        result.islands[1].x = nextInt(&rng, 16) + x;
        result.islands[1].y = nextInt(&rng, 16) + 55;
        result.islands[1].z = nextInt(&rng, 16) + z;
        result.islands[0].r = nextInt(&rng, 3) + 4;
        for (r = result.islands[0].r; r > 0.5; r -= nextInt(&rng, 2) + 0.5f) {}
        result.islands[1].r = nextInt(&rng, 3) + 4;
        result.count = 2;
        return result;
    }
    if (mc <= MC_1_17)
    {
        setSeed(&rng, rng + sconf.salt);
        if (nextFloat(&rng) >= sconf.rarity)
            return {};
        const auto second = nextInt(&rng, 4) == 0;
        result.islands[0].x = nextInt(&rng, 16) + x;
        result.islands[0].z = nextInt(&rng, 16) + z;
        result.islands[0].y = nextInt(&rng, 16) + 55;
        result.islands[0].r = nextInt(&rng, 3) + 4;
        for (r = result.islands[0].r; r > 0.5; r -= nextInt(&rng, 2) + 0.5f) {}
        if (!second)
        {
            result.count = 1;
            return result;
        }
        result.islands[1].x = nextInt(&rng, 16) + x;
        result.islands[1].z = nextInt(&rng, 16) + z;
        result.islands[1].y = nextInt(&rng, 16) + 55;
        result.islands[1].r = nextInt(&rng, 3) + 4;
        result.count = 2;
        return result;
    }

    xSetSeed(&xr, rng + sconf.salt);
    if (xNextFloat(&xr) >= sconf.rarity)
        return {};
    const auto second = (xNextIntJ(&xr, 4) == 3);
    result.islands[0].x = xNextIntJ(&xr, 16) + x;
    result.islands[0].z = xNextIntJ(&xr, 16) + z;
    result.islands[0].y = xNextIntJ(&xr, 16) + 55;
    result.islands[0].r = xNextIntJ(&xr, 3) + 4;
    if (!second)
    {
        result.count = 1;
        return result;
    }
    for (r = result.islands[0].r; r > 0.5; r -= xNextIntJ(&xr, 2) + 0.5f) {}
    result.islands[1].x = xNextIntJ(&xr, 16) + x;
    result.islands[1].z = xNextIntJ(&xr, 16) + z;
    result.islands[1].y = xNextIntJ(&xr, 16) + 55;
    result.islands[1].r = xNextIntJ(&xr, 3) + 4;
    result.count = 2;
    return result;
}

} // namespace

int getEndIslands(EndIsland islands[2], int mc, uint64_t seed, int chunkX, int chunkZ)
{
    const auto result = get_end_islands_impl(mc, seed, chunkX, chunkZ);
    for (int i = 0; i < result.count; ++i)
    {
        islands[i] = result.islands[static_cast<std::size_t>(i)];
    }
    return result.count;
}

static void applyEndIslandHeight(float *y, const EndIsland *island,
    int x, int z, int w, int h, int scale)
{
    int r = island->r;
    int r2 = (r + 1) * (r + 1);
    int x0 = floordiv(island->x - r, scale);
    int z0 = floordiv(island->z - r, scale);
    int x1 = floordiv(island->x + r, scale);
    int z1 = floordiv(island->z + r, scale);
    int ds = 0;
    int i, j;
    for (j = z0; j <= z1; j++)
    {
        if (j < z || j >= z+h)
            continue;
        int dz = j * scale - island->z + ds;
        for (i = x0; i <= x1; i++)
        {
            if (i < x || i >= x+w)
                continue;
            int dx = i * scale - island->x + ds;
            if (dx*dx + dz*dz > r2)
                continue;
            int idx = (j - z) * w + (i - x);
            if (y[idx] < island->y)
                y[idx] = island->y;
        }
    }
}

int mapEndIslandHeight(float *y, const EndNoise *en, uint64_t seed,
    int x, int z, int w, int h, int scale)
{
    int rmax = (6 + scale - 1) / scale;
    int cx = floordiv(x - rmax, 16 / scale);
    int cz = floordiv(z - rmax, 16 / scale);
    int cw = floordiv(x + w + rmax, 16 / scale) - cx + 1;
    int ch = floordiv(z + h + rmax, 16 / scale) - cz + 1;
    int ci, cj;

    std::vector<int> ids(static_cast<std::size_t>(cw) * static_cast<std::size_t>(ch), 0);
    mapEndBiome(en, ids.data(), cx, cz, cw, ch);

    for (cj = 0; cj < ch; cj++)
    {
        for (ci = 0; ci < cw; ci++)
        {
            if (ids[static_cast<std::size_t>(cj*cw + ci)] != small_end_islands)
                continue;
            const auto islands = get_end_islands_impl(en->mc, seed, cx+ci, cz+cj);
            for (int n = islands.count - 1; n >= 0; --n)
                applyEndIslandHeight(y, &islands.islands[static_cast<std::size_t>(n)], x, z, w, h, scale);
        }
    }

    return 0;
}

float getEndHeightNoise(const EndNoise *en, int x, int z, int range);

int isEndChunkEmpty(const EndNoise *en, const SurfaceNoise *sn, uint64_t seed,
    int chunkX, int chunkZ)
{
    int i, j, k;
    int x = chunkX * 2;
    int z = chunkZ * 2;
    double depth[3][3];
    float y[256];

    // check if small end islands intersect this chunk
    for (j = -1; j <= +1; j++)
    {
        for (i = -1; i <= +1; i++)
        {
            const auto islands = get_end_islands_impl(en->mc, seed, chunkX+i, chunkZ+j);
            for (int n = islands.count - 1; n >= 0; --n)
            {
                const auto &island = islands.islands[static_cast<std::size_t>(n)];
                if (island.x + island.r <= chunkX*16) continue;
                if (island.z + island.r <= chunkZ*16) continue;
                if (island.x - island.r > chunkX*16 + 15) continue;
                if (island.z - island.r > chunkZ*16 + 15) continue;
                int id;
                mapEndBiome(en, &id, island.x >> 4, island.z >> 4, 1, 1);
                if (id == small_end_islands)
                    return 0;
            }
        }
    }

    // clamped (32 + 46 - y) / 64.0
    static const double upper_drop[] = {
           1.0,    1.0,    1.0,    1.0,    1.0,    1.0,    1.0,    1.0, // 0-7
           1.0,    1.0,    1.0,    1.0,    1.0,    1.0,    1.0, 63./64, // 8-15
        62./64, 61./64, 60./64, 59./64, 58./64, 57./64, 56./64, 55./64, // 16-23
        54./64, 53./64, 52./64, 51./64, 50./64, 49./64, 48./64, 47./64, // 24-31
        46./64 // 32
    };
    // clamped (y - 1) / 7.0
    static const double lower_drop[] = {
          0.0,  0.0, 1./7, 2./7, 3./7, 4./7, 5./7, 6./7, // 0-7
          1.0,  1.0,  1.0,  1.0,  1.0,  1.0,  1.0,  1.0, // 8-15
          1.0,  1.0,  1.0,  1.0,  1.0,  1.0,  1.0,  1.0, // 16-23
          1.0,  1.0,  1.0,  1.0,  1.0,  1.0,  1.0,  1.0, // 24-31
          1.0, // 32
    };
    // inverse of clamping func: ( 30 * (1-l) / l + 3000 * (1-u) ) / u
    static const double inverse_drop[] = {
        1e9, 1e9, 180.0, 75.0, 40.0, 22.5, 12.0, 5.0, // 0-7
        0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, // 8-14
        1000./21, 3000./31, 9000./61, 200.0, // 15-18
    };
    const double eps = 0.001;

    // get the inner depth values and see if they imply blocks in the chunk
    for (i = 0; i < 2; i++)
    {
        for (j = 0; j < 2; j++)
        {
            depth[i][j] = getEndHeightNoise(en, x+i, z+j, 0) - 8.0f;
            for (k = 8; k <= 14; k++)
            {
                double u = upper_drop[k];
                double l = lower_drop[k];
                double noise = depth[i][j];
                double pivot = inverse_drop[k] - noise;
                noise += sampleSurfaceNoiseBetween(sn, x+i, k, z+j, pivot-eps, pivot+eps);
                noise = cubiomes_lerp(u, -3000, noise);
                noise = cubiomes_lerp(l, -30, noise);
                if (noise > 0)
                    return 0;
            }
        }
    }

    // fill in the depth values at the boundaries to neighbouring chunks
    for (i = 0; i < 3; i++)
        depth[i][2] = getEndHeightNoise(en, x+i, z+2, 0) - 8.0f;
    for (j = 0; j < 2; j++)
        depth[2][j] = getEndHeightNoise(en, x+2, z+j, 0) - 8.0f;

    // see if none of the noise values can generate blocks
    auto needs_full_check = false;
    for (i = 0; i < 3 && !needs_full_check; i++)
    {
        for (j = 0; j < 3 && !needs_full_check; j++)
        {
            for (k = 2; k < 18; k++)
            {
                double u = upper_drop[k];
                double l = lower_drop[k];
                double noise = depth[i][j];
                double pivot = inverse_drop[k] - noise;
                noise += sampleSurfaceNoiseBetween(sn, x+i, k, z+j, pivot-eps, pivot+eps);
                noise = cubiomes_lerp(u, -3000, noise);
                noise = cubiomes_lerp(l, -30, noise);
                if (noise > 0) {
                    needs_full_check = true;
                    break;
                }
            }
        }
    }
    if (!needs_full_check) {
        return 1;
    }

    mapEndSurfaceHeight(y, en, sn, chunkX*16, chunkZ*16, 16, 16, 1, 0);
    for (k = 0; k < 256; k++)
        if (y[k] != 0) return 0;
    return 1;
}

//==============================================================================
// Checking Biomes & Biome Helper Functions
//==============================================================================


static int id_matches(int id, uint64_t b, uint64_t m)
{
    return id < 128 ? !!(b & (1ULL << id)) : !!(m & (1ULL << (id-128)));
}

Pos locateBiome(
    const Generator *g, int x, int y, int z, int radius,
    uint64_t validB, uint64_t validM, uint64_t *rng, int *passes)
{
    Pos out = {x, z};
    int i, j, found;
    found = 0;

    if (g->mc >= MC_1_18)
    {
        x >>= 2;
        z >>= 2;
        radius >>= 2;
        uint64_t dat = 0;

        for (j = -radius; j <= radius; j++)
        {
            for (i = -radius; i <= radius; i++)
            {
                // emulate order-dependent biome generation MC-241546
                //int id = getBiomeAt(g, 4, x+i, y, z+j);
                int id = sampleBiomeNoise(&g->bn, nullptr, x+i, y, z+j, &dat, 0);
                if (!id_matches(id, validB, validM))
                    continue;

                if (found == 0 || nextInt(rng, found+1) == 0)
                {
                    out.x = (x+i) * 4;
                    out.z = (z+j) * 4;
                }
                found++;
            }
        }
    }
    else
    {
        int x1 = (x-radius) >> 2;
        int z1 = (z-radius) >> 2;
        int x2 = (x+radius) >> 2;
        int z2 = (z+radius) >> 2;
        int width  = x2 - x1 + 1;
        int height = z2 - z1 + 1;

        Range r = {4, x1, z1, width, height, y, 1};
        std::vector<int> ids(static_cast<std::size_t>(getMinCacheSize(g, r.scale, r.sx, r.sy, r.sz)), 0);
        if (ids.empty() || genBiomes(g, ids.data(), r) != 0) {
            if (passes != nullptr) {
                *passes = found;
            }
            return out;
        }

        if (g->mc >= MC_1_13)
        {
            for (i = 0, j = 2; i < width*height; i++)
            {
                if (!id_matches(ids[static_cast<std::size_t>(i)], validB, validM))
                    continue;
                if (found == 0 || nextInt(rng, j++) == 0)
                {
                    out.x = (x1 + i%width) * 4;
                    out.z = (z1 + i/width) * 4;
                    found = 1;
                }
            }
            found = j - 2;
        }
        else
        {
            for (i = 0; i < width*height; i++)
            {
                if (!id_matches(ids[static_cast<std::size_t>(i)], validB, validM))
                    continue;
                if (found == 0 || nextInt(rng, found + 1) == 0)
                {
                    out.x = (x1 + i%width) * 4;
                    out.z = (z1 + i/width) * 4;
                    ++found;
                }
            }
        }
    }


    if (passes != nullptr)
    {
        *passes = found;
    }

    return out;
}


int areBiomesViable(
    const Generator *g, int x, int y, int z, int rad,
    uint64_t validB, uint64_t validM, int approx)
{
    int x1 = (x - rad) >> 2, x2 = (x + rad) >> 2, sx = x2 - x1 + 1;
    int z1 = (z - rad) >> 2, z2 = (z + rad) >> 2, sz = z2 - z1 + 1;
    int i, j, id;

    // In 1.18+ the area is also checked in y, forming a cube volume.
    // However, this function is only used for monuments, which need ocean or
    // river, where we can get away with just checking the lowest y for caves.
    y = (y - rad) >> 2;

    // check corners
    Pos corners[4] = { {x1,z1}, {x2,z2}, {x1,z2}, {x2,z1} };
    for (i = 0; i < 4; i++)
    {
        id = getBiomeAt(g, 4, corners[i].x, y, corners[i].z);
        if (id < 0 || !id_matches(id, validB, validM))
            return 0;
    }
    if (approx >= 1)
        return 1;

    if (g->mc >= MC_1_18)
    {
        for (i = 0; i < sx; i++)
        {
            uint64_t dat = 0;
            for (j = 0; j < sz; j++)
            {
                if (g->mc >= MC_1_18)
                    id = sampleBiomeNoise(&g->bn, nullptr, x1+i, y, z1+j, &dat, 0);
                else
                    id = getBiomeAt(g, 4, x1+i, y, z1+j);
                if (id < 0 || !id_matches(id, validB, validM))
                    return 0;
            }
        }
    }
    else
    {
        Range r = {4, x1, z1, sx, sz, y, 1};
        std::vector<int> ids(static_cast<std::size_t>(getMinCacheSize(g, r.scale, r.sx, r.sy, r.sz)), 0);
        if (ids.empty() || genBiomes(g, ids.data(), r))
            return 0;
        for (i = 0; i < sx*sz; i++)
        {
            if (ids[static_cast<std::size_t>(i)] < 0 || !id_matches(ids[static_cast<std::size_t>(i)], validB, validM))
                return 0;
        }
    }

    return 1;
}


//==============================================================================
// Finding Strongholds and Spawn
//==============================================================================


int isStrongholdBiome(int mc, int id)
{
    if (!isOverworld(mc, id))
        return 0;
    if (isOceanic(id))
        return 0;
    switch (id)
    {
    case plains:
    case mushroom_fields:
    case taiga_hills:
        return mc >= MC_1_7;
    case swamp:
        return mc <= MC_1_6;
    case river:
    case frozen_river:
    case beach:
    case snowy_beach:
    case swamp_hills:
        return 0;
    case mushroom_field_shore:
        return mc >= MC_1_13;
    case stone_shore:
        return mc <= MC_1_17;
    case bamboo_jungle:
    case bamboo_jungle_hills:
        // simulate MC-199298
        return mc <= MC_1_15 || mc >= MC_1_18;
    case mangrove_swamp:
    case deep_dark:
        return 0;
    default:
        return 1;
    }
}

Pos initFirstStronghold(StrongholdIter *sh, int mc, uint64_t s48)
{
    double dist, angle;
    uint64_t rnds;
    Pos p;

    setSeed(&rnds, s48);

    angle = 2.0 * PI * nextDouble(&rnds);
    if (mc >= MC_1_9)
        dist = (4.0 * 32.0) + (nextDouble(&rnds) - 0.5) * 32 * 2.5;
    else
        dist = (1.25 + nextDouble(&rnds)) * 32.0;

    p.x = ((int)round(cos(angle) * dist) * 16) + 8;
    p.z = ((int)round(sin(angle) * dist) * 16) + 8;

    if (sh)
    {
        sh->pos.x = sh->pos.z = 0;
        sh->nextapprox = p;
        sh->index = 0;
        sh->ringnum = 0;
        sh->ringmax = 3;
        sh->ringidx = 0;
        sh->angle = angle;
        sh->dist = dist;
        sh->rnds = rnds;
        sh->mc = mc;
    }

    return p;
}

int nextStronghold(StrongholdIter *sh, const Generator *g)
{
    uint64_t validB = 0, validM = 0;
    int i;
    for (i = 0; i < 64; i++)
    {
        if (isStrongholdBiome(sh->mc, i))
            validB |= (1ULL << i);
        if (isStrongholdBiome(sh->mc, i+128))
            validM |= (1ULL << i);
    }

    if (sh->mc > MC_1_19_2)
    {
        if (g)
        {
            uint64_t lbr = sh->rnds;
            setSeed(&lbr, nextLong(&sh->rnds));
            sh->pos = locateBiome(g, sh->nextapprox.x, 0, sh->nextapprox.z, 112,
                validB, validM, &lbr, nullptr);
        }
        else
        {
            nextLong(&sh->rnds);
            sh->pos = sh->nextapprox;
        }
    }
    else if (sh->mc >= MC_B1_8)
    {
        sh->pos = locateBiome(g, sh->nextapprox.x, 0, sh->nextapprox.z, 112,
            validB, validM, &sh->rnds, nullptr);
    }
    else
    {
        return 0;
    }
    // staircase is located at (4, 4) in chunk
    sh->pos.x = (sh->pos.x & ~15) + 4;
    sh->pos.z = (sh->pos.z & ~15) + 4;

    sh->ringidx++;
    sh->angle += 2 * PI / sh->ringmax;

    if (sh->ringidx == sh->ringmax)
    {
        sh->ringnum++;
        sh->ringidx = 0;
        sh->ringmax = sh->ringmax + 2*sh->ringmax / (sh->ringnum+1);
        if (sh->ringmax > 128-sh->index)
            sh->ringmax = 128-sh->index;
        sh->angle += nextDouble(&sh->rnds) * PI * 2.0;
    }

    if (sh->mc >= MC_1_9)
    {
        sh->dist = (4.0 * 32.0) + (6.0 * sh->ringnum * 32.0) +
            (nextDouble(&sh->rnds) - 0.5) * 32 * 2.5;
    }
    else
    {
        sh->dist = (1.25 + nextDouble(&sh->rnds)) * 32.0;
    }

    sh->nextapprox.x = ((int)round(cos(sh->angle) * sh->dist) * 16) + 8;
    sh->nextapprox.z = ((int)round(sin(sh->angle) * sh->dist) * 16) + 8;
    sh->index++;

    return (sh->mc >= MC_1_9 ? 128 : 3) - (sh->index-1);
}


static
uint64_t calcFitness(const Generator *g, int x, int z)
{
    int64_t np[6];
    uint32_t flags = SAMPLE_NO_DEPTH | SAMPLE_NO_BIOME;
    sampleBiomeNoise(&g->bn, np, x>>2, 0, z>>2, nullptr, flags);
    static constexpr std::array<std::array<int64_t, 2>, 7> spawn_np{{
        {-10000,10000},{-10000,10000},{-1100,10000},{-10000,10000},{0,0},
        {-10000,-1600},{1600,10000} // [6]: weirdness for the second noise point
    }};
    uint64_t ds = 0, ds1 = 0, ds2 = 0;
    uint64_t a, b, q, i;
    for (i = 0; i < 5; i++)
    {
        a = +np[i] - static_cast<uint64_t>(spawn_np[static_cast<std::size_t>(i)][1]);
        b = -np[i] + static_cast<uint64_t>(spawn_np[static_cast<std::size_t>(i)][0]);
        q = (int64_t)a > 0 ? a : (int64_t)b > 0 ? b : 0;
        ds += q * q;
    }
    a = +np[5] - static_cast<uint64_t>(spawn_np[5][1]);
    b = -np[5] + static_cast<uint64_t>(spawn_np[5][0]);
    q = (int64_t)a > 0 ? a : (int64_t)b > 0 ? b : 0;
    ds1 = ds + q*q;
    a = +np[5] - static_cast<uint64_t>(spawn_np[6][1]);
    b = -np[5] + static_cast<uint64_t>(spawn_np[6][0]);
    q = (int64_t)a > 0 ? a : (int64_t)b > 0 ? b : 0;
    ds2 = ds + q*q;
    ds = ds1 <= ds2 ? ds1 : ds2;
    // apply dependence on distance from origin
    a = (int64_t)x*x;
    b = (int64_t)z*z;
    if (g->mc <= MC_1_21_1)
    {
        double s = (double)(a + b) / (2500 * 2500);
        q = (uint64_t)(s*s * 1e8) + ds;
    }
    else
    {
        q = ds * (2048LL * 2048LL) + a + b;
    }
    return q;
}

static
auto find_fittest_impl(const Generator *g, Pos start, uint64_t initial_fitness, double maxrad, double step)
    -> std::pair<Pos, uint64_t>
{
    auto pos = start;
    auto fitness = initial_fitness;
    double rad, ang;
    const auto p = start;
    for (rad = step; rad <= maxrad; rad += step)
    {
        for (ang = 0; ang <= PI*2; ang += step/rad)
        {
            int x = p.x + (int)(sin(ang) * rad);
            int z = p.z + (int)(cos(ang) * rad);
            uint64_t fit = calcFitness(g, x, z);
            // Then update pos and fitness if combined total is lower/better
            if (fit < fitness)
            {
                pos = Pos{.x = x, .z = z};
                fitness = fit;
            }
        }
    }
    return {pos, fitness};
}

static
Pos findFittestPos(const Generator *g)
{
    Pos spawn = {0, 0};
    uint64_t fitness = calcFitness(g, 0, 0);
    std::tie(spawn, fitness) = find_fittest_impl(g, spawn, fitness, 2048.0, 512.0);
    std::tie(spawn, fitness) = find_fittest_impl(g, spawn, fitness, 512.0, 32.0);
    // center of chunk
    spawn.x = (spawn.x & ~15) + 8;
    spawn.z = (spawn.z & ~15) + 8;
    return spawn;
}

// valid spawn biomes up to 1.17
static const uint64_t g_spawn_biomes_17 =
    (1ULL << forest) |
    (1ULL << plains) |
    (1ULL << taiga) |
    (1ULL << taiga_hills) |
    (1ULL << wooded_hills) |
    (1ULL << jungle) |
    (1ULL << jungle_hills);


Pos estimateSpawn(const Generator *g, uint64_t *rng)
{
    Pos spawn = {0, 0};

    if (g->mc <= MC_B1_7)
    {
        // finds a random sandblock (location is not fixed)
        return spawn;
    }
    else if (g->mc <= MC_1_17)
    {
        int found;
        uint64_t spawn_biomes = g_spawn_biomes_17;
        if (g->mc <= MC_1_0)
            spawn_biomes = (1ULL << forest) | (1ULL << swamp) |(1ULL << taiga);
        uint64_t s;
        setSeed(&s, g->seed);
        spawn = locateBiome(g, 0, 63, 0, 256, spawn_biomes, 0, &s, &found);
        if (!found)
            spawn.x = spawn.z = 8;
        if (rng)
            *rng = s;
    }
    else
    {
        spawn = findFittestPos(g);
    }

    return spawn;
}

Pos getSpawn(const Generator *g)
{
    uint64_t rng;
    Pos spawn = estimateSpawn(g, &rng);
    int i, j, k, u, v, cx0, cz0;
    uint32_t ii, jj;

    if (g->mc <= MC_B1_7)
        return spawn;

    SurfaceNoise sn;
    initSurfaceNoise(&sn, DIM_OVERWORLD, g->seed);

    if (g->mc <= MC_1_12)
    {
        for (i = 0; i < 1000; i++)
        {
            float y;
            int id, grass = 0;
            mapApproxHeight(&y, &id, g, &sn, spawn.x >> 2, spawn.z >> 2, 1, 1);
            getBiomeDepthAndScale(id, 0, 0, &grass);
            if (grass > 0 && y >= grass)
                break;
            spawn.x += nextInt(&rng, 64) - nextInt(&rng, 64);
            spawn.z += nextInt(&rng, 64) - nextInt(&rng, 64);
        }
    }
    else if (g->mc <= MC_1_17)
    {
        j = k = u = 0;
        v = -1;
        for (i = 0; i < 1024; i++)
        {
            if (j > -16 && j <= 16 && k > -16 && k <= 16)
            {
                // find server spawn point in chunk
                float y[16];
                int ids[16];
                cx0 = (spawn.x & ~15) + j * 16; // start of chunk
                cz0 = (spawn.z & ~15) + k * 16;
                mapApproxHeight(y, ids, g, &sn, cx0 >> 2, cz0 >> 2, 4, 4);
                for (ii = 0; ii < 4; ii++)
                {
                    for (jj = 0; jj < 4; jj++)
                    {
                        int grass = 0;
                        getBiomeDepthAndScale(ids[jj*4+ii], 0, 0, &grass);
                        if (grass <= 0 || y[jj*4+ii] < grass)
                            continue;
                        spawn.x = cx0 + ii * 4;
                        spawn.z = cz0 + jj * 4;
                        return spawn;
                    }
                }
            }
            if (j == k || (j < 0 && j == -k) || (j > 0 && j == 1 - k))
            {
                int tmp = u;
                u = -v;
                v = tmp;
            }
            j += u;
            k += v;
        }
        // chunk center
        spawn.x = (spawn.x & ~15) + 8;
        spawn.z = (spawn.z & ~15) + 8;
    }
    else
    {
        j = k = u = 0;
        v = -1;
        for (i = 0; i < 121; i++)
        {
            if (j >= -5 && j <= 5 && k >= -5 && k <= 5)
            {
                // find server spawn point in chunk
                cx0 = (spawn.x & ~15) + j * 16;
                cz0 = (spawn.z & ~15) + k * 16;
                for (ii = 0; ii < 4; ii++)
                {
                    for (jj = 0; jj < 4; jj++)
                    {
                        float y;
                        int id;
                        int x = cx0 + ii * 4;
                        int z = cz0 + jj * 4;
                        mapApproxHeight(&y, &id, g, &sn, x >> 2, z >> 2, 1, 1);
                        if (y > 63 || id == frozen_ocean ||
                            id == deep_frozen_ocean || id == frozen_river)
                        {
                            spawn.x = x;
                            spawn.z = z;
                            return spawn;
                        }
                    }
                }
            }
            if (j == k || (j < 0 && j == -k) || (j > 0 && j == 1 - k))
            {
                int tmp = u;
                u = -v;
                v = tmp;
            }
            j += u;
            k += v;
        }
        // chunk center
        spawn.x = (spawn.x & ~15) + 8;
        spawn.z = (spawn.z & ~15) + 8;
    }

    return spawn;
}



//==============================================================================
// Validating Structure Positions
//==============================================================================


int isViableFeatureBiome(int mc, int structureType, int biomeID)
{
    switch (structureType)
    {
    case Desert_Pyramid:
        return biomeID == desert || biomeID == desert_hills;

    case Jungle_Pyramid:
        return (biomeID == jungle || biomeID == jungle_hills ||
                biomeID == bamboo_jungle || biomeID == bamboo_jungle_hills);

    case Swamp_Hut:
        return biomeID == swamp;

    case Igloo:
        if (mc <= MC_1_8) return 0;
        return biomeID == snowy_tundra || biomeID == snowy_taiga || biomeID == snowy_slopes;

    case Ocean_Ruin:
        if (mc <= MC_1_12) return 0;
        return isOceanic(biomeID);

    case Shipwreck:
        if (mc <= MC_1_12) return 0;
        return isOceanic(biomeID) || biomeID == beach || biomeID == snowy_beach;

    case Ruined_Portal:
    case Ruined_Portal_N:
        return mc >= MC_1_16_1;

    case Ancient_City:
        if (mc <= MC_1_18) return 0;
        return biomeID == deep_dark;

    case Trail_Ruins:
        if (mc <= MC_1_19) return 0;
        else {
            switch (biomeID) {
            case taiga:
            case snowy_taiga:
            case old_growth_pine_taiga:
            case old_growth_spruce_taiga:
            case old_growth_birch_forest:
            case jungle:
                return 1;
            default:
                return 0;
            }
        }

    case Trial_Chambers:
        if (mc <= MC_1_20) return 0;
        return biomeID != deep_dark && isOverworld(mc, biomeID);

    case Treasure:
        if (mc <= MC_1_12) return 0;
        return biomeID == beach || biomeID == snowy_beach;

    case Mineshaft:
        return isOverworld(mc, biomeID);

    case Desert_Well:
        return biomeID == desert;

    case Monument:
        if (mc <= MC_1_7) return 0;
        return isDeepOcean(biomeID);

    case Outpost:
        if (mc <= MC_1_13) return 0;
        if (mc >= MC_1_18) {
            switch (biomeID) {
            case desert:
            case plains:
            case savanna:
            case snowy_plains:
            case taiga:
            case meadow:
            case frozen_peaks:
            case jagged_peaks:
            case stony_peaks:
            case snowy_slopes:
            case grove:
            case cherry_grove:
                return 1;
            default:
                return 0;
            }
        }
        // fall through
    case Village:
        if (biomeID == plains || biomeID == desert || biomeID == savanna)
            return 1;
        if (mc >= MC_1_10 && biomeID == taiga)
            return 1;
        if (mc >= MC_1_14 && biomeID == snowy_tundra)
            return 1;
        if (mc >= MC_1_18 && biomeID == meadow)
            return 1;
        return 0;

    case Mansion:
        if (mc <= MC_1_10) return 0;
        return biomeID == dark_forest || biomeID == dark_forest_hills;

    case Fortress:
        return (biomeID == nether_wastes || biomeID == soul_sand_valley ||
                biomeID == warped_forest || biomeID == crimson_forest ||
                biomeID == basalt_deltas);

    case Bastion:
        if (mc <= MC_1_15) return 0;
        return (biomeID == nether_wastes || biomeID == soul_sand_valley ||
                biomeID == warped_forest || biomeID == crimson_forest);

    case End_City:
        if (mc <= MC_1_8) return 0;
        return biomeID == end_midlands || biomeID == end_highlands;

    case End_Gateway:
        if (mc <= MC_1_12) return 0;
        return biomeID == end_highlands;

    default:
        fprintf(stderr,
                "isViableFeatureBiome: not implemented for structure type %d.\n",
                structureType);
        exit(1);
    }
    return 0;
}


static int mapViableBiome(const Layer * l, int * out, int x, int z, int w, int h)
{
    int err = mapBiome(l, out, x, z, w, h);
    if unlikely(err != 0)
        return err;

    int styp = ((const int*) l->data)[0];
    int i, j;

    for (j = 0; j < h; j++)
    {
        for (i = 0; i < w; i++)
        {
            int biomeID = out[i + w*j];
            switch (styp)
            {
            case Desert_Pyramid:
            case Desert_Well:
                if (biomeID == desert || isMesa(biomeID))
                    return 0;
                break;
            case Jungle_Pyramid:
                if (biomeID == jungle)
                    return 0;
                break;
            case Swamp_Hut:
                if (biomeID == swamp)
                    return 0;
                break;
            case Igloo:
                if (biomeID == snowy_tundra || biomeID == snowy_taiga)
                    return 0;
                break;
            case Treasure:
                if (isOceanic(biomeID))
                    return 0;
                break;
            case Ocean_Ruin:
            case Shipwreck:
            case Monument:
                if (isOceanic(biomeID))
                    return 0;
                break;
            case Mansion:
                if (biomeID == dark_forest)
                    return 0;
                break;
            default:
                return 0;
            }
        }
    }

    return 1; // required biomes not found: set err status to stop generator
}

static int mapViableShore(const Layer * l, int * out, int x, int z, int w, int h)
{
    int err = mapShore(l, out, x, z, w, h);
    if unlikely(err != 0)
        return err;

    int styp = ((const int*) l->data)[0];
    int mc   = ((const int*) l->data)[1];
    int i, j;

    for (j = 0; j < h; j++)
    {
        for (i = 0; i < w; i++)
        {
            int biomeID = out[i + w*j];
            switch (styp)
            {
            case Desert_Pyramid:
            case Jungle_Pyramid:
            case Swamp_Hut:
            case Igloo:
            case Ocean_Ruin:
            case Shipwreck:
            case Village:
            case Monument:
            case Mansion:
            case Treasure:
            case Desert_Well:
                if (isViableFeatureBiome(mc, styp, biomeID))
                    return 0;
                break;

            default:
                return 0;
            }
        }
    }

    return 1;
}


static const uint64_t g_monument_biomes2 =
    (1ULL << deep_frozen_ocean) |
    (1ULL << deep_cold_ocean) |
    (1ULL << deep_ocean) |
    (1ULL << deep_lukewarm_ocean) |
    (1ULL << deep_warm_ocean);

static const uint64_t g_monument_biomes1 =
    (1ULL << ocean) |
    (1ULL << deep_ocean) |
    (1ULL << river) |
    (1ULL << frozen_river) |
    (1ULL << frozen_ocean) |
    (1ULL << deep_frozen_ocean) |
    (1ULL << cold_ocean) |
    (1ULL << deep_cold_ocean) |
    (1ULL << lukewarm_ocean) |
    (1ULL << deep_lukewarm_ocean) |
    (1ULL << warm_ocean) |
    (1ULL << deep_warm_ocean);

int isViableStructurePos(int structureType, Generator *g, int x, int z, uint32_t flags)
{
    constexpr auto approx = 0; // enables approximation levels
    const int64_t chunkX = x >> 4;
    const int64_t chunkZ = z >> 4;

    // Structures are positioned at the chunk origin, but the biome check is
    // performed near the middle of the chunk [(9,9) in 1.13, TODO: check 1.7]
    // In 1.16 the biome check is always performed at (2,2) with layer scale=4.
    if (g->dim == DIM_NETHER)
    {
        if (structureType == Fortress && g->mc <= MC_1_17)
            return 1;
        if (g->mc <= MC_1_15)
            return 0;
        if (structureType == Ruined_Portal_N)
            return 1;
        if (structureType == Fortress)
        {   // in 1.18+ fortresses generate everywhere, where bastions don't
            StructureConfig sc{};
            if (!getStructureConfig(Fortress, g->mc, &sc))
                return 0;
            Pos rp = {
                floordiv(x, sc.regionSize << 4),
                floordiv(z, sc.regionSize << 4)
            };
            if (!getStructurePos(Bastion, g->mc, g->seed, rp.x, rp.z, &rp))
                return 1;
            return !isViableStructurePos(Bastion, g, x, z, flags);
        }

        auto sampleY = 0;
        auto sampleX = 0;
        auto sampleZ = 0;
        if (g->mc >= MC_1_18 && structureType == Bastion)
        {
            StructureVariant sv{};
            getVariant(&sv, Bastion, g->mc, g->seed, x, z, -1);
            sampleX = (chunkX*32 + 2*sv.x + sv.sx-1) / 2 >> 2;
            sampleZ = (chunkZ*32 + 2*sv.z + sv.sz-1) / 2 >> 2;
            if (g->mc >= MC_1_19_2)
                sampleY = 33 >> 2; // nether biomes don't actually vary in Y
        }
        else
        {
            sampleX = (chunkX * 4) + 2;
            sampleZ = (chunkZ * 4) + 2;
        }
        const auto id = getBiomeAt(g, 4, sampleX, sampleY, sampleZ);
        return isViableFeatureBiome(g->mc, structureType, id);
    }

    if (g->dim == DIM_END)
    {
        switch (structureType)
        {
        case End_City:
            if (g->mc <= MC_1_8) return 0;
            break;
        case End_Gateway:
            if (g->mc <= MC_1_12) return 0;
            break;
        default:
            return 0;
        }
        // End biomes vary only on a per-chunk scale (1:16)
        // voronoi pre-1.15 shouldn't matter for End Cities as the check will
        // be near the chunk center
        const auto id = getBiomeAt(g, 16, chunkX, 0, chunkZ);
        return isViableFeatureBiome(g->mc, structureType, id) ? id : 0;
    }

    // Overworld fast paths that do not require biome-layer overrides.
    if (structureType == Mineshaft) {
        return 1;
    }
    if (structureType == Ruined_Portal || structureType == Ruined_Portal_N) {
        return g->mc > MC_1_15 ? 1 : 0;
    }
    if (structureType == Geode) {
        return g->mc > MC_1_16 ? 1 : 0;
    }

    struct OverworldLayerOverride
    {
        Generator *g;
        Layer lbiome{};
        Layer lshore{};
        Layer *entry{nullptr};
        bool active{false};

        OverworldLayerOverride(Generator *gen, int *data) : g(gen)
        {
            if (g->mc > MC_1_17) {
                return;
            }
            active = true;
            lbiome = g->layered.ls.layers[L_BIOME_256];
            lshore = g->layered.ls.layers[L_SHORE_16];
            entry = g->layered.entry;
            g->layered.ls.layers[L_BIOME_256].data = data;
            g->layered.ls.layers[L_BIOME_256].getMap = mapViableBiome;
            g->layered.ls.layers[L_SHORE_16].data = data;
            g->layered.ls.layers[L_SHORE_16].getMap = mapViableShore;
        }

        ~OverworldLayerOverride()
        {
            if (!active) {
                return;
            }
            g->layered.ls.layers[L_BIOME_256] = lbiome;
            g->layered.ls.layers[L_SHORE_16] = lshore;
            g->layered.entry = entry;
        }
    };
    std::array<int, 2> layer_filter_data{structureType, g->mc};
    OverworldLayerOverride scoped_override{g, layer_filter_data.data()};

    auto check_feature_like = [&]() -> int {
        auto sampleX = 0;
        auto sampleZ = 0;
        if (g->mc <= MC_1_15)
        {
            g->layered.entry = &g->layered.ls.layers[L_VORONOI_1];
            sampleX = chunkX * 16 + 9;
            sampleZ = chunkZ * 16 + 9;
        }
        else
        {
            if (g->mc <= MC_1_17)
                g->layered.entry = &g->layered.ls.layers[L_RIVER_MIX_4];
            sampleX = chunkX * 4 + 2;
            sampleZ = chunkZ * 4 + 2;
        }
        const auto id = getBiomeAt(g, 0, sampleX, 319>>2, sampleZ);
        return (id >= 0 && isViableFeatureBiome(g->mc, structureType, id)) ? 1 : 0;
    };

    switch (structureType)
    {
    case Trail_Ruins:
        return g->mc > MC_1_19 ? check_feature_like() : 0;
    case Ocean_Ruin:
    case Shipwreck:
    case Treasure:
        return g->mc > MC_1_12 ? check_feature_like() : 0;
    case Igloo:
        return g->mc > MC_1_8 ? check_feature_like() : 0;
    case Desert_Pyramid:
    case Jungle_Pyramid:
    case Swamp_Hut:
        return check_feature_like();

    case Desert_Well:
    {
        auto sampleX = 0;
        auto sampleZ = 0;
        if (g->mc <= MC_1_15)
        {
            g->layered.entry = &g->layered.ls.layers[L_VORONOI_1];
            sampleX = x;
            sampleZ = z;
        }
        else
        {
            if (g->mc <= MC_1_17)
                g->layered.entry = &g->layered.ls.layers[L_RIVER_MIX_4];
            sampleX = x >> 2;
            sampleZ = z >> 2;
        }
        const auto id = getBiomeAt(g, 0, sampleX, 319>>2, sampleZ);
        return (id >= 0 && isViableFeatureBiome(g->mc, structureType, id)) ? 1 : 0;
    }

    case Village:
    {
        if (g->mc <= MC_1_17)
        {
            auto sampleX = 0;
            auto sampleZ = 0;
            if (g->mc == MC_1_15)
            {   // exclusively in MC_1_15, villages used the same biome check
                // as other structures
                g->layered.entry = &g->layered.ls.layers[L_VORONOI_1];
                sampleX = chunkX * 16 + 9;
                sampleZ = chunkZ * 16 + 9;
            }
            else
            {
                g->layered.entry = &g->layered.ls.layers[L_RIVER_MIX_4];
                sampleX = chunkX * 4 + 2;
                sampleZ = chunkZ * 4 + 2;
            }
            auto id = getBiomeAt(g, 0, sampleX, 0, sampleZ);
            if (id < 0 || !isViableFeatureBiome(g->mc, structureType, id))
                return 0;
            if (flags && static_cast<uint32_t>(id) != flags)
                return 0;
            if (g->mc <= MC_1_9)
            {   // before MC_1_10 villages did not spread into invalid biomes,
                // which could cause them to fail to generate on the first
                // check at block (2, 2) in the starting chunk
                sampleX = chunkX * 16 + 2;
                sampleZ = chunkZ * 16 + 2;
                id = getBiomeAt(g, 1, sampleX, 0, sampleZ);
                if (id < 0 || !isViableFeatureBiome(g->mc, structureType, id))
                    return 0;
            }
            return id; // biome for viability, useful for further analysis
        }

        constexpr std::array<int, 5> village_biomes{plains, desert, savanna, taiga, snowy_tundra};
        for (const auto biome : village_biomes)
        {
            if (flags && flags != static_cast<uint32_t>(biome))
                continue;
            StructureVariant sv{};
            getVariant(&sv, Village, g->mc, g->seed, x, z, biome);
            const auto sampleX = (chunkX*32 + 2*sv.x + sv.sx-1) / 2 >> 2;
            const auto sampleZ = (chunkZ*32 + 2*sv.z + sv.sz-1) / 2 >> 2;
            const auto id = getBiomeAt(g, 0, sampleX, 319 >> 2, sampleZ);
            if (id == biome || (id == meadow && biome == plains))
                return biome;
        }
        return 0;
    }

    case Outpost:
    {
        if (g->mc <= MC_1_13)
            return 0;

        uint64_t rng = g->seed;
        setAttemptSeed(&rng, chunkX, chunkZ);
        if (nextInt(&rng, 5) != 0)
            return 0;

        // look for villages within 10 chunks
        StructureConfig vilconf{};
        if (!getStructureConfig(Village, g->mc, &vilconf))
            return 0;
        const auto cx0 = (chunkX-10);
        const auto cx1 = (chunkX+10);
        const auto cz0 = (chunkZ-10);
        const auto cz1 = (chunkZ+10);
        const auto rx0 = floordiv(cx0, vilconf.regionSize);
        const auto rx1 = floordiv(cx1, vilconf.regionSize);
        const auto rz0 = floordiv(cz0, vilconf.regionSize);
        const auto rz1 = floordiv(cz1, vilconf.regionSize);
        for (auto rz = rz0; rz <= rz1; rz++)
        {
            for (auto rx = rx0; rx <= rx1; rx++)
            {
                const auto p = getFeaturePos(vilconf, g->seed, rx, rz);
                const auto cx = p.x >> 4;
                const auto cz = p.z >> 4;
                if (cx >= cx0 && cx <= cx1 && cz >= cz0 && cz <= cz1)
                {
                    if (g->mc >= MC_1_16_1)
                        return 0;
                    if (isViableStructurePos(Village, g, p.x, p.z, 0))
                        return 0;
                }
            }
        }

        auto sampleX = 0;
        auto sampleZ = 0;
        if (g->mc >= MC_1_18)
        {
            rng = chunkGenerateRnd(g->seed, chunkX, chunkZ);
            switch (nextInt(&rng, 4)) {
            case 0: sampleX = +15; sampleZ = +15; break;
            case 1: sampleX = -15; sampleZ = +15; break;
            case 2: sampleX = -15; sampleZ = -15; break;
            case 3: sampleX = +15; sampleZ = -15; break;
            default: std::unreachable();
            }
            sampleX = (chunkX * 32 + sampleX) / 2 >> 2;
            sampleZ = (chunkZ * 32 + sampleZ) / 2 >> 2;
        }
        else if (g->mc >= MC_1_16_1)
        {
            g->layered.entry = &g->layered.ls.layers[L_RIVER_MIX_4];
            sampleX = chunkX * 4 + 2;
            sampleZ = chunkZ * 4 + 2;
        }
        else
        {
            g->layered.entry = &g->layered.ls.layers[L_VORONOI_1];
            sampleX = chunkX * 16 + 9;
            sampleZ = chunkZ * 16 + 9;
        }

        const auto id = getBiomeAt(g, 0, sampleX, 319>>2, sampleZ);
        return (id >= 0 && isViableFeatureBiome(g->mc, structureType, id)) ? 1 : 0;
    }

    case Monument:
    {
        if (g->mc <= MC_1_7)
            return 0;
        if (g->mc == MC_1_8)
        {   // In 1.8 monuments require only a single deep ocean block.
            const auto id = getBiomeAt(g, 1, chunkX * 16 + 8, 0, chunkZ * 16 + 8);
            if (id < 0 || !isDeepOcean(id))
                return 0;
        }
        else if (g->mc <= MC_1_17)
        {   // Monuments require two viability checks with the ocean layer
            // branch => worth checking for potential deep ocean beforehand.
            g->layered.entry = &g->layered.ls.layers[L_SHORE_16];
            const auto id = getBiomeAt(g, 0, chunkX, 0, chunkZ);
            if (id < 0 || !isDeepOcean(id))
                return 0;
        }

        const auto sampleX = chunkX * 16 + 8;
        const auto sampleZ = chunkZ * 16 + 8;
        if (g->mc >= MC_1_9 && g->mc <= MC_1_17)
        {   // check for deep ocean center
            if (!areBiomesViable(g, sampleX, 63, sampleZ, 16, g_monument_biomes2, 0, approx))
                return 0;
        }
        else if (g->mc >= MC_1_18)
        {   // check is done at y level of ocean floor - approx. with y = 36
            const auto id = getBiomeAt(g, 4, sampleX>>2, 36>>2, sampleZ>>2);
            if (!isDeepOcean(id))
                return 0;
        }

        return areBiomesViable(g, sampleX, 63, sampleZ, 29, g_monument_biomes1, 0, approx) ? 1 : 0;
    }

    case Mansion:
    {
        if (g->mc <= MC_1_10)
            return 0;
        if (g->mc <= MC_1_17)
        {
            const auto sampleX = chunkX * 16 + 8;
            const auto sampleZ = chunkZ * 16 + 8;
            const uint64_t b = (1ULL << dark_forest);
            const uint64_t m = (1ULL << (dark_forest_hills-128));
            if (!areBiomesViable(g, sampleX, 0, sampleZ, 32, b, m, approx))
                return 0;
            return 1;
        }
        // In 1.18 the generation gets the minimum surface height among the
        // four structure corners (note structure has rotation).
        // This minimum height has to be y >= 60. The biome check is done
        // at the center position at that height.
        // TODO: get surface height
        const auto sampleX = chunkX * 16 + 7;
        const auto sampleZ = chunkZ * 16 + 7;
        const auto id = getBiomeAt(g, 4, sampleX>>2, 319>>2, sampleZ>>2);
        return (id >= 0 && isViableFeatureBiome(g->mc, structureType, id)) ? 1 : 0;
    }

    case Ancient_City:
        if (g->mc <= MC_1_18)
            return 0;
        [[fallthrough]];

    case Trial_Chambers:
    {
        if (structureType == Trial_Chambers && g->mc <= MC_1_20)
            return 0;
        StructureVariant sv{};
        getVariant(&sv, structureType, g->mc, g->seed, x, z, -1);
        const auto sampleX = (chunkX*32 + 2*sv.x + sv.sx - 1) / 2 >> 2;
        const auto sampleZ = (chunkZ*32 + 2*sv.z + sv.sz - 1) / 2 >> 2;
        const auto sampleY = sv.y >> 2;
        const auto id = getBiomeAt(g, 4, sampleX, sampleY, sampleZ);
        return (id >= 0 && isViableFeatureBiome(g->mc, structureType, id)) ? 1 : 0;
    }

    default:
        fprintf(stderr,
                "isViableStructurePos: bad structure type %d or dimension %d\n",
                structureType, g->dim);
        return 0;
    }
}


int isViableStructureTerrain(int structType, Generator *g, int x, int z)
{
    int sx, sz;
    if (g->mc <= MC_1_17)
        return 1;
    if (structType == Desert_Pyramid || structType == Jungle_Temple)
    {
        sx = (structType == Desert_Pyramid ? 21 : 12);
        sz = (structType == Desert_Pyramid ? 21 : 15);
    }
    else if (structType == Mansion)
    {
        int cx = x >> 4, cz = z >> 4;
        uint64_t rng = chunkGenerateRnd(g->seed, cx, cz);
        int rot = nextInt(&rng, 4);
        sx = 5;
        sz = 5;
        if (rot == 0) { sx = -5; }
        if (rot == 1) { sx = -5; sz = -5; }
        if (rot == 2) { sz = -5; }
        x = cx * 16 + 7;
        z = cz * 16 + 7;
    }
    else
    {
        return 1;
    }

    // approx surface height using depth parameter (0.5 ~ sea level)
    double corners[][2] = {
        {(x+ 0)/4.0, (z+ 0)/4.0},
        {(x+sx)/4.0, (z+sz)/4.0},
        {(x+ 0)/4.0, (z+sz)/4.0},
        {(x+sx)/4.0, (z+ 0)/4.0},
    };
    int nptype = g->bn.nptype;
    int i, ret = 1;
    g->bn.nptype = NP_DEPTH;
    for (i = 0; i < 4; i++)
    {
        double depth = sampleClimatePara(&g->bn, 0, corners[i][0], corners[i][1]);
        if (depth < 0.48)
        {
            ret = 0;
            break;
        }
    }
    g->bn.nptype = nptype;
    return ret;
}


/* Given bordering noise columns and a fractional position between those,
 * determine the surface block height (i.e. where the interpolated noise > 0).
 * Note that the noise columns should be of size: ncolxz[ colheight+1 ]
 */
int getSurfaceHeight(
        const double ncol00[], const double ncol01[],
        const double ncol10[], const double ncol11[],
        int colymin, int colymax, int blockspercell, double dx, double dz);

void sampleNoiseColumnEnd(double column[], const SurfaceNoise *sn,
        const EndNoise *en, int x, int z, int colymin, int colymax);

int isViableEndCityTerrain(const Generator *g, const SurfaceNoise *sn,
        int blockX, int blockZ)
{
    const EndNoise *en = &g->en;
    int chunkX = blockX >> 4;
    int chunkZ = blockZ >> 4;
    blockX = chunkX * 16 + 7;
    blockZ = chunkZ * 16 + 7;
    int cellx = (blockX >> 3);
    int cellz = (blockZ >> 3);

    enum { y0 = 15, y1 = 18, yn = y1-y0+1 };
    double ncol[3][3][yn];

    sampleNoiseColumnEnd(ncol[0][0], sn, en, cellx, cellz, y0, y1);
    sampleNoiseColumnEnd(ncol[0][1], sn, en, cellx, cellz+1, y0, y1);
    sampleNoiseColumnEnd(ncol[1][0], sn, en, cellx+1, cellz, y0, y1);
    sampleNoiseColumnEnd(ncol[1][1], sn, en, cellx+1, cellz+1, y0, y1);

    int h00, h01, h10, h11;
    h00 = getSurfaceHeight(ncol[0][0], ncol[0][1], ncol[1][0], ncol[1][1],
            y0, y1, 4, (blockX & 7) / 8.0, (blockZ & 7) / 8.0);

    uint64_t cs;
    if (en->mc <= MC_1_18)
        setSeed(&cs, chunkX + chunkZ * 10387313ULL);
    else
        cs = chunkGenerateRnd(g->seed, chunkX, chunkZ);

    switch (nextInt(&cs, 4))
    {
    case 0: // (++) 0
        sampleNoiseColumnEnd(ncol[0][2], sn, en, cellx+0, cellz+2, y0, y1);
        sampleNoiseColumnEnd(ncol[1][2], sn, en, cellx+1, cellz+2, y0, y1);
        sampleNoiseColumnEnd(ncol[2][0], sn, en, cellx+2, cellz+0, y0, y1);
        sampleNoiseColumnEnd(ncol[2][1], sn, en, cellx+2, cellz+1, y0, y1);
        sampleNoiseColumnEnd(ncol[2][2], sn, en, cellx+2, cellz+2, y0, y1);
        h01 = getSurfaceHeight(ncol[0][1], ncol[0][2], ncol[1][1], ncol[1][2],
                y0, y1, 4, ((blockX    ) & 7) / 8.0, ((blockZ + 5) & 7) / 8.0);
        h10 = getSurfaceHeight(ncol[1][0], ncol[1][1], ncol[2][0], ncol[2][1],
                y0, y1, 4, ((blockX + 5) & 7) / 8.0, ((blockZ    ) & 7) / 8.0);
        h11 = getSurfaceHeight(ncol[1][1], ncol[1][2], ncol[2][1], ncol[2][2],
                y0, y1, 4, ((blockX + 5) & 7) / 8.0, ((blockZ + 5) & 7) / 8.0);
        break;

    case 1: // (-+) 90
        sampleNoiseColumnEnd(ncol[0][2], sn, en, cellx+0, cellz+2, y0, y1);
        sampleNoiseColumnEnd(ncol[1][2], sn, en, cellx+1, cellz+2, y0, y1);
        h01 = getSurfaceHeight(ncol[0][1], ncol[0][2], ncol[1][1], ncol[1][2],
                y0, y1, 4, ((blockX    ) & 7) / 8.0, ((blockZ + 5) & 7) / 8.0);
        h10 = getSurfaceHeight(ncol[0][0], ncol[0][1], ncol[1][0], ncol[1][1],
                y0, y1, 4, ((blockX - 5) & 7) / 8.0, ((blockZ    ) & 7) / 8.0);
        h11 = getSurfaceHeight(ncol[0][1], ncol[0][2], ncol[1][1], ncol[1][2],
                y0, y1, 4, ((blockX - 5) & 7) / 8.0, ((blockZ + 5) & 7) / 8.0);
        break;

    case 2: // (--) 180
        h01 = getSurfaceHeight(ncol[0][0], ncol[0][1], ncol[1][0], ncol[1][1],
                y0, y1, 4, ((blockX    ) & 7) / 8.0, ((blockZ - 5) & 7) / 8.0);
        h10 = getSurfaceHeight(ncol[0][0], ncol[0][1], ncol[1][0], ncol[1][1],
                y0, y1, 4, ((blockX - 5) & 7) / 8.0, ((blockZ    ) & 7) / 8.0);
        h11 = getSurfaceHeight(ncol[0][0], ncol[0][1], ncol[1][0], ncol[1][1],
                y0, y1, 4, ((blockX - 5) & 7) / 8.0, ((blockZ - 5) & 7) / 8.0);
        break;

    case 3: // (+-) 270
        sampleNoiseColumnEnd(ncol[2][0], sn, en, cellx+2, cellz+0, y0, y1);
        sampleNoiseColumnEnd(ncol[2][1], sn, en, cellx+2, cellz+1, y0, y1);
        h01 = getSurfaceHeight(ncol[0][0], ncol[0][1], ncol[1][0], ncol[1][1],
                y0, y1, 4, ((blockX    ) & 7) / 8.0, ((blockZ - 5) & 7) / 8.0);
        h10 = getSurfaceHeight(ncol[1][0], ncol[1][1], ncol[2][0], ncol[2][1],
                y0, y1, 4, ((blockX + 5) & 7) / 8.0, ((blockZ    ) & 7) / 8.0);
        h11 = getSurfaceHeight(ncol[1][0], ncol[1][1], ncol[2][0], ncol[2][1],
                y0, y1, 4, ((blockX + 5) & 7) / 8.0, ((blockZ - 5) & 7) / 8.0);
        break;

    default:
        return 0; // error
    }
    //printf("%d %d %d %d\n", h00, h01, h10, h11);
    if (h01 < h00) h00 = h01;
    if (h10 < h00) h00 = h10;
    if (h11 < h00) h00 = h11;
    return h00 >= 60 ? h00 : 0;
}


//==============================================================================
// Finding Properties of Structures
//==============================================================================


STRUCT(PieceEnv)
{
    Piece *list;
    int *n;
    uint64_t *rng;
    int *ship;
    std::vector<int> *pending;
    int y;
    int typlast;
    int nmax;
    int ntyp[PIECE_COUNT];
};

typedef int (piecefunc_t)(PieceEnv *env, Piece *current, int depth);

static piecefunc_t genTower;
static piecefunc_t genBridge;
static piecefunc_t genHouseTower;
static piecefunc_t genFatTower;

struct EndCityPieceInfo
{
    int sx;
    int sy;
    int sz;
    const char *name;
};

constexpr std::array<EndCityPieceInfo, 20> kEndCityPieceInfo{{
    {  9,  3,  9, "base_floor"},
    { 11,  1, 11, "base_roof"},
    {  4,  5,  1, "bridge_end"},
    {  4,  6,  7, "bridge_gentle_stairs"},
    {  4,  5,  3, "bridge_piece"},
    {  4,  6,  3, "bridge_steep_stairs"},
    { 12,  3, 12, "fat_tower_base"},
    { 12,  7, 12, "fat_tower_middle"},
    { 16,  5, 16, "fat_tower_top"},
    { 11,  7, 11, "second_floor_1"},
    { 11,  7, 11, "second_floor_2"},
    { 13,  1, 13, "second_roof"},
    { 12, 23, 28, "ship"},
    { 13,  7, 13, "third_floor_1"},
    { 13,  7, 13, "third_floor_2"},
    { 15,  1, 15, "third_roof"},
    {  6,  6,  6, "tower_base"},
    {  6,  3,  6, "tower_floor"}, // unused
    {  6,  3,  6, "tower_piece"},
    {  8,  4,  8, "tower_top"},
}};

template <typename Table, typename Key, typename Proj>
constexpr auto table_lookup(const Table &table, const Key &key, Proj proj)
    -> const typename Table::value_type*
{
    for (const auto &entry : table)
    {
        if (entry.*proj == key)
            return &entry;
    }
    return nullptr;
}


namespace {

struct Footprint
{
    int sx;
    int sy;
    int sz;
};

struct WeightedVariantChoice
{
    int upper_exclusive;
    std::uint8_t start;
    Footprint size;
    bool abandoned;
};

constexpr auto apply_orthogonal_rotation(StructureVariant &variant, int sx, int sz, int rotation) -> void
{
    switch (rotation)
    { // orientation: 0:north, 1:east, 2:south, 3:west
    case 0: variant.rotation = 0; variant.mirror = 0; variant.sx = sx; variant.sz = sz; break;
    case 1: variant.rotation = 1; variant.mirror = 0; variant.sx = sz; variant.sz = sx; break;
    case 2: variant.rotation = 0; variant.mirror = 1; variant.sx = sx; variant.sz = sz; break;
    case 3: variant.rotation = 1; variant.mirror = 1; variant.sx = sz; variant.sz = sx; break;
    default: std::unreachable();
    }
}

constexpr auto maybe_temple_footprint(int structure_type, Footprint &out) -> bool
{
    switch (structure_type)
    {
    case Desert_Pyramid: out = Footprint{21, 15, 21}; return true;
    case Jungle_Temple:  out = Footprint{12, 10, 15}; return true;
    case Swamp_Hut:      out = Footprint{ 7,  7,  9}; return true;
    default: return false;
    }
}

auto init_igloo_variant(
    StructureVariant &variant,
    int mc,
    std::uint64_t seed,
    int x,
    int z,
    std::uint64_t &rng
) -> void
{
    if (mc <= MC_1_12)
    {
        setSeed(&rng, getPopulationSeed(mc, seed, (x>>4) - 1, (z>>4) - 1));
    }
    variant.rotation = nextInt(&rng, 4);
    variant.basement = nextDouble(&rng) < 0.5;
    variant.size = nextInt(&rng, 8) + 4;
    variant.sy = 5;
    apply_orthogonal_rotation(variant, 7, 8, variant.rotation);
}

auto init_rotated_feature_bounds(
    StructureVariant &variant,
    int mc,
    int x,
    int z,
    int sx,
    int sy,
    int sz
) -> int;

template <std::size_t N>
auto apply_weighted_choice(
    StructureVariant &variant,
    int roll,
    const std::array<WeightedVariantChoice, N> &choices,
    Footprint &size_out
) -> bool
{
    for (const auto &choice : choices)
    {
        if (roll < choice.upper_exclusive)
        {
            variant.start = choice.start;
            variant.abandoned = choice.abandoned ? 1 : 0;
            size_out = choice.size;
            return true;
        }
    }
    return false;
}

auto init_village_variant(
    StructureVariant &variant,
    int mc,
    int biome_id,
    std::uint64_t &rng,
    int x,
    int z
) -> int
{
    if (mc <= MC_1_9)
        return 0;
    if (!isViableFeatureBiome(mc, Village, biome_id))
        return 0;
    if (mc <= MC_1_13)
    {
        skipNextN(&rng, mc == MC_1_13 ? 10 : 11);
        variant.abandoned = nextInt(&rng, 50) == 0;
        return 1;
    }

    variant.biome = (biome_id == meadow) ? plains : biome_id;
    variant.rotation = nextInt(&rng, 4);
    Footprint size{};

    switch (variant.biome)
    {
    case plains: {
        constexpr std::array<WeightedVariantChoice, 8> kChoices{{
            { 50, 0, { 9, 4,  9}, false},
            {100, 1, {10, 7, 10}, false},
            {150, 2, { 8, 5, 15}, false},
            {200, 3, {11, 9, 11}, false},
            {201, 0, { 9, 4,  9}, true },
            {202, 1, {10, 7, 10}, true },
            {203, 2, { 8, 5, 15}, true },
            {204, 3, {11, 9, 11}, true },
        }};
        if (!apply_weighted_choice(variant, nextInt(&rng, 204), kChoices, size))
            std::unreachable();
        break;
    }
    case desert: {
        constexpr std::array<WeightedVariantChoice, 6> kChoices{{
            { 98, 1, {17, 6,  9}, false},
            {196, 2, {12, 6, 12}, false},
            {245, 3, {15, 6, 15}, false},
            {247, 1, {17, 6,  9}, true },
            {249, 2, {12, 6, 12}, true },
            {250, 3, {15, 6, 15}, true },
        }};
        if (!apply_weighted_choice(variant, nextInt(&rng, 250), kChoices, size))
            std::unreachable();
        break;
    }
    case savanna: {
        constexpr std::array<WeightedVariantChoice, 8> kChoices{{
            {100, 1, {14, 5, 12}, false},
            {150, 2, {11, 6, 11}, false},
            {300, 3, { 9, 6, 11}, false},
            {450, 4, { 9, 6,  9}, false},
            {452, 1, {14, 5, 12}, true },
            {453, 2, {11, 6, 11}, true },
            {456, 3, { 9, 6, 11}, true },
            {459, 4, { 9, 6,  9}, true },
        }};
        if (!apply_weighted_choice(variant, nextInt(&rng, 459), kChoices, size))
            std::unreachable();
        break;
    }
    case taiga: {
        constexpr std::array<WeightedVariantChoice, 4> kChoices{{
            { 49, 1, {22, 3, 18}, false},
            { 98, 2, { 9, 7,  9}, false},
            { 99, 1, {22, 3, 18}, true },
            {100, 2, { 9, 7,  9}, true },
        }};
        if (!apply_weighted_choice(variant, nextInt(&rng, 100), kChoices, size))
            std::unreachable();
        break;
    }
    case snowy_tundra: {
        constexpr std::array<WeightedVariantChoice, 6> kChoices{{
            {100, 1, {12, 8, 8}, false},
            {150, 2, {11, 5, 9}, false},
            {300, 3, { 7, 7, 7}, false},
            {302, 1, {12, 8, 8}, true },
            {303, 2, {11, 5, 9}, true },
            {306, 3, { 7, 7, 7}, true },
        }};
        if (!apply_weighted_choice(variant, nextInt(&rng, 306), kChoices, size))
            std::unreachable();
        break;
    }
    default:
        return 0;
    }

    return init_rotated_feature_bounds(variant, mc, x, z, size.sx, size.sy, size.sz);
}

auto init_bastion_variant(
    StructureVariant &variant,
    int mc,
    std::uint64_t &rng,
    int x,
    int z
) -> int
{
    static constexpr std::array<Footprint, 4> kBastionStarts{{
        {46, 24, 46}, // units/air_base
        {30, 24, 48}, // hoglin_stable/air_base
        {38, 48, 38}, // treasure/big_air_full
        {16, 32, 32}, // bridge/starting_pieces/entrance_base
    }};
    variant.rotation = nextInt(&rng, 4);
    variant.start = nextInt(&rng, 4);
    if (mc == MC_1_16_1)
    {   // swapped in 1.16.1 only
        const auto tmp = variant.start;
        variant.start = variant.rotation;
        variant.rotation = tmp;
    }
    const auto &size = kBastionStarts[static_cast<std::size_t>(variant.start)];
    return init_rotated_feature_bounds(variant, mc, x, z, size.sx, size.sy, size.sz);
}

auto init_temple_variant(StructureVariant &variant, int structure_type, int mc, std::uint64_t &rng) -> int
{
    Footprint dims{};
    if (!maybe_temple_footprint(structure_type, dims))
        return 0;
    variant.sy = dims.sy;
    if (mc <= MC_1_19)
    {
        variant.sx = dims.sx;
        variant.sz = dims.sz;
        return 1;
    }
    apply_orthogonal_rotation(variant, dims.sx, dims.sz, nextInt(&rng, 4));
    return 1;
}

auto init_trial_chambers_variant(StructureVariant &variant, std::uint64_t &rng) -> int
{
    variant.y = nextInt(&rng, 1 + 20) - 40;
    variant.rotation = nextInt(&rng, 4);
    variant.start = nextInt(&rng, 2);
    variant.sx = 19;
    variant.sy = 20;
    variant.sz = 19;
    switch (variant.rotation)
    { // 0:0, 1:cw90, 2:cw180, 3:cw270=ccw90
    case 0: break;
    case 1: variant.x = 1 - variant.sz; variant.z = 0; break;
    case 2: variant.x = 1 - variant.sx; variant.z = 1 - variant.sz; break;
    case 3: variant.x = 0; variant.z = 1 - variant.sx; break;
    default: std::unreachable();
    }
    return 1;
}

auto init_rotated_feature_bounds(
    StructureVariant &variant,
    int mc,
    int x,
    int z,
    int sx,
    int sy,
    int sz
) -> int
{
    variant.sy = sy;
    if (mc >= MC_1_18)
    {
        switch (variant.rotation)
        { // 0:0, 1:cw90, 2:cw180, 3:cw270=ccw90
        case 0: variant.x = 0;    variant.z = 0;    variant.sx = sx; variant.sz = sz; break;
        case 1: variant.x = 1-sz; variant.z = 0;    variant.sx = sz; variant.sz = sx; break;
        case 2: variant.x = 1-sx; variant.z = 1-sz; variant.sx = sx; variant.sz = sz; break;
        case 3: variant.x = 0;    variant.z = 1-sx; variant.sx = sz; variant.sz = sx; break;
        default: std::unreachable();
        }
    }
    else
    {
        switch (variant.rotation)
        { // 0:0, 1:cw90, 2:cw180, 3:cw270=ccw90
        case 0: variant.x = 0;        variant.z = 0;        variant.sx = sx; variant.sz = sz; break;
        case 1: variant.x = (x<0)-sz; variant.z = 0;        variant.sx = sz; variant.sz = sx; break;
        case 2: variant.x = (x<0)-sx; variant.z = (z<0)-sz; variant.sx = sx; variant.sz = sz; break;
        case 3: variant.x = 0;        variant.z = (z<0)-sx; variant.sx = sz; variant.sz = sx; break;
        default: std::unreachable();
        }
    }
    return 1;
}

auto init_geode_variant(
    StructureVariant &variant,
    int mc,
    std::uint64_t seed,
    int x,
    int z,
    std::uint64_t &rng
) -> int
{
    StructureConfig sc{};
    getStructureConfig(Geode, mc, &sc);
    if (mc >= MC_1_18)
    {
        Xoroshiro xr{};
        xSetSeed(&xr, getPopulationSeed(mc, seed, x&~15, z&~15) + sc.salt);
        if (xNextFloat(&xr) >= sc.rarity)
            return 0;
        variant.x = xNextIntJ(&xr, 16);
        variant.z = xNextIntJ(&xr, 16);
        variant.x -= x & 15;
        variant.z -= z & 15;
        variant.y = xNextIntJ(&xr, 1 + 30 + 58) - 58;
        variant.size = xNextIntJ(&xr, 2) + 3;
        xSkipN(&xr, 2);
        variant.cracked = xNextFloat(&xr) < 0.95;
    }
    else
    {
        setSeed(&rng, getPopulationSeed(mc, seed, x&~15, z&~15) + sc.salt);
        if (nextFloat(&rng) >= sc.rarity)
            return 0;
        variant.x = nextInt(&rng, 16);
        variant.z = nextInt(&rng, 16);
        variant.x -= x & 15;
        variant.z -= z & 15;
        variant.y = nextInt(&rng, 1+46-6) + 6;
        variant.size = nextInt(&rng, 2) + 3;
        skipNextN(&rng, 2);
        variant.cracked = nextFloat(&rng) < 0.95;
    }
    variant.x += 5;
    variant.y += 5;
    variant.z += 5;
    return 1;
}

template <typename T, std::size_t N>
constexpr auto contains(const std::array<T, N> &arr, T value) -> bool
{
    for (const auto &entry : arr)
    {
        if (entry == value)
            return true;
    }
    return false;
}

auto resolve_ruined_portal_biome(int mc, int biome_id) -> int
{
    const auto category = getCategory(mc, biome_id);
    static constexpr std::array<int, 5> kDirectCategories{
        desert, jungle, swamp, ocean, nether_wastes
    };
    if (contains(kDirectCategories, category))
        return category;

    if (biome_id == mangrove_swamp)
        return swamp;

    static constexpr std::array<int, 20> kMountainLikeBiomes{
        mountains, mountain_edge, wooded_mountains, gravelly_mountains,
        modified_gravelly_mountains, savanna_plateau, shattered_savanna,
        shattered_savanna_plateau, badlands, eroded_badlands, wooded_badlands_plateau,
        modified_badlands_plateau, modified_wooded_badlands_plateau,
        snowy_taiga_mountains, taiga_mountains, stony_shore, meadow,
        frozen_peaks, jagged_peaks, stony_peaks
    };
    if (contains(kMountainLikeBiomes, biome_id) || biome_id == snowy_slopes)
        return mountains;

    return plains;
}

auto init_ruined_portal_variant(
    StructureVariant &variant,
    int mc,
    int biome_id,
    std::uint64_t &rng
) -> int
{
    variant.biome = resolve_ruined_portal_biome(mc, biome_id);
    if (variant.biome == plains || variant.biome == mountains)
    {
        variant.underground = nextFloat(&rng) < 0.5f;
        if (variant.underground)
            variant.airpocket = 1;
        else
            variant.airpocket = nextFloat(&rng) < 0.5f;
    }
    else if (variant.biome == jungle)
    {
        variant.airpocket = nextFloat(&rng) < 0.5f;
    }

    variant.giant = nextFloat(&rng) < 0.05f;
    variant.start = variant.giant ? (1 + nextInt(&rng, 3)) : (1 + nextInt(&rng, 10));
    variant.rotation = nextInt(&rng, 4);
    variant.mirror = nextFloat(&rng) < 0.5f;
    return 1;
}

auto init_ancient_city_variant(StructureVariant &variant, std::uint64_t &rng, int x, int z) -> int
{
    variant.rotation = nextInt(&rng, 4);
    variant.start = 1 + nextInt(&rng, 3); // city_center_1..3
    constexpr int center_sx = 18;
    constexpr int center_sy = 31;
    constexpr int center_sz = 41;
    switch (variant.rotation)
    { // 0:0, 1:cw90, 2:cw180, 3:cw270=ccw90
    case 0: x = -(x>0);            z = -(z>0);            variant.sx = center_sx; variant.sz = center_sz; break;
    case 1: x = +(x<0) - center_sz; z = -(z>0);           variant.sx = center_sz; variant.sz = center_sx; break;
    case 2: x = +(x<0) - center_sx; z = +(z<0) - center_sz; variant.sx = center_sx; variant.sz = center_sz; break;
    case 3: x = -(x>0);            z = +(z<0) - center_sx; variant.sx = center_sz; variant.sz = center_sx; break;
    default: std::unreachable();
    }
    constexpr int anchor_sx = 13;
    constexpr int anchor_sz = 20;
    switch (variant.rotation)
    { // 0:0, 1:cw90, 2:cw180, 3:cw270=ccw90
    case 0: variant.x = x-anchor_sx; variant.z = z-anchor_sz; break;
    case 1: variant.x = x+anchor_sz; variant.z = z-anchor_sx; break;
    case 2: variant.x = x+anchor_sx; variant.z = z+anchor_sz; break;
    case 3: variant.x = x-anchor_sz; variant.z = z+anchor_sx; break;
    default: std::unreachable();
    }
    variant.y = -27;
    variant.sy = center_sy;
    return 1;
}

auto get_variant_impl(StructureVariant *r, int structType, int mc, std::uint64_t seed,
        int x, int z, int biomeID) -> int
{
    uint64_t rng = chunkGenerateRnd(seed, x >> 4, z >> 4);

    *r = StructureVariant{};
    r->start = -1;
    r->biome = -1;
    r->y = 320;

    switch (structType)
    {
    case Village:
        return init_village_variant(*r, mc, biomeID, rng, x, z);

    case Bastion:
        return init_bastion_variant(*r, mc, rng, x, z);

    case Ancient_City:
        return init_ancient_city_variant(*r, rng, x, z);

    case Ruined_Portal:
    case Ruined_Portal_N:
        return init_ruined_portal_variant(*r, mc, biomeID, rng);

    case Monument:
        r->x = r->z = -29;
        r->sx = r->sz = 58;
        return 1;

    case Igloo:
        init_igloo_variant(*r, mc, seed, x, z, rng);
        return 1;

    case Desert_Pyramid:
    case Jungle_Temple:
    case Swamp_Hut:
        return init_temple_variant(*r, structType, mc, rng);

    case Geode:
        return init_geode_variant(*r, mc, seed, x, z, rng);

    case Trial_Chambers:
        return init_trial_chambers_variant(*r, rng);

    default:
        return 0;
    }
}

} // namespace

int getVariant(StructureVariant *r, int structType, int mc, uint64_t seed,
        int x, int z, int biomeID)
{
    return get_variant_impl(r, structType, mc, seed, x, z, biomeID);
}

static
Piece *addEndCityPiece(PieceEnv *env, Piece *prev, int rot, int px, int py, int pz, int typ)
{
    if (*env->n >= env->nmax)
        return nullptr;

    Piece *p = env->list + *env->n;
    (*env->n)++;
    const auto &piece_info = kEndCityPieceInfo[static_cast<std::size_t>(typ)];
    p->name = piece_info.name;
    p->rot = rot;
    p->depth = 0;
    p->type = typ;

    Pos3 pos = {px, py, pz};
    if (prev)
        pos = prev->pos;
    p->bb0 = p->bb1 = p->pos = pos;
    p->bb1.y += piece_info.sy;
    switch (rot)
    {
    case 0: p->bb1.x += piece_info.sx; p->bb1.z += piece_info.sz; break; // 0
    case 1: p->bb0.x -= piece_info.sz; p->bb1.z += piece_info.sx; break; // 90
    case 2: p->bb0.x -= piece_info.sx; p->bb0.z -= piece_info.sz; break; // 180
    case 3: p->bb1.x += piece_info.sz; p->bb0.z -= piece_info.sx; break; // 270
    default: std::unreachable();
    }
    if (prev)
    {
        int dx = 0, dy = py, dz = 0;
        switch (prev->rot)
        {
        case 0: dx += px; dz += pz; break; // 0
        case 1: dx -= pz; dz += px; break; // 90
        case 2: dx -= px; dz -= pz; break; // 180
        case 3: dx += pz; dz -= px; break; // 270
        default: std::unreachable();
        }
        p->pos.x += dx; p->pos.y += dy; p->pos.z += dz;
        p->bb0.x += dx; p->bb0.y += dy; p->bb0.z += dz;
        p->bb1.x += dx; p->bb1.y += dy; p->bb1.z += dz;
    }
    return p;
}

static
int genPiecesRecusively(piecefunc_t gen, PieceEnv *env, Piece *current, int depth)
{
    if (depth > 8)
        return 0;
    if (*env->n >= env->nmax)
        return 0;
    int i, j, n_local = 0;
    PieceEnv env_local = *env;
    env_local.list = env->list + *env->n;
    env_local.n = &n_local;
    env_local.nmax = env->nmax - *env->n;
    if (!gen(&env_local, current, depth))
        return 0;
    int gendepth = next(env->rng, 32);
    for (i = 0; i < n_local; i++)
    {
        Piece *p = env_local.list + i;
        p->depth = gendepth;
        for (j = 0; j < *env->n; j++)
        {   // check for piece with bounding box collition
            Piece *q = env->list + j;
            if (q->bb1.x >= p->bb0.x && q->bb0.x <= p->bb1.x &&
                q->bb1.z >= p->bb0.z && q->bb0.z <= p->bb1.z &&
                q->bb1.y >= p->bb0.y && q->bb0.y <= p->bb1.y)
            {
                if (current->depth != q->depth)
                    return 0;
                break;
            }
        }
    }
    (*env->n) += n_local;
    return 1;
}

static
int genTower(PieceEnv *env, Piece *current, int depth)
{
    int rot = current->rot;
    int x = 3 + nextInt(env->rng, 2);
    int z = 3 + nextInt(env->rng, 2);
    Piece *base = current;
    base = addEndCityPiece(env, base, rot, x, -3, z, TOWER_BASE);
    base = addEndCityPiece(env, base, rot, 0, 7, 0, TOWER_PIECE);
    Piece *floor = (nextInt(env->rng, 3) == 0 ? base : nullptr);
    int floorcnt = 1 + nextInt(env->rng, 3);
    int i;
    for (i = 0; i < floorcnt; i++)
    {
        base = addEndCityPiece(env, base, rot, 0, 4, 0, TOWER_PIECE);
        if (i < floorcnt - 1 && next(env->rng, 1))
            floor = base;
    }
    if (floor)
    {
        static const int binfo[][4] = {
            {0, 1, -1, 0}, // 0
            {1, 6, -1, 1}, // 90
            {3, 0, -1, 5}, // 270
            {2, 5, -1, 6}, // 180
        };
        for (i = 0; i < 4; i++)
        {
            if (!next(env->rng, 1))
                continue;
            int brot = (rot + binfo[i][0]) & 3;
            Piece *bridge = addEndCityPiece(env, base, brot,
                binfo[i][1], binfo[i][2], binfo[i][3], BRIDGE_END);
            genPiecesRecusively(genBridge, env, bridge, depth+1);
        }
    }
    else if (depth != 7)
    {
        return genPiecesRecusively(genFatTower, env, base, depth+1);
    }

    addEndCityPiece(env, base, rot, -1, 4, -1, TOWER_TOP);
    return 1;
}

static
int genBridge(PieceEnv *env, Piece *current, int depth)
{
    int rot = current->rot;
    int i, y, floorcnt = 1 + nextInt(env->rng, 4);
    Piece *base = current;
    base = addEndCityPiece(env, base, rot, 0, 0, -4, BRIDGE_PIECE);
    base->depth = -1;
    for (i = y = 0; i < floorcnt; i++)
    {
        if (next(env->rng, 1))
        {
            base = addEndCityPiece(env, base, rot, 0, y, -4, BRIDGE_PIECE);
            y = 0;
            continue;
        }
        if (next(env->rng, 1))
            base = addEndCityPiece(env, base, rot, 0, y, -4, BRIDGE_STEEP_STAIRS);
        else
            base = addEndCityPiece(env, base, rot, 0, y, -8, BRIDGE_GENTLE_STAIRS);
        y = 4;
    }
    if (!*env->ship && nextInt(env->rng, 10 - depth) == 0)
    {
        int x = -8 + nextInt(env->rng, 8);
        int z = -70 + nextInt(env->rng, 10);
        base = addEndCityPiece(env, base, rot, x, y, z, END_SHIP);
        *env->ship = 1;
    }
    else
    {
        env->y = y + 1;
        if (!genPiecesRecusively(genHouseTower, env, base, depth+1))
            return 0;
    }
    base = addEndCityPiece(env, base, (rot+2)&3, 4, y, 0, BRIDGE_END);
    base->depth = -1;
    return 1;
}

static
int genHouseTower(PieceEnv *env, Piece *current, int depth)
{
    if (depth > 8) return 0;
    int rot = current->rot;
    Piece *base = current;
    base = addEndCityPiece(env, base, rot, -3, env->y, -11, BASE_FLOOR);
    int size = nextInt(env->rng, 3);
    if (size == 0)
    {
        addEndCityPiece(env, base, rot, -1, 4, -1, BASE_ROOF);
        return 1;
    }
    base = addEndCityPiece(env, base, rot, -1, 0, -1, SECOND_FLOOR_2);
    if (size == 1)
    {
        base = addEndCityPiece(env, base, rot, -1, 8, -1, SECOND_ROOF);
    }
    else
    {
        base = addEndCityPiece(env, base, rot, -1, 4, -1, THIRD_FLOOR_2);
        base = addEndCityPiece(env, base, rot, -1, 8, -1, THIRD_ROOF);
    }
    genPiecesRecusively(genTower, env, base, depth+1);
    return 1;
}

static
int genFatTower(PieceEnv *env, Piece *current, int depth)
{
    int rot = current->rot;
    int i, j;
    Piece *base = current;
    base = addEndCityPiece(env, base, rot, -3, 4, -3, FAT_TOWER_BASE);
    base = addEndCityPiece(env, base, rot, 0, 4, 0, FAT_TOWER_MIDDLE);
    static const int binfo[][4] = {
        {0,  4, -1,  0}, // 0
        {1, 12, -1,  4}, // 90
        {3,  0, -1,  8}, // 270
        {2,  8, -1, 12}, // 180
    };
    for (j = 0; j < 2 && nextInt(env->rng, 3) != 0; j++)
    {
        base = addEndCityPiece(env, base, rot, 0, 8, 0, FAT_TOWER_MIDDLE);
        for (i = 0; i < 4; i++)
        {
            if (!next(env->rng, 1))
                continue;
            int brot = (rot + binfo[i][0]) & 3;
            Piece *bridge = addEndCityPiece(env, base, brot,
                binfo[i][1], binfo[i][2], binfo[i][3], BRIDGE_END);
            genPiecesRecusively(genBridge, env, bridge, depth+1);
        }
    }
    addEndCityPiece(env, base, rot, -2, 8, -2, FAT_TOWER_TOP);
    return 1;
}

int getEndCityPieces(Piece *list, uint64_t seed, int chunkX, int chunkZ)
{
    uint64_t rng = chunkGenerateRnd(seed, chunkX, chunkZ);
    int rot = nextInt(&rng, 4);
    int ship = 0, n = 0;
    PieceEnv env{};
    env.list = list;
    env.n = &n;
    env.rng = &rng;
    env.ship = &ship;
    env.nmax = END_CITY_PIECES_MAX;
    Piece *base = nullptr;
    int x = chunkX * 16 + 8, z = chunkZ * 16 + 8;
    base = addEndCityPiece(&env, base, rot, x, 0, z, BASE_FLOOR);
    base = addEndCityPiece(&env, base, rot, -1, 0, -1, SECOND_FLOOR_1);
    base = addEndCityPiece(&env, base, rot, -1, 4, -1, THIRD_FLOOR_1);
    base = addEndCityPiece(&env, base, rot, -1, 8, -1, THIRD_ROOF);
    genPiecesRecusively(genTower, &env, base, 1);
    return n;
}


static constexpr struct
{
    Pos3 offset, size;
    int skip, repeatable, weight, max;
    const char *name;
}
fortress_info[] = {
    {{ 0, 0,0}, {18, 9,18}, 0, 0, 0, 0, "NeStart"}, // FORTRESS_START
    {{-1,-3,0}, { 4, 9,18}, 0, 1,30, 0, "NeBS"},    // BRIDGE_STRAIGHT
    {{-8,-3,0}, {18, 9,18}, 0, 0,10, 4, "NeBCr"},   // BRIDGE_CROSSING
    {{-2, 0,0}, { 6, 8, 6}, 0, 0,10, 4, "NeRC"},    // BRIDGE_FORTIFIED_CROSSING
    {{-2, 0,0}, { 6,10, 6}, 0, 0,10, 3, "NeSR"},    // BRIDGE_STAIRS
    {{-2, 0,0}, { 6, 7, 8}, 0, 0, 5, 2, "NeMT"},    // BRIDGE_SPAWNER
    {{-5,-3,0}, {12,13,12}, 0, 0, 5, 1, "NeCE"},    // BRIDGE_CORRIDOR_ENTRANCE
    {{-1, 0,0}, { 4, 6, 4}, 0, 1,25, 0, "NeSC"},    // CORRIDOR_STRAIGHT
    {{-1, 0,0}, { 4, 6, 4}, 0, 0,15, 5, "NeSCSC"},  // CORRIDOR_CROSSING
    {{-1, 0,0}, { 4, 6, 4}, 1, 0, 5,10, "NeSCRT"},  // CORRIDOR_TURN_RIGHT
    {{-1, 0,0}, { 4, 6, 4}, 1, 0, 5,10, "NeSCLT"},  // CORRIDOR_TURN_LEFT
    {{-1,-7,0}, { 4,13, 9}, 0, 1,10, 3, "NeCCS"},   // CORRIDOR_STAIRS
    {{-3, 0,0}, { 8, 6, 8}, 0, 0, 7, 2, "NeCTB"},   // CORRIDOR_T_CROSSING
    {{-5,-3,0}, {12,13,12}, 0, 0, 5, 2, "NeCSR"},   // CORRIDOR_NETHER_WART
    {{-1,-3,0}, { 4, 9, 7}, 1, 0, 0, 0, "NeBEF"},   // FORTRESS_END
};

static
Piece *addFortressPiece(PieceEnv *env, int typ, int x, int y, int z, int depth, int facing, int pending)
{
    if (*env->n >= env->nmax)
        return nullptr;

    Pos3 pos = {x, y, z};
    Pos3 b0 = pos, b1 = pos;
    Pos3 d0 = fortress_info[typ].offset, d1 = fortress_info[typ].size;
    b0.y += d0.y;
    b1.y += d0.y+d1.y;
    switch (facing)
    {
    case 0: // 0, north
        b0.x += d0.x;       b0.z += d0.z-d1.z;
        b1.x += d0.x+d1.x;  b1.z += d0.z;
        break;
    case 1: // 90, east
        b0.x += d0.z;       b0.z += d0.x;
        b1.x += d0.z+d1.z;  b1.z += d0.x+d1.x;
        break;
    case 2: // 180, south
        b0.x += d0.x;       b0.z += d0.z;
        b1.x += d0.x+d1.x;  b1.z += d0.z+d1.z;
        break;
    case 3: // 270, west
        b0.x += d0.z-d1.z;  b0.z += d0.x;
        b1.x += d0.z;       b1.z += d0.x+d1.x;
        break;
    }
    Piece *p = env->list + *env->n;
    p->name = fortress_info[typ].name;
    p->pos = pos;
    p->bb0 = b0;
    p->bb1 = b1;
    p->rot = facing;
    p->depth = depth;
    p->type = typ;

    int i, n = *env->n;
    for (i = 0; i < n; i++)
    {
        Piece *q = env->list + i;
        if (q->bb1.x >= p->bb0.x && q->bb0.x <= p->bb1.x &&
            q->bb1.z >= p->bb0.z && q->bb0.z <= p->bb1.z &&
            q->bb1.y >= p->bb0.y && q->bb0.y <= p->bb1.y)
        {
            return nullptr; // collision
        }
    }
    // accept the piece and append it to the processing front
    skipNextN(env->rng, fortress_info[typ].skip);
    //int queue = 0;
    if (pending)
    {
        const int piece_index = *env->n;
        (*env->n)++;
        env->ntyp[typ]++;
        if (typ != FORTRESS_END)
            env->typlast = typ;
        if (env->pending) {
            env->pending->push_back(piece_index);
        }
    }
    //printf("[%3d] typ=%2d @(%4d %4d %4d) f=%d p=%d queue=%2d   rng:%ld\n",
    //    (*env->n-1), typ, b0.x, b0.y, b0.z, facing, pending, queue, *env->rng);
    //fflush(stdout);
    return p;
}


static
void extendFortress(PieceEnv *env, Piece *p, int offh, int offv, int turn, int corridor)
{
    int x, y, z, t, i;
    int depth = p->depth + 1;
    int facing = p->rot;
    int typ0 = corridor ? CORRIDOR_STRAIGHT : BRIDGE_STRAIGHT;
    int typ1 = typ0 + (corridor ? 7 : 6);
    int valid = -1;
    int weight_tot = 0;

    y = p->bb0.y + offv;

    if (turn == 0) { // forward
        switch (facing) {
        case 0: x = p->bb0.x+offh; z = p->bb0.z-1;    break;
        case 1: x = p->bb1.x+1;    z = p->bb0.z+offh; break;
        case 2: x = p->bb0.x+offh; z = p->bb1.z+1;    break;
        case 3: x = p->bb0.x-1;    z = p->bb0.z+offh; break;
        default: std::unreachable();
        }
    } else if (turn == -1) { // left
        if (facing & 1) { x = p->bb0.x+offh; z = p->bb0.z-1;    facing = 0; }
        else            { x = p->bb0.x-1;    z = p->bb0.z+offh; facing = 3; }
    } else if (turn == +1) { // right
        if (facing & 1) { x = p->bb0.x+offh, z = p->bb1.z+1;    facing = 2; }
        else            { x = p->bb1.x+1;    z = p->bb0.z+offh; facing = 1; }
    } else std::unreachable();

    if (IABS(x - env->list->bb0.x) > 112 || IABS(z - env->list->bb0.z) > 112) {
        addFortressPiece(env, FORTRESS_END, x, y, z, depth, facing, 0);
        return;
    }

    for (valid = 0, t = typ0; t < typ1; t++)
    {
        int max = fortress_info[t].max;
        if (max > 0 && env->ntyp[t] >= max)
            continue;
        if (max > 0)
            valid = 1;
        weight_tot += fortress_info[t].weight;
    }

    if (valid == 0 || weight_tot <= 0 || depth > 30) {
        addFortressPiece(env, FORTRESS_END, x, y, z, depth, facing, valid >= 0);
        return;
    }

    for (i = 0; i < 5; i++)
    {
        int n = nextInt(env->rng, weight_tot);
        for (t = typ0; t < typ1; t++)
        {
            int max = fortress_info[t].max;
            if (max > 0 && env->ntyp[t] >= max)
                continue;
            n -= fortress_info[t].weight;
            if (n >= 0)
                continue;
            if (env->typlast == t && !fortress_info[t].repeatable)
                break;
            if (addFortressPiece(env, t, x, y, z, depth, facing, 1) != nullptr)
                return;
        }
    }
    addFortressPiece(env, FORTRESS_END, x, y, z, depth, facing, valid >= 0);
}

static
void extendFortressPiece(PieceEnv *env, Piece *p)
{
    struct Step { int offh, offv, turn, corridor; };
    struct Rule { int type; std::array<Step, 3> steps; int count; };
    static constexpr std::array<Rule, 10> kFortressRules{{
        {BRIDGE_STRAIGHT,           {{{1, 3,  0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}}, 1},
        {BRIDGE_CROSSING,           {{{8, 3,  0, 0}, {8, 3,-1, 0}, {8, 3, 1, 0}}}, 3},
        {FORTRESS_START,            {{{8, 3,  0, 0}, {8, 3,-1, 0}, {8, 3, 1, 0}}}, 3},
        {BRIDGE_FORTIFIED_CROSSING, {{{2, 0,  0, 0}, {2, 0,-1, 0}, {2, 0, 1, 0}}}, 3},
        {BRIDGE_STAIRS,             {{{2, 6,  1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}}, 1},
        {BRIDGE_CORRIDOR_ENTRANCE,  {{{5, 3,  0, 1}, {0, 0, 0, 0}, {0, 0, 0, 0}}}, 1},
        {CORRIDOR_STRAIGHT,         {{{1, 0,  0, 1}, {0, 0, 0, 0}, {0, 0, 0, 0}}}, 1},
        {CORRIDOR_CROSSING,         {{{1, 0,  0, 1}, {1, 0,-1, 1}, {1, 0, 1, 1}}}, 3},
        {CORRIDOR_TURN_RIGHT,       {{{1, 0,  1, 1}, {0, 0, 0, 0}, {0, 0, 0, 0}}}, 1},
        {CORRIDOR_TURN_LEFT,        {{{1, 0, -1, 1}, {0, 0, 0, 0}, {0, 0, 0, 0}}}, 1},
    }};

    if (const auto *rule = table_lookup(kFortressRules, p->type, &Rule::type); rule != nullptr)
    {
        for (int i = 0; i < rule->count; i++)
        {
            const auto &s = rule->steps[static_cast<std::size_t>(i)];
            extendFortress(env, p, s.offh, s.offv, s.turn, s.corridor);
        }
        return;
    }

    if (p->type == CORRIDOR_STAIRS)
    {
        extendFortress(env, p, 1, 0, 0, 1);
        return;
    }
    if (p->type == CORRIDOR_T_CROSSING)
    {
        const auto h = (p->rot == 0 || p->rot == 3) ? 5 : 1;
        extendFortress(env, p, h, 0, -1, nextInt(env->rng, 8) != 0);
        extendFortress(env, p, h, 0,  1, nextInt(env->rng, 8) != 0);
        return;
    }
    if (p->type == CORRIDOR_NETHER_WART)
    {
        extendFortress(env, p, 5, 3,  0, 1);
        extendFortress(env, p, 5, 11, 0, 1);
    }
}

int getFortressPieces(Piece *list, int n, int mc, uint64_t seed, int chunkX, int chunkZ)
{
    uint64_t rng = seed;
    if (mc <= MC_1_15)
    {
        setAttemptSeed(&rng, chunkX, chunkZ);
        nextInt(&rng, 3);
        nextInt(&rng, 8);
        nextInt(&rng, 8);
    }
    else
    {
        rng = chunkGenerateRnd(seed, chunkX, chunkZ);
    }

    int count = 1;
    std::vector<int> pending{};
    pending.reserve(static_cast<std::size_t>(std::max(n, 0)));
    PieceEnv env{};
    env.list = list;
    env.n = &count;
    env.rng = &rng;
    env.pending = &pending;
    env.ntyp[0] = 1;
    env.typlast = 0;
    env.nmax = n;
    Piece *p = list;
    Pos3 pos = {chunkX * 16 + 2, 64, chunkZ * 16 + 2};
    p->name = fortress_info[0].name;
    p->bb0 = p->bb1 = p->pos = pos;
    p->bb1.x += fortress_info[0].size.x;
    p->bb1.y += fortress_info[0].size.y;
    p->bb1.z += fortress_info[0].size.z;
    p->rot = nextInt(&rng, 4);
    p->depth = 0;
    p->type = 0;
    extendFortressPiece(&env, p);
    while (!pending.empty())
    {
        const int i = nextInt(&rng, static_cast<int>(pending.size()));
        const int qidx = pending[static_cast<std::size_t>(i)];
        pending[static_cast<std::size_t>(i)] = pending.back();
        pending.pop_back();
        Piece *q = list + qidx;
        extendFortressPiece(&env, q);
    }
    return count;
}


namespace {

auto get_house_list_impl(std::span<int, 9> out, std::uint64_t seed, int chunk_x, int chunk_z) -> std::uint64_t
{
    uint64_t rng = chunkGenerateRnd(seed, chunk_x, chunk_z);
    skipNextN(&rng, 1);
    struct Roll { int min; int max; };
    static constexpr std::array<Roll, 9> kRolls{{
        {2, 4}, // HouseSmall
        {0, 1}, // Church
        {0, 2}, // Library
        {2, 5}, // WoodHut
        {0, 2}, // Butcher
        {1, 4}, // FarmLarge
        {2, 4}, // FarmSmall
        {0, 1}, // Blacksmith
        {0, 3}, // HouseLarge
    }};
    for (std::size_t i = 0; i < kRolls.size(); ++i)
    {
        const auto &roll = kRolls[i];
        out[i] = nextInt(&rng, roll.max - roll.min + 1) + roll.min;
    }

    return rng;
}

} // namespace

uint64_t getHouseList(int *out, uint64_t seed, int chunkX, int chunkZ)
{
    return get_house_list_impl(std::span<int, 9>{out, 9}, seed, chunkX, chunkZ);
}


void getFixedEndGateways(int mc, uint64_t seed, Pos src[20])
{
    get_fixed_end_gateways_impl(mc, seed, std::span<Pos, 20>{src, 20});
}

namespace {

auto get_fixed_end_gateways_impl(int mc, std::uint64_t seed, std::span<Pos, 20> out) -> void
{
    (void) mc;
    static constexpr std::array<Pos, 20> fixed{{
        { 96,  0}, { 91, 29}, { 77, 56}, { 56, 77}, { 29, 91},
        { -1, 96}, {-30, 91}, {-57, 77}, {-78, 56}, {-92, 29},
        {-96, -1}, {-92,-30}, {-78,-57}, {-57,-78}, {-30,-92},
        {  0,-96}, { 29,-92}, { 56,-78}, { 77,-57}, { 91,-30},
    }};

    std::array<std::uint8_t, 20> order{{
        19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0
    }};

    uint64_t rng = 0;
    setSeed(&rng, seed);

    for (int i = 0; i < 20; i++)
    {
        const auto j = static_cast<std::uint8_t>(19 - nextInt(&rng, 20 - i));
        const auto tmp = order[static_cast<std::size_t>(i)];
        order[i] = order[j];
        order[j] = tmp;
    }

    for (int i = 0; i < 20; i++)
        out[static_cast<std::size_t>(i)] = fixed[order[static_cast<std::size_t>(i)]];
}

} // namespace

Pos getLinkedGatewayChunk(const EndNoise *en, const SurfaceNoise *sn, uint64_t seed,
    Pos src, Pos *dst)
{
    return get_linked_gateway_chunk_impl(en, sn, seed, src, dst);
}

namespace {

auto get_linked_gateway_chunk_impl(
    const EndNoise *en,
    const SurfaceNoise *sn,
    std::uint64_t seed,
    Pos src,
    Pos *dst
) -> Pos
{
    double invr = 1.0 / sqrt(src.x * src.x + src.z * src.z);
    double dx = src.x * invr;
    double dz = src.z * invr;
    double px = dx * 1024;
    double pz = dz * 1024;
    dx *= 16;
    dz *= 16;

    int i;
    Pos c;
    c.x = (int) floor(px) >> 4;
    c.z = (int) floor(pz) >> 4;

    if (isEndChunkEmpty(en, sn, seed, c.x, c.z))
    {   // look forward for the first non-empty chunk
        for (i = 0; i < 15; i++)
        {
            int qx = (int) floor(px += dx) >> 4;
            int qz = (int) floor(pz += dz) >> 4;
            if (qx == c.x && qz == c.z)
                continue;
            c.x = qx;
            c.z = qz;
            if (!isEndChunkEmpty(en, sn, seed, c.x, c.z))
                break;
        }
    }
    else
    {   // look backward for the last non-empty chunk
        for (i = 0; i < 15; i++)
        {
            int qx = (int) floor(px -= dx) >> 4;
            int qz = (int) floor(pz -= dz) >> 4;
            if (isEndChunkEmpty(en, sn, seed, qx, qz))
                break;
            c.x = qx;
            c.z = qz;
        }
    }
    if (dst)
    {
        dst->x = (int) floor(px);
        dst->z = (int) floor(pz);
    }
    return c;
}

auto get_linked_gateway_pos_impl(const EndNoise *en, const SurfaceNoise *sn, std::uint64_t seed, Pos src) -> Pos
{
    float y[33*33]; // buffer for [16][16] and [33][33]
    int ymin = 0;
    int i, j;

    Pos dst;
    Pos c = getLinkedGatewayChunk(en, sn, seed, src, &dst);

    if (en->mc > MC_1_16)
    {
        // The original java implementation has a bug where the result
        // variable for the in-chunk block search is assigned a reference
        // to the mutable iterator, which ends up as the last iteration
        // position and discards the found location.
        dst.x = c.x * 16 + 15;
        dst.z = c.z * 16 + 15;
    }
    else
    {
        mapEndSurfaceHeight(y, en, sn, c.x*16, c.z*16, 16, 16, 1, 30);
        mapEndIslandHeight(y, en, seed, c.x*16, c.z*16, 16, 16, 1);

        uint64_t d = 0;
        for (j = 0; j < 16; j++)
        {
            for (i = 0; i < 16; i++)
            {
                int v = (int) y[j*16 + i];
                if (v < 30) continue;
                uint64_t dx = 16*c.x + i;
                uint64_t dz = 16*c.z + j;
                uint64_t dr = dx*dx + dz*dz + v*v;
                if (dr > d)
                {
                    d = dr;
                    dst.x = dx;
                    dst.z = dz;
                }
            }
        }
        // use the previous result to retrieve the minimum y-level we know,
        // we can skip generation of surfaces that are lower than this
        for (i = 0; i < 16*16; i++)
            if (y[i] > ymin)
                ymin = (int) floor(y[i]);
    }

    Pos sp = { dst.x-16, dst.z-16 };
    // checking end islands is much cheaper than surface height generation, so
    // we can also skip surface generation lower than the highest island around
    std::fill(std::begin(y), std::end(y), 0.0f);
    mapEndIslandHeight(y, en, seed, sp.x, sp.z, 33, 33, 1);
    for (i = 0; i < 33*33; i++)
        if (y[i] > ymin)
            ymin = (int) floor(y[i]);

    mapEndSurfaceHeight(y, en, sn, sp.x, sp.z, 33, 33, 1, ymin);
    mapEndIslandHeight(y, en, seed, sp.x, sp.z, 33, 33, 1);

    float v = -1;
    for (i = 0; i < 33; i++)
    {
        for (j = 0; j < 33; j++)
        {
            if (y[j*33 + i] <= v)
                continue;
            v = y[j*33 + i];
            dst.x = sp.x + i;
            dst.z = sp.z + j;
        }
    }

    return dst;
}

} // namespace

Pos getLinkedGatewayPos(const EndNoise *en, const SurfaceNoise *sn, uint64_t seed, Pos src)
{
    return get_linked_gateway_pos_impl(en, sn, seed, src);
}


//==============================================================================
// Seed Filters
//==============================================================================


double inverf(double x)
{   // compute the inverse error function via newton's method
    double t = x, dt = 1;
    while (fabs(dt) > FLT_EPSILON)
    {
        dt = 0.5 * sqrt(PI) * (erf(t) - x) / exp(-t*t);
        t -= dt;
    }
    return t;
}

void wilson(double n, double p, double z, double *lo, double *hi)
{   // compute the wilson score interval
    double s = z * z / n;
    double t = 1 / (1 + s);
    double w = t * (p + 0.5 * s);
    double d = t * z * sqrt( (p*(1-p) + 0.25*s) / n ) + FLT_EPSILON;
    *lo = w - d;
    *hi = w + d;
}

int monteCarloBiomes(
        Generator         * g,
        Range               r,
        uint64_t          * rng,
        double              coverage,
        double              confidence,
        int (*eval)(Generator *g, int scale, int x, int y, int z, void*),
        void              * data
        )
{
    if (r.sy == 0)
        r.sy = 1;

    struct tuple_t { int x, y, z; };
    std::vector<tuple_t> buf{};
    size_t n = (size_t)r.sx*r.sy*r.sz;

    // z-score (i.e. probit, or standard deviations) for the confidence
    double zscore = sqrt(2.0) * inverf(confidence);

    // One standard deviation is approximately given by sqrt(n) elements,
    // hence we will take zscore * sqrt(n) as the maximum number of samples
    // to reach the desired confidence. This gives us a wilson score interval
    // for the upper and lower bound of successes we aim for.
    double wn = zscore * sqrt(n);
    double wlo, whi;
    wilson(wn, coverage, zscore, &wlo, &whi);

    // When the number of samples approaches the total number elements,
    // we can avoid repeated samples by shuffling a buffer.
    // (TODO: adjust for hypergeometric distribution?)
    if (n < 4 * wn && n < INT_MAX)
        buf.resize(n);

    if (!buf.empty())
    {
        size_t idx = 0;
        int i, k, j;
        for (k = 0; k < r.sy; k++)
        {
            for (j = 0; j < r.sz; j++)
            {
                for (i = 0; i < r.sx; i++)
                {
                    buf[idx] = tuple_t{.x = i, .y = k, .z = j};
                    idx++;
                }
            }
        }
    }

    size_t i = 0;
    double m = 0; // number of samples
    double x = 0; // number of successes
    int ret = 1;

    // iterate over the area in a random order
    for (i = 0; i < n; i++)
    {
        tuple_t t{};
        if (!buf.empty())
        {
            int j = n - i;
            int k = nextInt(rng, j);
            t = buf[k];
            if (k != j-1)
            {
                buf[k] = buf[j-1];
                buf[j-1] = t;
            }
        }
        else
        {
            t.x = nextInt(rng, r.sx);
            t.y = nextInt(rng, r.sy);
            t.z = nextInt(rng, r.sz);
        }

        int status = eval(g, r.scale, r.x+t.x, r.y+t.y, r.z+t.z, data);
        if (status == -1)
            continue;
        else if (status == 0)
            ;
        else if (status == 1)
            x += 1.0;
        else
        {
            ret = 0;
            break;
        }
        m += 1.0;

        // check if we can abort early with the current confidence interval
        double per_m = 1.0 / m;
        double lo, hi;
        wilson(m, x * per_m, zscore, &lo, &hi);

        if (lo - per_m > coverage)
        {
            ret = 1;
            break;
        }
        if (hi + per_m < coverage)
        {
            ret = 0;
            break;
        }

        if (hi - lo < whi - wlo)
        {   // should occur around i ~ wn
            ret = x * per_m > coverage;
            break;
        }
    }
    return ret;
}


void setupBiomeFilter(
    BiomeFilter *bf,
    int mc, uint32_t flags,
    const int *required, int requiredLen,
    const int *excluded, int excludedLen,
    const int *matchany, int matchanyLen)
{
    int i, id;

    *bf = BiomeFilter{};
    bf->flags = flags;

    // The matchany set is built from the intersection of each member,
    // individually treated as a required biome. The search can be aborted with
    // a positive result, as soon as any of those biomes is encountered.
    for (i = 0; i < matchanyLen; i++)
    {
        id = matchany[i];
        if (id < 128)
            bf->biomeToPick |= (1ULL << id);
        else
            bf->biomeToPickM |= (1ULL << (id-128));

        BiomeFilter ibf;
        setupBiomeFilter(&ibf, mc, 0, &id, 1, 0, 0, 0, 0);
        if (i == 0)
        {
            bf->tempsToFind  = ibf.tempsToFind;
            bf->otempToFind  = ibf.otempToFind;
            bf->majorToFind  = ibf.majorToFind;
            bf->edgesToFind  = ibf.edgesToFind;
            bf->raresToFind  = ibf.raresToFind;
            bf->raresToFindM = ibf.raresToFindM;
            bf->shoreToFind  = ibf.shoreToFind;
            bf->shoreToFindM = ibf.shoreToFindM;
            bf->riverToFind  = ibf.riverToFind;
            bf->riverToFindM = ibf.riverToFindM;
            bf->oceanToFind  = ibf.oceanToFind;
        }
        else
        {
            bf->tempsToFind  &= ibf.tempsToFind;
            bf->otempToFind  &= ibf.otempToFind;
            bf->majorToFind  &= ibf.majorToFind;
            bf->edgesToFind  &= ibf.edgesToFind;
            bf->raresToFind  &= ibf.raresToFind;
            bf->raresToFindM &= ibf.raresToFindM;
            bf->shoreToFind  &= ibf.shoreToFind;
            bf->shoreToFindM &= ibf.shoreToFindM;
            bf->riverToFind  &= ibf.riverToFind;
            bf->riverToFindM &= ibf.riverToFindM;
            bf->oceanToFind  &= ibf.oceanToFind;
        }
    }

    // The excluded set is built by checking which of the biomes from each
    // layer have the potential to yield something other than one of the
    // excluded biomes.
    for (i = 0; i < excludedLen; i++)
    {
        id = excluded[i];
        if (id & ~0xbf) // i.e. not in ranges [0,64),[128,192)
        {
            fprintf(stderr, "setupBiomeFilter: biomeID=%d not supported.\n", id);
            exit(-1);
        }
        if (id < 128)
            bf->biomeToExcl |= (1ULL << id);
        else
            bf->biomeToExclM |= (1ULL << (id-128));
    }
    if (excludedLen && mc >= MC_1_7)
    {   // TODO: this does not fully work yet...
        uint64_t b, m;
        int j;
        for (j = Oceanic; j <= Freezing+Special; j++)
        {
            b = m = 0;
            int temp = (j <= Freezing) ? j : ((j - Special) | 0xf00);
            genPotential(&b, &m, L_SPECIAL_1024, mc, flags, temp);
            if ((bf->biomeToExcl & b) || (bf->biomeToExclM & m))
                bf->tempsToExcl |= (1ULL << j);
        }
        for (j = 0; j < 256; j++)
        {
            if (!isOverworld(mc, j))
                continue;
            if (j < 128)
            {
                b = m = 0;
                genPotential(&b, &m, L_BIOME_256, mc, flags, j);
                if ((~bf->biomeToExcl & b) || (~bf->biomeToExclM & m))
                    bf->majorToExcl |= (1ULL << j);
            }
            b = m = 0;
            genPotential(&b, &m, L_BIOME_EDGE_64, mc, flags, j);
            if ((~bf->biomeToExcl & b) || (~bf->biomeToExclM & m))
            {
                if (j < 128)
                    bf->edgesToExcl |= (1ULL << j);
                else // bamboo_jungle are mapped onto & 0x3F
                    bf->edgesToExcl |= (1ULL << (j-128));
            }
            b = m = 0;
            genPotential(&b, &m, L_SUNFLOWER_64, mc, flags, j);
            if ((~bf->biomeToExcl & b) || (~bf->biomeToExclM & m))
            {
                if (j < 128)
                    bf->raresToExcl |= (1ULL << j);
                else
                    bf->raresToExclM |= (1ULL << (j-128));
            }
            b = m = 0;
            genPotential(&b, &m, L_SHORE_16, mc, flags, j);
            if ((~bf->biomeToExcl & b) || (~bf->biomeToExclM & m))
            {
                if (j < 128)
                    bf->shoreToExcl |= (1ULL << j);
                else
                    bf->shoreToExclM |= (1ULL << (j-128));
            }
            b = m = 0;
            genPotential(&b, &m, L_RIVER_MIX_4, mc, flags, j);
            if ((~bf->biomeToExcl & b) || (~bf->biomeToExclM & m))
            {
                if (j < 128)
                    bf->riverToExcl |= (1ULL << j);
                else
                    bf->riverToExclM |= (1ULL << (j-128));
            }
        }
    }

    // The required set is built from the biomes that should be present at each
    // of the layers. The search can be aborted with a negative result as soon
    // as a biome is missing at the corresponding layer.
    for (i = 0; i < requiredLen; i++)
    {
        id = required[i];
        if (id & ~0xbf) // i.e. not in ranges [0,64),[128,192)
        {
            fprintf(stderr, "setupBiomeFilter: biomeID=%d not supported.\n", id);
            exit(-1);
        }

        switch (id)
        {
        case mushroom_fields:
            // mushroom shores can generate with hills and at rivers
            bf->raresToFind |= (1ULL << mushroom_fields);
            // fall through
        case mushroom_field_shore:
            bf->tempsToFind |= (1ULL << Oceanic);
            bf->majorToFind |= (1ULL << mushroom_fields);
            bf->riverToFind |= (1ULL << id);
            break;

        case badlands_plateau:
        case wooded_badlands_plateau:
        case badlands:
        case eroded_badlands:
        case modified_badlands_plateau:
        case modified_wooded_badlands_plateau:
            bf->tempsToFind |= (1ULL << (Warm+Special));
            if (id == badlands_plateau || id == modified_badlands_plateau)
                bf->majorToFind |= (1ULL << badlands_plateau);
            if (id == wooded_badlands_plateau || id == modified_wooded_badlands_plateau)
                bf->majorToFind |= (1ULL << wooded_badlands_plateau);
            if (id < 128) {
                bf->raresToFind |= (1ULL << id);
                bf->riverToFind |= (1ULL << id);
            } else {
                bf->raresToFindM |= (1ULL << (id-128));
                bf->riverToFindM |= (1ULL << (id-128));
            }
            break;

        case jungle:
        case jungle_edge:
        case jungle_hills:
        case modified_jungle:
        case modified_jungle_edge:
        case bamboo_jungle:
        case bamboo_jungle_hills:
            bf->tempsToFind |= (1ULL << (Lush+Special));
            bf->majorToFind |= (1ULL << jungle);
            if (id == bamboo_jungle || id == bamboo_jungle_hills) {
                // bamboo%64 are End biomes, so we can reuse the edgesToFind
                bf->edgesToFind |= (1ULL << (bamboo_jungle & 0x3f));
                bf->raresToFindM |= (1ULL << (id-128));
                bf->riverToFindM |= (1ULL << (id-128));
            } else if (id == jungle_edge) {
                // un-modified jungle_edge can be created at shore layer
                bf->riverToFind |= (1ULL << jungle_edge);
            } else {
                if (id == modified_jungle_edge)
                    bf->edgesToFind |= (1ULL << jungle_edge);
                else
                    bf->edgesToFind |= (1ULL << jungle);
                if (id < 128) {
                    bf->raresToFind |= (1ULL << id);
                    bf->riverToFind |= (1ULL << id);
                } else {
                    bf->raresToFindM |= (1ULL << (id-128));
                    bf->riverToFindM |= (1ULL << (id-128));
                }
            }
            break;

        case giant_tree_taiga:
        case giant_tree_taiga_hills:
        case giant_spruce_taiga:
        case giant_spruce_taiga_hills:
            bf->tempsToFind |= (1ULL << (Cold+Special));
            bf->majorToFind |= (1ULL << giant_tree_taiga);
            bf->edgesToFind |= (1ULL << giant_tree_taiga);
            if (id < 128) {
                bf->raresToFind |= (1ULL << id);
                bf->riverToFind |= (1ULL << id);
            } else {
                bf->raresToFindM |= (1ULL << (id-128));
                bf->riverToFindM |= (1ULL << (id-128));
            }
            break;

        case savanna:
        case savanna_plateau:
        case shattered_savanna:
        case shattered_savanna_plateau:
        case desert_hills:
        case desert_lakes:
            bf->tempsToFind |= (1ULL << Warm);
            if (id == desert_hills || id == desert_lakes) {
                bf->majorToFind |= (1ULL << desert);
                bf->edgesToFind |= (1ULL << desert);
            } else {
                bf->majorToFind |= (1ULL << savanna);
                bf->edgesToFind |= (1ULL << savanna);
            }
            if (id < 128) {
                bf->raresToFind |= (1ULL << id);
                bf->riverToFind |= (1ULL << id);
            } else {
                bf->raresToFindM |= (1ULL << (id-128));
                bf->riverToFindM |= (1ULL << (id-128));
            }
            break;

        case dark_forest:
        case dark_forest_hills:
        case birch_forest:
        case birch_forest_hills:
        case tall_birch_forest:
        case tall_birch_hills:
        case swamp:
        case swamp_hills:
            bf->tempsToFind |= (1ULL << Lush);
            if (id == dark_forest || id == dark_forest_hills) {
                bf->majorToFind |= (1ULL << dark_forest);
                bf->edgesToFind |= (1ULL << dark_forest);
            }
            else if (id == birch_forest || id == birch_forest_hills ||
                     id == tall_birch_forest || id == tall_birch_hills) {
                bf->majorToFind |= (1ULL << birch_forest);
                bf->edgesToFind |= (1ULL << birch_forest);
            }
            else if (id == swamp || id == swamp_hills) {
                bf->majorToFind |= (1ULL << swamp);
                bf->edgesToFind |= (1ULL << swamp);
            }
            if (id < 128) {
                bf->raresToFind |= (1ULL << id);
                bf->riverToFind |= (1ULL << id);
            } else {
                bf->raresToFindM |= (1ULL << (id-128));
                bf->riverToFindM |= (1ULL << (id-128));
            }
            break;

        case snowy_taiga:
        case snowy_taiga_hills:
        case snowy_taiga_mountains:
        case snowy_tundra:
        case snowy_mountains:
        case ice_spikes:
        case frozen_river:
            bf->tempsToFind |= (1ULL << Freezing);
            if (id == snowy_taiga || id == snowy_taiga_hills ||
                id == snowy_taiga_mountains)
                bf->edgesToFind |= (1ULL << snowy_taiga);
            else
                bf->edgesToFind |= (1ULL << snowy_tundra);
            if (id == frozen_river) {
                bf->raresToFind |= (1ULL << snowy_tundra);
                bf->riverToFind |= (1ULL << id);
            } else if (id < 128) {
                bf->raresToFind |= (1ULL << id);
                bf->riverToFind |= (1ULL << id);
            } else {
                bf->raresToFindM |= (1ULL << (id-128));
                bf->riverToFindM |= (1ULL << (id-128));
            }
            break;

        case sunflower_plains:
            bf->raresToFindM |= (1ULL << (id-128));
            bf->riverToFindM |= (1ULL << (id-128));
            break;

        case snowy_beach:
            bf->tempsToFind |= (1ULL << Freezing);
            // fall through
        case beach:
        case stone_shore:
            bf->riverToFind |= (1ULL << id);
            break;

        case mountains:
            bf->majorToFind |= (1ULL << mountains);
            // fall through
        case wooded_mountains:
            bf->raresToFind |= (1ULL << id);
            bf->riverToFind |= (1ULL << id);
            break;
        case gravelly_mountains:
            bf->majorToFind |= (1ULL << mountains);
            // fall through
        case modified_gravelly_mountains:
            bf->raresToFindM |= (1ULL << (id-128));
            bf->riverToFindM |= (1ULL << (id-128));
            break;

        case taiga:
        case taiga_hills:
            bf->edgesToFind |= (1ULL << taiga);
            bf->raresToFind |= (1ULL << id);
            bf->riverToFind |= (1ULL << id);
            break;
        case taiga_mountains:
            bf->edgesToFind |= (1ULL << taiga);
            bf->raresToFindM |= (1ULL << (id-128));
            bf->riverToFindM |= (1ULL << (id-128));
            break;

        case plains:
        case forest:
        case wooded_hills:
            bf->raresToFind |= (1ULL << id);
            bf->riverToFind |= (1ULL << id);
            break;
        case flower_forest:
            bf->raresToFindM |= (1ULL << (id-128));
            bf->riverToFindM |= (1ULL << (id-128));
            break;

        case desert: // can generate at shore layer
            bf->riverToFind |= (1ULL << id);
            break;

        default:
            if (isOceanic(id)) {
                bf->tempsToFind |= (1ULL << Oceanic);
                bf->oceanToFind |= (1ULL << id);
                if (isShallowOcean(id)) {
                    if (id != lukewarm_ocean && id != cold_ocean)
                        bf->otempToFind |= (1ULL << id);
                } else {
                    if (id == deep_warm_ocean)
                        bf->otempToFind |= (1ULL << warm_ocean);
                    else if (id == deep_ocean)
                        bf->otempToFind |= (1ULL << ocean);
                    else if (id == deep_frozen_ocean)
                        bf->otempToFind |= (1ULL << frozen_ocean);
                    if (!(flags & FORCE_OCEAN_VARIANTS)) {
                        bf->raresToFind |= (1ULL << deep_ocean);
                        bf->riverToFind |= (1ULL << deep_ocean);
                    }
                }
            } else {
                if (id < 64)
                    bf->riverToFind |= (1ULL << id);
                else
                    bf->riverToFindM |= (1ULL << (id-128));
            }
            break;
        }
    }

    bf->biomeToFind = bf->riverToFind;
    bf->biomeToFind &= ~((1ULL << ocean) | (1ULL << deep_ocean));
    bf->biomeToFind |= bf->oceanToFind;
    bf->biomeToFindM = bf->riverToFindM;

    bf->shoreToFind = bf->riverToFind;
    bf->shoreToFind &= ~((1ULL << river) | (1ULL << frozen_river));
    bf->shoreToFindM = bf->riverToFindM;

    bf->specialCnt = 0;
    bf->specialCnt += !!(bf->tempsToFind & (1ULL << (Warm+Special)));
    bf->specialCnt += !!(bf->tempsToFind & (1ULL << (Lush+Special)));
    bf->specialCnt += !!(bf->tempsToFind & (1ULL << (Cold+Special)));
}


typedef struct {
    Generator *g;
    int *ids;
    Range r;
    uint32_t flags;
    uint64_t b, m;
    uint64_t breq, mreq;
    uint64_t bexc, mexc;
    uint64_t bany, many;
    volatile char *stop;
} gdt_info_t;

static int f_graddesc_test(void *data, int x, int z, double p)
{
    (void) p;
    gdt_info_t *info = (gdt_info_t *) data;
    if (info->stop && *info->stop)
        return 1;
    int idx = (z - info->r.z) * info->r.sx + (x - info->r.x);
    if (info->ids[idx] != -1)
        return 0;
    int id = getBiomeAt(info->g, info->r.scale, x, info->r.y, z);
    info->ids[idx] = id;
    if (id < 128) info->b |= (1ULL << id);
    else info->m |= (1ULL << (id-128));

    // check if we know enough to stop
    int match_exc = (info->bexc|info->mexc) == 0;
    int match_any = (info->bany|info->many) == 0;
    int match_req = (info->breq|info->mreq) == 0;
    if (!match_exc && ((info->b & info->bexc) || (info->m & info->mexc)))
        return 1; // encountered an excluded biome -> stop
    match_any |= ((info->b & info->bany) || (info->m & info->many));
    match_req |= ((info->b & info->breq) == info->breq &&
                  (info->m & info->mreq) == info->mreq);
    if (match_exc && match_any && match_req)
        return 1; // all conditions met -> stop
    return 0;
}

int checkForBiomes(
        Generator         * g,
        int               * cache,
        Range               r,
        int                 dim,
        uint64_t            seed,
        const BiomeFilter * filter,
        volatile char     * stop
        )
{
    if (stop && *stop)
        return 0;
    int i, j, k, ret;
    if (r.sy == 0)
        r.sy = 1;

    if (g->mc <= MC_B1_7)
    {   // TODO: optimize
        std::vector<int> owned_ids;
        int *ids = cache;
        if (ids == nullptr)
        {
            owned_ids.resize(getMinCacheSize(g, r.scale, r.sx, r.sy, r.sz), 0);
            ids = owned_ids.data();
        }

        if (g->dim != dim || g->seed != seed)
            applySeed(g, dim, seed);

        genBiomes(g, ids, r);
        uint64_t b = 0;
        for (i = 0; i < r.sx*r.sz; i++)
            b |= (1ULL << ids[i]);

        int match_exc = (filter->biomeToExcl) == 0;
        int match_any = (filter->biomeToPick) == 0;
        int match_req = (filter->biomeToFind) == 0;
        match_exc |= (b & filter->biomeToExcl) == 0;
        match_any |= (b & filter->biomeToPick) != 0;
        match_req |= (b & filter->biomeToFind) == filter->biomeToFind;
        return match_exc && match_any && match_req;
    }
    if (g->mc <= MC_1_17 && dim == DIM_OVERWORLD)
    {
        Layer *entry = (Layer*) getLayerForScale(g, r.scale);
        ret = checkForBiomesAtLayer(&g->layered.ls, entry, cache, seed,
            r.x, r.z, r.sx, r.sz, filter);
        if (ret == 0 && r.sy > 1 && cache)
        {
            for (i = 0; i < r.sy; i++)
            {   // overworld has no vertical noise: expanding 2D into 3D
                for (j = 0; j < r.sx*r.sz; j++)
                    cache[i*r.sx*r.sz + j] = cache[j];
            }
        }
        return ret;
    }

    int id;
    std::vector<int> owned_ids;
    int *ids = cache;
    if (ids == nullptr)
    {
        owned_ids.resize(getMinCacheSize(g, r.scale, r.sx, r.sy, r.sz), 0);
        ids = owned_ids.data();
    }

    if (g->dim != dim || g->seed != seed)
    {
        applySeed(g, dim, seed);
    }

    gdt_info_t info[1];
    info->g = g;
    info->ids = ids;
    info->r = r;
    info->flags = filter->flags;
    info->b = info->m = 0;
    info->breq = filter->biomeToFind;
    info->mreq = filter->biomeToFindM;
    info->bexc = filter->biomeToExcl;
    info->mexc = filter->biomeToExclM;
    info->bany = filter->biomeToPick;
    info->many = filter->biomeToPickM;
    info->stop = stop;

    ret = 0;
    std::fill_n(ids, static_cast<std::size_t>(r.sx * r.sz), -1);

    struct Tuple
    {
        int i;
        int x;
        int y;
        int z;
    };

    const int n = r.sx*r.sy*r.sz;
    int trials = n;
    bool sample_biomes = true;

    if (r.scale == 4 && r.sx * r.sz > 64 && dim == DIM_OVERWORLD)
    {
        // Do a gradient descent to find the min/max of some climate parameters
        // and check the biomes along the way. This has a much better chance
        // of finding the biomes with exteme climates early.
        double tmin, tmax;
        int err = 0;
        do
        {
            err = getParaRange(&g->bn.climate[NP_TEMPERATURE], &tmin, &tmax,
                r.x, r.z, r.sx, r.sz, info, f_graddesc_test);
            if (err) break;
            err = getParaRange(&g->bn.climate[NP_HUMIDITY], &tmin, &tmax,
                r.x, r.z, r.sx, r.sz, info, f_graddesc_test);
            if (err) break;
            err = getParaRange(&g->bn.climate[NP_EROSION], &tmin, &tmax,
                r.x, r.z, r.sx, r.sz, info, f_graddesc_test);
            if (err) break;
            //err = getParaRange(&g->bn.climate[NP_CONTINENTALNESS], &tmin, &tmax,
            //    r.x, r.z, r.sx, r.sz, info, f_graddesc_test);
            //if (err) break;
            //err = getParaRange(&g->bn.climate[NP_WEIRDNESS], &tmin, &tmax,
            //    r.x, r.z, r.sx, r.sz, info, f_graddesc_test);
            //if (err) break;
        }
        while (0);
        if (err || (stop && *stop) || (filter->flags & BF_APPROX))
            sample_biomes = false;
    }

    std::vector<Tuple> buf;
    if (sample_biomes)
    {
        // We'll shuffle the coordinates so we'll generate the biomes in a
        // stochastic manner.
        buf.resize(static_cast<std::size_t>(n));

        id = 0;
        for (k = 0; k < r.sy; k++)
        {
            for (j = 0; j < r.sz; j++)
            {
                for (i = 0; i < r.sx; i++)
                {
                    buf[static_cast<std::size_t>(id)] = Tuple{
                        .i = id,
                        .x = i,
                        .y = k,
                        .z = j,
                    };
                    id++;
                }
            }
        }

        // Determine a number of trials that gives a decent chance to sample all
        // the biomes that are present, assuming a completely random and
        // independent biome distribution. (This is actually not at all the case.)
        if (filter->flags & BF_APPROX)
        {
            int t = 400 + (int) sqrt(n);
            if (trials > t)
                trials = t;
        }

        for (i = 0; i < trials; i++)
        {
            const int remaining = n - i;
            const int swap_idx = rand() % remaining;
            auto current = buf[static_cast<std::size_t>(swap_idx)];
            if (swap_idx != remaining-1)
            {
                buf[static_cast<std::size_t>(swap_idx)] =
                    buf[static_cast<std::size_t>(remaining-1)];
                buf[static_cast<std::size_t>(remaining-1)] = current;
            }

            if (stop && *stop)
                break;
            if (current.y == 0 && info->ids[current.i] != -1)
                continue;
            id = getBiomeAt(g, r.scale, r.x+current.x, r.y+current.y, r.z+current.z);
            info->ids[current.i] = id;
            if (id < 128) info->b |= (1ULL << id);
            else info->m |= (1ULL << (id-128));

            // check if we know enough to yield a result
            int match_exc = (info->bexc|info->mexc) == 0;
            int match_any = (info->bany|info->many) == 0;
            int match_req = (info->breq|info->mreq) == 0;
            if (!match_exc && ((info->b & info->bexc) || (info->m & info->mexc)))
                break; // encountered an excluded biome
            match_any |= ((info->b & info->bany) || (info->m & info->many));
            match_req |= ((info->b & info->breq) == info->breq &&
                          (info->m & info->mreq) == info->mreq);
            if (match_exc && match_any && match_req)
                break; // all conditions met
        }
    }

    if (stop && *stop)
    {
        ret = 0;
    }
    else
    {   // given the biome set {info.b, info.m} determine if we have a match
        int match_exc = (info->bexc|info->mexc) == 0;
        int match_any = (info->bany|info->many) == 0;
        int match_req = (info->breq|info->mreq) == 0;
        match_exc |= ((info->b & info->bexc) || (info->m & info->mexc)) == 0;
        match_any |= ((info->b & info->bany) || (info->m & info->many));
        match_req |= ((info->b & info->breq) == info->breq &&
                      (info->m & info->mreq) == info->mreq);
        ret = (match_exc && match_any && match_req);
    }
    return ret;
}


STRUCT(filter_data_t)
{
    const BiomeFilter *bf;
    int (*map)(const Layer *, int *, int, int, int, int);
};

enum { M_STOP=1, M_DONE=2 };

static int mapFilterSpecial(const Layer * l, int * out, int x, int z, int w, int h)
{
    const filter_data_t *f = (const filter_data_t*) l->data;
    int i, j;
    uint64_t temps;

    /// pre-gen checks
    int specialcnt = f->bf->specialCnt;
    if (specialcnt > 0)
    {
        uint64_t ss = l->startSeed;
        uint64_t cs;

        for (j = 0; j < h; j++)
        {
            for (i = 0; i < w; i++)
            {
                cs = getChunkSeed(ss, x+i, z+j);
                if (mcFirstIsZero(cs, 13))
                    specialcnt--;
            }
        }
        if (specialcnt > 0)
            return M_STOP;
    }

    int err = f->map(l, out, x, z, w, h);
    if unlikely(err != 0)
        return err;

    temps = 0;

    for (j = 0; j < h; j++)
    {
        for (i = 0; i < w; i++)
        {
            int id = out[i + w*j];
            int isspecial = id & 0xf00;
            id &= ~0xf00;
            if (isspecial && id != Freezing)
               temps |= (1ULL << (id+Special));
            else
               temps |= (1ULL << id);
        }
    }
    if ((temps & f->bf->tempsToFind) ^ f->bf->tempsToFind)
        return M_STOP;
    return 0;
}

static int mapFilterMushroom(const Layer * l, int * out, int x, int z, int w, int h)
{
    const filter_data_t *f = (const filter_data_t*) l->data;
    int i, j;
    int err;

    if (w*h < 100 && (f->bf->majorToFind & (1ULL << mushroom_fields)))
    {
        uint64_t ss = l->startSeed;
        bool has_proto_mushroom = false;

        for (j = 0; j < h; j++)
        {
            for (i = 0; i < w; i++)
            {
                const uint64_t cs = getChunkSeed(ss, i+x, j+z);
                if (mcFirstIsZero(cs, 100))
                {
                    has_proto_mushroom = true;
                    break;
                }
            }
            if (has_proto_mushroom)
                break;
        }
        if (!has_proto_mushroom)
            return M_STOP;
    }

    err = f->map(l, out, x, z, w, h);
    if unlikely(err != 0)
        return err;

    if (f->bf->majorToFind & (1ULL << mushroom_fields))
    {
        for (i = 0; i < w*h; i++)
            if (out[i] == mushroom_fields)
                return 0;
        return M_STOP;
    }
    return 0;
}

static int mapFilterBiome(const Layer * l, int * out, int x, int z, int w, int h)
{
    const filter_data_t *f = (const filter_data_t*) l->data;
    int i, j;
    uint64_t b;

    int err = f->map(l, out, x, z, w, h);
    if unlikely(err != 0)
        return err;

    b = 0;
    for (j = 0; j < h; j++)
    {
        for (i = 0; i < w; i++)
        {
            int id = out[i + w*j];
            b |= (1ULL << id);
        }
    }
    if ((b & f->bf->majorToFind) ^ f->bf->majorToFind)
        return M_STOP;
    return 0;
}

static int mapFilterOceanTemp(const Layer * l, int * out, int x, int z, int w, int h)
{
    const filter_data_t *f = (const filter_data_t*) l->data;
    int i, j;
    uint64_t b;

    int err = f->map(l, out, x, z, w, h);
    if unlikely(err != 0)
        return err;

    b = 0;
    for (j = 0; j < h; j++)
    {
        for (i = 0; i < w; i++)
        {
            int id = out[i + w*j];
            b |= (1ULL << id);
        }
    }
    if ((b & f->bf->otempToFind) ^ f->bf->otempToFind)
        return M_STOP;
    return 0;
}

static int mapFilterBiomeEdge(const Layer * l, int * out, int x, int z, int w, int h)
{
    const filter_data_t *f = (const filter_data_t*) l->data;
    uint64_t b;
    int i;
    int err;

    err = f->map(l, out, x, z, w, h);
    if unlikely(err != 0)
        return err;

    b = 0;
    for (i = 0; i < w*h; i++)
        b |= (1ULL << (out[i] & 0x3f));
    if ((b & f->bf->edgesToFind) ^ f->bf->edgesToFind)
        return M_STOP;
    return 0;
}

static int mapFilterRareBiome(const Layer * l, int * out, int x, int z, int w, int h)
{
    const filter_data_t *f = (const filter_data_t*) l->data;
    uint64_t b, bm;
    int i;
    int err;

    err = f->map(l, out, x, z, w, h);
    if unlikely(err != 0)
        return err;

    b = 0; bm = 0;
    for (i = 0; i < w*h; i++)
    {
        int id = out[i];
        if (id < 128) b |= (1ULL << id);
        else bm |= (1ULL << (id-128));
    }
    if ((b & f->bf->raresToFind) ^ f->bf->raresToFind)
        return M_STOP;
    if ((bm & f->bf->raresToFindM) ^ f->bf->raresToFindM)
        return M_STOP;
    return 0;
}

static int mapFilterShore(const Layer * l, int * out, int x, int z, int w, int h)
{
    const filter_data_t *f = (const filter_data_t*) l->data;
    uint64_t b, bm;
    int i;

    int err = f->map(l, out, x, z, w, h);
    if unlikely(err != 0)
        return err;

    b = 0; bm = 0;
    for (i = 0; i < w*h; i++)
    {
        int id = out[i];
        if (id < 128) b |= (1ULL << id);
        else bm |= (1ULL << (id-128));
    }
    if ((b & f->bf->shoreToFind) ^ f->bf->shoreToFind)
        return M_STOP;
    if ((bm & f->bf->shoreToFindM) ^ f->bf->shoreToFindM)
        return M_STOP;
    return 0;
}

static int mapFilterRiverMix(const Layer * l, int * out, int x, int z, int w, int h)
{
    const filter_data_t *f = (const filter_data_t*) l->data;
    uint64_t b, bm;
    int i;

    int err = f->map(l, out, x, z, w, h);
    if unlikely(err != 0)
        return err;

    b = 0; bm = 0;
    for (i = 0; i < w*h; i++)
    {
        int id = out[i];
        if (id < 128) b |= (1ULL << id);
        else bm |= (1ULL << (id-128));
    }
    if ((b & f->bf->riverToFind) ^ f->bf->riverToFind)
        return M_STOP;
    if ((bm & f->bf->riverToFindM) ^ f->bf->riverToFindM)
        return M_STOP;
    return 0;
}

static int mapFilterOceanMix(const Layer * l, int * out, int x, int z, int w, int h)
{
    const filter_data_t *f = (const filter_data_t*) l->data;
    uint64_t b;
    int i;
    int err;

    if (f->bf->riverToFind)
    {
        err = cubiomes::cpp::invoke_layer_map(
            *l->p,
            std::span<int>{out, static_cast<std::size_t>(w) * static_cast<std::size_t>(h)},
            x,
            z,
            w,
            h
        ); // RiverMix
        if (err)
            return err;
    }

    err = f->map(l, out, x, z, w, h);
    if unlikely(err != 0)
        return err;

    b = 0;
    for (i = 0; i < w*h; i++)
    {
        int id = out[i];
        if (id < 128) b |= (1ULL << id);
    }

    if ((b & f->bf->oceanToFind) ^ f->bf->oceanToFind)
        return M_STOP;
    return 0;
}

class ScopedLayerMapOverride final
{
public:
    ScopedLayerMapOverride(
        filter_data_t &fd,
        const BiomeFilter *bf,
        Layer &layer,
        int (*map)(const Layer *, int *, int, int, int, int)
    ) : fd_(fd), layer_(layer)
    {
        fd_.bf = bf;
        fd_.map = layer_.getMap;
        layer_.data = static_cast<void*>(&fd_);
        layer_.getMap = map;
    }

    ~ScopedLayerMapOverride()
    {
        layer_.getMap = fd_.map;
        layer_.data = nullptr;
    }

    ScopedLayerMapOverride(const ScopedLayerMapOverride&) = delete;
    auto operator=(const ScopedLayerMapOverride&) -> ScopedLayerMapOverride& = delete;

private:
    filter_data_t &fd_;
    Layer &layer_;
};

static
int testExclusion(Layer *layer, int *cache, int x, int z, const BiomeFilter *bf)
{
    int err = cubiomes::cpp::invoke_layer_map(
        *layer,
        std::span<int>{cache, 1U},
        x,
        z,
        1,
        1
    );
    if (err)
        return 0; // skip, but don't treat error as valid
    int id = cache[0];
    if (id < 128)
        return (bf->biomeToExcl & (1ULL << id)) != 0;
    return (bf->biomeToExclM & (1ULL << (id-128))) != 0;
}

int checkForBiomesAtLayer(
        LayerStack        * g,
        Layer             * entry,
        int               * cache,
        uint64_t            seed,
        int                 x,
        int                 z,
        unsigned int        w,
        unsigned int        h,
        const BiomeFilter * filter
        )
{
    Layer *l;
    int *ids = cache;
    int ret, err;
    int memsiz = 0;
    int mem1x1;
    std::vector<int> owned_ids;

    if (filter->flags & BF_APPROX) // TODO: protoCheck for 1.6-
    {
        l = entry;

        int i, j;
        int bx = x * l->scale;
        int bz = z * l->scale;
        int bw = w * l->scale;
        int bh = h * l->scale;
        int x0, z0, x1, z1;
        uint64_t ss, cs;
        uint64_t potential, required;

        int specialcnt = filter->specialCnt;
        if (specialcnt > 0)
        {
            l = &g->layers[L_SPECIAL_1024];
            x0 = (bx) / l->scale; if (x < 0) x0--;
            z0 = (bz) / l->scale; if (z < 0) z0--;
            x1 = (bx + bw) / l->scale; if (x+(int)w >= 0) x1++;
            z1 = (bz + bh) / l->scale; if (z+(int)h >= 0) z1++;
            ss = getStartSeed(seed, l->layerSalt);

            for (j = z0; j <= z1; j++)
            {
                for (i = x0; i <= x1; i++)
                {
                    cs = getChunkSeed(ss, i, j);
                    if (mcFirstIsZero(cs, 13))
                        specialcnt--;
                }
            }
            if (specialcnt > 0)
                return 0;
        }

        l = &g->layers[L_BIOME_256];
        x0 = bx / l->scale; if (x < 0) x0--;
        z0 = bz / l->scale; if (z < 0) z0--;
        x1 = (bx + bw) / l->scale; if (x+(int)w >= 0) x1++;
        z1 = (bz + bh) / l->scale; if (z+(int)h >= 0) z1++;

        if (filter->majorToFind & (1ULL << mushroom_fields))
        {
            ss = getStartSeed(seed, g->layers[L_MUSHROOM_256].layerSalt);
            bool has_proto_mushroom = false;

            for (j = z0; j <= z1; j++)
            {
                for (i = x0; i <= x1; i++)
                {
                    cs = getChunkSeed(ss, i, j);
                    if (mcFirstIsZero(cs, 100))
                    {
                        has_proto_mushroom = true;
                        break;
                    }
                }
                if (has_proto_mushroom)
                    break;
            }
            if (!has_proto_mushroom)
                return 0;
        }

        potential = 0;
        required = filter->majorToFind & (
                (1ULL << badlands_plateau) | (1ULL << wooded_badlands_plateau) |
                (1ULL << desert) | (1ULL << savanna) | (1ULL << plains) |
                (1ULL << forest) | (1ULL << dark_forest) | (1ULL << mountains) |
                (1ULL << birch_forest) | (1ULL << swamp));

        ss = getStartSeed(seed, l->layerSalt);

        for (j = z0; j <= z1; j++)
        {
            for (i = x0; i <= x1; i++)
            {
                cs = getChunkSeed(ss, i, j);
                int cs6 = mcFirstInt(cs, 6);
                int cs3 = mcFirstInt(cs, 3);
                int cs4 = mcFirstInt(cs, 4);

                if (cs3) potential |= (1ULL << badlands_plateau);
                else potential |= (1ULL << wooded_badlands_plateau);

                switch (cs6)
                {
                case 0: potential |= (1ULL << desert) | (1ULL << forest); break;
                case 1: potential |= (1ULL << desert) | (1ULL << dark_forest); break;
                case 2: potential |= (1ULL << desert) | (1ULL << mountains); break;
                case 3: potential |= (1ULL << savanna) | (1ULL << plains); break;
                case 4: potential |= (1ULL << savanna) | (1ULL << birch_forest); break;
                case 5: potential |= (1ULL << plains) | (1ULL << swamp); break;
                }

                if (cs4 == 3) potential |= (1ULL << snowy_taiga);
                else potential |= (1ULL << snowy_tundra);
            }
        }

        if ((potential & required) ^ required)
            return 0;
    }

    l = g->layers;
    if (ids == nullptr)
    {
        memsiz = getMinLayerCacheSize(entry, w, h);
        owned_ids.resize(static_cast<std::size_t>(memsiz), 0);
        ids = owned_ids.data();
    }

    if ((filter->biomeToExcl | filter->biomeToExclM) && w*h > 1)
    {
        err = 0;
        if (memsiz == 0)
            memsiz = getMinLayerCacheSize(entry, w, h);
        mem1x1 = getMinLayerCacheSize(entry, 1, 1);
        if (mem1x1 * 2 < memsiz)
        {
            setLayerSeed(entry, seed);
            err = testExclusion(entry, ids, x+w/2, z+h/2, filter);
        }
        if (mem1x1 * 5 < memsiz)
        {
            if (!err) err = testExclusion(entry, ids, x,     z,     filter);
            if (!err) err = testExclusion(entry, ids, x+w-1, z+h-1, filter);
            if (!err) err = testExclusion(entry, ids, x,     z+h-1, filter);
            if (!err) err = testExclusion(entry, ids, x+w-1, z,     filter);
        }
        if (err)
            return 0;
    }

    filter_data_t fd[9];
    ScopedLayerMapOverride guard0(fd[0], filter, l[L_OCEAN_MIX_4], mapFilterOceanMix);
    ScopedLayerMapOverride guard1(fd[1], filter, l[L_RIVER_MIX_4], mapFilterRiverMix);
    ScopedLayerMapOverride guard2(fd[2], filter, l[L_SHORE_16], mapFilterShore);
    ScopedLayerMapOverride guard3(fd[3], filter, l[L_SUNFLOWER_64], mapFilterRareBiome);
    ScopedLayerMapOverride guard4(fd[4], filter, l[L_BIOME_EDGE_64], mapFilterBiomeEdge);
    ScopedLayerMapOverride guard5(fd[5], filter, l[L_OCEAN_TEMP_256], mapFilterOceanTemp);
    ScopedLayerMapOverride guard6(fd[6], filter, l[L_BIOME_256], mapFilterBiome);
    ScopedLayerMapOverride guard7(fd[7], filter, l[L_MUSHROOM_256], mapFilterMushroom);
    ScopedLayerMapOverride guard8(fd[8], filter, l[L_SPECIAL_1024], mapFilterSpecial);

    ret = 0;
    setLayerSeed(entry, seed);
    err = cubiomes::cpp::invoke_layer_map(
        *entry,
        std::span<int>{ids, static_cast<std::size_t>(w) * static_cast<std::size_t>(h)},
        x,
        z,
        static_cast<std::int32_t>(w),
        static_cast<std::int32_t>(h)
    );
    if (err == 0)
    {
        uint64_t b = 0, m = 0;
        unsigned int i;
        for (i = 0; i < w*h; i++)
        {
            int id = ids[i];
            if (id < 128) b |= (1ULL << id);
            else m |= (1ULL << (id-128));
        }

        int match_exc = (filter->biomeToExcl|filter->biomeToExclM) == 0;
        int match_any = (filter->biomeToPick|filter->biomeToPickM) == 0;
        int match_req = (filter->biomeToFind|filter->biomeToFindM) == 0;
        match_exc |= ((b & filter->biomeToExcl) || (m & filter->biomeToExclM)) == 0;
        match_any |= ((b & filter->biomeToPick) || (m & filter->biomeToPickM));
        match_req |= ((b & filter->biomeToFind)  == filter->biomeToFind &&
                      (m & filter->biomeToFindM) == filter->biomeToFindM);
        if (match_exc && match_any && match_req)
            ret = 1;
    }
    else if (err == M_STOP)
    {   // biome requirements not met
        ret = 0;
    }
    else if (err == M_DONE)
    {   // exclusion biomes cannot generate
        ret = 2;
    }

    return ret;
}


int checkForTemps(LayerStack *g, uint64_t seed, int x, int z, int w, int h, const int tc[9])
{
    uint64_t ls = getLayerSalt(3); // L_SPECIAL_1024 layer seed
    uint64_t ss = getStartSeed(seed, ls);

    int i, j;
    int scnt = 0;

    if (tc[Special+Warm] > 0) scnt += tc[Special+Warm];
    if (tc[Special+Lush] > 0) scnt += tc[Special+Lush];
    if (tc[Special+Cold] > 0) scnt += tc[Special+Cold];

    if (scnt > 0)
    {
        for (j = 0; j < h; j++)
        {
            for (i = 0; i < w; i++)
            {
                if (mcFirstIsZero(getChunkSeed(ss, x+i, z+j), 13))
                    scnt--;
            }
        }
        if (scnt > 0)
            return 0;
    }

    Layer *l = &g->layers[L_SPECIAL_1024];
    int ccnt[9] = {0};
    std::vector<int> area(getMinLayerCacheSize(l, w, h), 0);
    int ret = 1;

    setLayerSeed(l, seed);
    genArea(l, area.data(), x, z, w, h);

    for (i = 0; i < w*h; i++)
    {
        int id = area[static_cast<std::size_t>(i)];
        int t = id & 0xff;
        if (id != t && t != Freezing)
            t += Special;
        ccnt[t]++;
    }
    for (i = 0; i < 9; i++)
    {
        if (ccnt[i] < tc[i] || (ccnt[i] && tc[i] < 0))
        {
            ret = 0;
            break;
        }
    }

    return ret;
}


struct locate_info_t
{
    Generator *g;
    int *ids;
    Range r;
    int match, tol;
    volatile char *stop;
};

static
int floodFillGen(struct locate_info_t *info, int i, int j, Pos *p)
{
    typedef struct { int i, j, d; } entry_t;
    std::vector<entry_t> queue(static_cast<std::size_t>(info->r.sx) * static_cast<std::size_t>(info->r.sz));
    int qn = 1;
    queue[0] = entry_t{.i = i, .j = j, .d = 0};
    int64_t sumx = 0;
    int64_t sumz = 0;
    int n = 0;
    while (--qn >= 0)
    {
        if (info->stop && *info->stop)
        {
            return 0;
        }
        int d = queue[static_cast<std::size_t>(qn)].d;
        i = queue[static_cast<std::size_t>(qn)].i;
        j = queue[static_cast<std::size_t>(qn)].j;
        int k = j * info->r.sx + i;
        int id = info->ids[k];
        if (id == INT_MAX)
            continue;
        info->ids[k] = INT_MAX;
        int x = info->r.x + i;
        int z = info->r.z + j;
        if (info->g->mc >= MC_1_18)
            id = getBiomeAt(info->g, info->r.scale, x, info->r.y, z);
        if (id == info->match)
        {
            sumx += x;
            sumz += z;
            n++;
            d = 0;
        }
        else
        {
            if (++d >= info->tol)
                continue;
        }
        entry_t next[] = { {i,j-1,d}, {i,j+1,d}, {i-1,j,d}, {i+1,j,d} };
        for (k = 0; k < 4; k++)
        {
            i = next[k].i; j = next[k].j;
            if (i < 0 || i >= info->r.sx || j < 0 || j >= info->r.sz)
                continue;
            if (info->ids[j * info->r.sx + i] == INT_MAX)
                continue;
            queue[static_cast<std::size_t>(qn++)] = next[k];
        }
    }
    if (n)
    {
        p->x = (int) round((sumx / (double)n + 0.5) * info->r.scale);
        p->z = (int) round((sumz / (double)n + 0.5) * info->r.scale);
    }
    return n;
}


int getBiomeCenters(Pos *pos, int *siz, int nmax, Generator *g, Range r,
    int match, int minsiz, int tol, volatile char *stop)
{
    if (minsiz <= 0)
        minsiz = 1;
    int i, j, k, n = 0;
    std::vector<int> ids(static_cast<std::size_t>(r.sx) * static_cast<std::size_t>(r.sz), -1);
    if (tol <= 0)
        tol = 1;
    int step = tol;
    struct locate_info_t info;
    info.g = g;
    info.ids = ids.data();
    info.r = r;
    info.stop = stop;
    info.match = match;
    info.tol = tol;

    if (g->mc >= MC_1_18)
    {
        const int *lim = getBiomeParaLimits(g->mc, match);

        int para[] = {
            NP_TEMPERATURE,
            NP_HUMIDITY,
            NP_EROSION,
            NP_CONTINENTALNESS,
            NP_WEIRDNESS,
        };
        int npara = sizeof(para) / sizeof(para[0]);
        if (tol == 1)
            step = 1 + floor(sqrt(minsiz) * 0.5);

        for (j = 0; j < r.sz; j += step)
        {
            for (i = 0; i < r.sx; i += step)
            {
                if (stop && *stop)
                    break;
                for (k = 0; k < npara; k++)
                {
                    const int *plim = lim + 2*para[k];
                    if (plim[0] == INT_MIN && plim[1] == INT_MAX)
                        continue;
                    DoublePerlinNoise *dpn = &g->bn.climate[para[k]];
                    double px = (r.x+i) * r.scale / 4.0;
                    double pz = (r.z+j) * r.scale / 4.0;
                    int p = 10000 * sampleDoublePerlin(dpn, px, 0, pz);
                    if (p < plim[0] || p > plim[1])
                    {
                        ids[static_cast<std::size_t>(j*r.sx + i)] = -2;
                        break;
                    }
                }
            }
        }
        match = -1; // id entries that are still -1 are our candidates
    }
    else // 1.17-
    {
        int ts = 32 / r.scale;
        if (r.sx + r.sz < 32)
            ts = 8;

        int tx = (int) floor(r.x / (double)ts);
        int tz = (int) floor(r.z / (double)ts);
        int tw = (int) ceil((r.x+r.sx) / (double)ts) - tx;
        int th = (int) ceil((r.z+r.sz) / (double)ts) - tz;
        int ti, tj;

        BiomeFilter bf;
        setupBiomeFilter(&bf, g->mc, 0, &match, 1, 0, 0, 0, 0);
        //applySeed(g, 0, g->seed);

        Range tr = { r.scale, 0, 0, ts, ts, 0, 1 };
        std::vector<int> cache(getMinCacheSize(g, tr.scale, tr.sx, tr.sy, tr.sz), 0);

        for (tj = 0; tj < th; tj++)
        {
            for (ti = 0; ti < tw; ti++)
            {
                if (stop && *stop)
                    break;
                tr.x = (tx+ti) * ts;
                tr.z = (tz+tj) * ts;
                if (checkForBiomes(g, cache.data(), tr, DIM_OVERWORLD, g->seed,
                    &bf, stop) != 1)
                {
                    continue;
                }
                for (j = 0; j < ts; j++)
                {
                    int jj = tr.z + j - r.z;
                    if (jj < 0 || jj >= r.sz)
                        continue;
                    for (i = 0; i < ts; i++)
                    {
                        int ii = tr.x + i - r.x;
                        if (ii < 0 || ii >= r.sx)
                            continue;
                        ids[static_cast<std::size_t>(jj*r.sx + ii)] = cache[static_cast<std::size_t>(j*tr.sx + i)];
                    }
                }
            }
        }
    }

    applySeed(g, DIM_OVERWORLD, g->seed);
    for (j = 0; j < r.sz; j += step)
    {
        for (i = 0; i < r.sx; i += step)
        {
            if (stop && *stop)
                break;
            if (ids[static_cast<std::size_t>(j*r.sx + i)] != match)
                continue;
            Pos center;
            int area = floodFillGen(&info, i, j, &center);
            if (area >= minsiz)
            {
                pos[n] = center;
                if (siz) siz[n] = area;
                if (++n >= nmax)
                    return n;
            }
        }
    }
    return n;
}


int canBiomeGenerate(int layerId, int mc, uint32_t flags, int id)
{
    int dofilter = 0;

    if (mc >= MC_1_13)
    {
        if (layerId == L_OCEAN_TEMP_256)
            return isShallowOcean(id);
        if ((flags & FORCE_OCEAN_VARIANTS) && isOceanic(id))
            return id != deep_warm_ocean;
    }

    if (dofilter || layerId == L_BIOME_256)
    {
        dofilter = 1;
        if (id >= 64)
            return 0;
    }
    if (dofilter || (layerId == L_BAMBOO_256 && mc >= MC_1_14))
    {
        dofilter = 1;
        switch (id)
        {
        case jungle_edge:
        case wooded_mountains:
        case badlands:
            return 0;
        }
    }
    if (dofilter || (layerId == L_BIOME_EDGE_64 && mc >= MC_1_7))
    {
        dofilter = 1;
        if (id >= 64 && id != bamboo_jungle)
            return 0;
        switch (id)
        {
        case snowy_mountains:
        case desert_hills:
        case wooded_hills:
        case taiga_hills:
        case jungle_hills:
        case birch_forest_hills:
        case snowy_taiga_hills:
        case giant_tree_taiga_hills:
        case savanna_plateau:
            return 0;
        }
    }
    if (dofilter || (layerId == L_ZOOM_64 && mc <= MC_1_0))
    {
        dofilter = 1;
        if (id == mushroom_field_shore)
            return 0;
    }
    if (dofilter || layerId == L_HILLS_64)
    {
        dofilter = 1;
        if (id == frozen_ocean)
            return 0;
        // sunflower_plains actually generates at Hills layer as well
    }
    if (dofilter || (layerId == L_ZOOM_16 && mc <= MC_1_6))
    {
        dofilter = 1;
        if (id == mountain_edge)
            return 0;
    }
    if (dofilter || (layerId == L_SUNFLOWER_64 && mc >= MC_1_7))
    {
        dofilter = 1;
        switch (id)
        {
        case beach:
        case stone_shore:
        case snowy_beach:
            return 0;
        case mushroom_field_shore:
            if (mc != MC_1_0)
                return 0;
            break;
        }
    }
    if (dofilter || layerId == L_SHORE_16)
    {
        dofilter = 1;
        if (id == river)
            return 0;
    }
    if (dofilter || (layerId == L_SWAMP_RIVER_16 && mc <= MC_1_6))
    {
        dofilter = 1;
        if (id == frozen_river)
            return 0;
    }
    if (dofilter || layerId == L_RIVER_MIX_4)
    {
        dofilter = 1;
        if (isDeepOcean(id) && id != deep_ocean)
            return 0;
        if (isShallowOcean(id) && id != ocean)
        {
            if (mc >= MC_1_7 || id != frozen_ocean)
                return 0;
        }
    }
    if (dofilter || (layerId == L_OCEAN_MIX_4 && mc >= MC_1_13))
    {
        dofilter = 1;
    }

    if (!dofilter && layerId != L_VORONOI_1)
    {
        printf("canBiomeGenerate(): unsupported layer (%d) or version (%d)\n",
            layerId, mc);
        return 0;
    }
    return isOverworld(mc, id);
}

void getAvailableBiomes(uint64_t *mL, uint64_t *mM, int layerId, int mc, uint32_t flags)
{
    *mL = *mM = 0;
    int i;
    if (mc <= MC_B1_7 || mc >= MC_1_18)
    {
        for (i = 0; i < 64; i++)
        {
            if (isOverworld(mc, i))
                *mL |= (1ULL << i);
            if (isOverworld(mc, i+128))
                *mM |= (1ULL << i);
        }
    }
    else if (mc >= MC_1_13 && layerId == L_OCEAN_TEMP_256)
    {
        *mL =
            (1ULL << ocean) |
            (1ULL << frozen_ocean) |
            (1ULL << warm_ocean) |
            (1ULL << lukewarm_ocean) |
            (1ULL << cold_ocean);
    }
    else
    {
        for (i = 0; i < 64; i++)
        {
            if (canBiomeGenerate(layerId, mc, i, flags))
                *mL |= (1ULL << i);
            if (canBiomeGenerate(layerId, mc, i+128, flags))
                *mM |= (1ULL << i);
        }
    }
}

struct _gp_args
{
    uint64_t *mL, *mM;
    int mc;
    uint32_t flags;
};

static void _genPotential(struct _gp_args *a, int layer, int id)
{
    int mc = a->mc;
    // filter out bad biomes
    if (layer >= L_BIOME_256 && !canBiomeGenerate(layer, mc, a->flags, id))
        return;

    switch (layer)
    {
    case L_SPECIAL_1024: // biomes added in (L_SPECIAL_1024, L_MUSHROOM_256]
        if (mc <= MC_1_6) {
            printf("genPotential() bad layer %d for version\n", layer);
            return;
        }
        if (id == Oceanic)
            _genPotential(a, L_MUSHROOM_256, mushroom_fields);
        if ((id & ~0xf00) >= Oceanic && (id & ~0xf00) <= Freezing)
            _genPotential(a, L_MUSHROOM_256, id);
        break;

    case L_MUSHROOM_256: // biomes added in (L_MUSHROOM_256, L_DEEP_OCEAN_256]
        if (mc >= MC_1_7) {
            if (id == Oceanic)
                _genPotential(a, L_DEEP_OCEAN_256, deep_ocean);
            if (id == mushroom_fields)
                _genPotential(a, L_DEEP_OCEAN_256, id);
            if ((id & ~0xf00) >= Oceanic && (id & ~0xf00) <= Freezing)
                _genPotential(a, L_DEEP_OCEAN_256, id);
        } else { // (L_MUSHROOM_256, L_BIOME_256] for 1.6
            if (id == ocean || id == mushroom_fields) {
                _genPotential(a, L_BIOME_256, id);
            } else {
                _genPotential(a, L_BIOME_256, desert);
                _genPotential(a, L_BIOME_256, forest);
                _genPotential(a, L_BIOME_256, mountains);
                _genPotential(a, L_BIOME_256, swamp);
                _genPotential(a, L_BIOME_256, plains);
                _genPotential(a, L_BIOME_256, taiga);
                if (mc >= MC_1_2)
                    _genPotential(a, L_BIOME_256, jungle);
                if (id != plains)
                    _genPotential(a, L_BIOME_256, snowy_tundra);
            }
        }
        break;

    case L_DEEP_OCEAN_256: // biomes added in (L_DEEP_OCEAN_256, L_BIOME_256]
        if (mc <= MC_1_6) {
            printf("genPotential() bad layer %d for version\n", layer);
            return;
        }
        switch (id & ~0xf00)
        {
        case Warm:
            if (id & 0xf00) {
                _genPotential(a, L_BIOME_256, badlands_plateau);
                _genPotential(a, L_BIOME_256, wooded_badlands_plateau);
            } else {
                _genPotential(a, L_BIOME_256, desert);
                _genPotential(a, L_BIOME_256, savanna);
                _genPotential(a, L_BIOME_256, plains);
            }
            break;
        case Lush:
            if (id & 0xf00) {
                _genPotential(a, L_BIOME_256, jungle);
            } else {
                _genPotential(a, L_BIOME_256, forest);
                _genPotential(a, L_BIOME_256, dark_forest);
                _genPotential(a, L_BIOME_256, mountains);
                _genPotential(a, L_BIOME_256, plains);
                _genPotential(a, L_BIOME_256, birch_forest);
                _genPotential(a, L_BIOME_256, swamp);
            }
            break;
        case Cold:
            if (id & 0xf00) {
                _genPotential(a, L_BIOME_256, giant_tree_taiga);
            } else {
                _genPotential(a, L_BIOME_256, forest);
                _genPotential(a, L_BIOME_256, mountains);
                _genPotential(a, L_BIOME_256, taiga);
                _genPotential(a, L_BIOME_256, plains);
            }
            break;
        case Freezing:
            _genPotential(a, L_BIOME_256, snowy_tundra);
            _genPotential(a, L_BIOME_256, snowy_taiga);
            break;
        default:
            id &= ~0xf00;
            _genPotential(a, L_BIOME_256, id);
        }
        break;

    case L_BIOME_256: // biomes added in (L_BIOME_256, L_BIOME_EDGE_64]
    case L_BAMBOO_256:
    case L_ZOOM_64:
        if (mc <= MC_1_13 && layer == L_BAMBOO_256) {
            printf("genPotential() bad layer %d for version\n", layer);
            return;
        }
        if (mc >= MC_1_7) {
            if (mc >= MC_1_14 && id == jungle)
                _genPotential(a, L_BIOME_EDGE_64, bamboo_jungle);
            if (id == wooded_badlands_plateau || id == badlands_plateau)
                _genPotential(a, L_BIOME_EDGE_64, badlands);
            else if(id == giant_tree_taiga)
                _genPotential(a, L_BIOME_EDGE_64, taiga);
            else if (id == desert)
                _genPotential(a, L_BIOME_EDGE_64, wooded_mountains);
            else if (id == swamp) {
                _genPotential(a, L_BIOME_EDGE_64, jungle_edge);
                _genPotential(a, L_BIOME_EDGE_64, plains);
            }
            _genPotential(a, L_BIOME_EDGE_64, id);
            break;
        }
        // (L_BIOME_256, L_HILLS_64] for 1.6
        // fallthrough

    case L_BIOME_EDGE_64: // biomes added in (L_BIOME_EDGE_64, L_HILLS_64]
        if (mc <= MC_1_6 && layer == L_BIOME_EDGE_64) {
            printf("genPotential() bad layer %d for version\n", layer);
            return;
        }
        if (!isShallowOcean(id) && getMutated(mc, id) > 0)
             _genPotential(a, L_HILLS_64, getMutated(mc, id));
        switch (id)
        {
        case desert:
            _genPotential(a, L_HILLS_64, desert_hills);
            break;
        case forest:
            _genPotential(a, L_HILLS_64, wooded_hills);
            break;
        case birch_forest:
            _genPotential(a, L_HILLS_64, birch_forest_hills);
            _genPotential(a, L_HILLS_64, getMutated(mc, birch_forest_hills));
            break;
        case dark_forest:
            _genPotential(a, L_HILLS_64, plains);
            _genPotential(a, L_HILLS_64, getMutated(mc, plains));
            break;
        case taiga:
            _genPotential(a, L_HILLS_64, taiga_hills);
            break;
        case giant_tree_taiga:
            _genPotential(a, L_HILLS_64, giant_tree_taiga_hills);
            _genPotential(a, L_HILLS_64, getMutated(mc, giant_tree_taiga_hills));
            break;
        case snowy_taiga:
            _genPotential(a, L_HILLS_64, snowy_taiga_hills);
            break;
        case plains:
            if (mc >= MC_1_7)
                _genPotential(a, L_HILLS_64, wooded_hills);
            _genPotential(a, L_HILLS_64, forest);
            _genPotential(a, L_HILLS_64, getMutated(mc, forest));
            break;
        case snowy_tundra:
            _genPotential(a, L_HILLS_64, snowy_mountains);
            break;
        case jungle:
            _genPotential(a, L_HILLS_64, jungle_hills);
            break;
        case bamboo_jungle:
            _genPotential(a, L_HILLS_64, bamboo_jungle_hills);
            break;
        case ocean:
            if (mc >= MC_1_7)
                _genPotential(a, L_HILLS_64, deep_ocean);
            break;
        case mountains:
            if (mc >= MC_1_7) {
                _genPotential(a, L_HILLS_64, wooded_mountains);
                _genPotential(a, L_HILLS_64, getMutated(mc, wooded_mountains));
            }
            break;
        case savanna:
            _genPotential(a, L_HILLS_64, savanna_plateau);
            _genPotential(a, L_HILLS_64, getMutated(mc, savanna_plateau));
            break;
        default:
            if (areSimilar(mc, id, wooded_badlands_plateau))
            {
                _genPotential(a, L_HILLS_64, badlands);
                _genPotential(a, L_HILLS_64, getMutated(mc, badlands));
            }
            else if (isDeepOcean(id))
            {
                _genPotential(a, L_HILLS_64, plains);
                _genPotential(a, L_HILLS_64, forest);
                _genPotential(a, L_HILLS_64, getMutated(mc, plains));
                _genPotential(a, L_HILLS_64, getMutated(mc, forest));
            }
        }
        _genPotential(a, L_HILLS_64, id);
        break;

    case L_HILLS_64: // biomes added in (L_HILLS_64, L_RARE_BIOME_64]
        if (mc <= MC_1_6) { // (L_HILLS_64, L_SHORE_16] for 1.6
            if (id == mushroom_fields)
                _genPotential(a, L_SHORE_16, mushroom_field_shore);
            else if (id == mountains)
                _genPotential(a, L_SHORE_16, mountain_edge);
            else if (id != ocean && id != river && id != swamp)
                _genPotential(a, L_SHORE_16, beach);
            _genPotential(a, L_SHORE_16, id);
        } else {
            if (id == plains)
                _genPotential(a, L_SUNFLOWER_64, sunflower_plains);
            _genPotential(a, L_SUNFLOWER_64, id);
        }
        break;

    case L_SUNFLOWER_64: // biomes added in (L_SUNFLOWER_64, L_SHORE_16] 1.7+
        if (mc <= MC_1_6) {
            printf("genPotential() bad layer %d for version\n", layer);
            return;
        }
        // fallthrough
    case L_ZOOM_16:
        if (mc <= MC_1_0 && layer == L_ZOOM_16) {
            _genPotential(a, L_SHORE_16, id);
            break;
        }
        if (id == mushroom_fields)
            _genPotential(a, L_SHORE_16, mushroom_field_shore);
        else if (getCategory(mc, id) == jungle) {
            _genPotential(a, L_SHORE_16, beach);
            _genPotential(a, L_SHORE_16, jungle_edge);
        }
        else if (id == mountains || id == wooded_mountains || id == mountain_edge)
            _genPotential(a, L_SHORE_16, stone_shore);
        else if (isSnowy(id))
            _genPotential(a, L_SHORE_16, snowy_beach);
        else if (id == badlands || id == wooded_badlands_plateau)
            _genPotential(a, L_SHORE_16, desert);
        else if (id != ocean && id != deep_ocean && id != river && id != swamp)
            _genPotential(a, L_SHORE_16, beach);
        _genPotential(a, L_SHORE_16, id);
        break;

    case L_SHORE_16: // biomes added in (L_SHORE_16, L_RIVER_MIX_4]
    case L_SWAMP_RIVER_16:
    case L_ZOOM_4:
        if (id == snowy_tundra)
            _genPotential(a, L_RIVER_MIX_4, frozen_river);
        else if (id == mushroom_fields || id == mushroom_field_shore)
            _genPotential(a, L_RIVER_MIX_4, mushroom_field_shore);
        else if (id != ocean && (mc <= MC_1_6 || !isOceanic(id)))
            _genPotential(a, L_RIVER_MIX_4, river);
        _genPotential(a, L_RIVER_MIX_4, id);
        break;

    case L_RIVER_MIX_4: // biomes added in (L_RIVER_MIX_4, L_VORONOI_1]
        if (mc >= MC_1_13 && isOceanic(id)) {
            if (id == ocean) {
                _genPotential(a, L_VORONOI_1, ocean);
                _genPotential(a, L_VORONOI_1, warm_ocean);
                _genPotential(a, L_VORONOI_1, lukewarm_ocean);
                _genPotential(a, L_VORONOI_1, cold_ocean);
                _genPotential(a, L_VORONOI_1, frozen_ocean);
            } else if (id == deep_ocean) {
                _genPotential(a, L_VORONOI_1, deep_ocean);
                _genPotential(a, L_VORONOI_1, deep_lukewarm_ocean);
                _genPotential(a, L_VORONOI_1, deep_cold_ocean);
                _genPotential(a, L_VORONOI_1, deep_frozen_ocean);
            }
            else break;
        }
        _genPotential(a, L_VORONOI_1, id);
        break;

    case L_OCEAN_MIX_4:
        if (mc <= MC_1_12) {
            printf("genPotential() bad layer %d for version\n", layer);
            return;
        }
        // fallthrough

    case L_VORONOI_1:
        if (id < 128)   *a->mL |= 1ULL << id;
        else            *a->mM |= 1ULL << (id-128);
        break;

    default:
        printf("genPotential() not implemented for layer %d\n", layer);
    }
}

void genPotential(uint64_t *mL, uint64_t *mM, int layerId, int mc, uint32_t flags, int id)
{
    struct _gp_args args = { mL, mM, mc, flags };
    _genPotential(&args, layerId, id);
}


double getParaDescent(const DoublePerlinNoise *para, double factor,
    int x, int z, int w, int h, int i0, int j0, int maxrad,
    int maxiter, double alpha, void *data, int (*func)(void*,int,int,double))
{
    /// Do a gradient descent on a grid...
    /// To start with, we will just consider a step size of 1 in one axis:
    ///   Try going in positive x: if gradient is upwards go to negative x
    ///   then do the same with z - if all 4 directions go upwards then we have
    ///   found a minimum, otherwise repeat.
    /// We can remember and try the direction from the previous cycle first to
    /// reduce the number of wrong guesses.
    ///
    /// We can also use a larger step size than 1, as long as we believe that
    /// the minimum is not in between. To determine if this is viable, we check
    /// the step size of 1 first, and then jump if the gradient appears large
    /// enough in that direction.
    ///
    ///TODO:
    /// The perlin noise can be sampled continuously, so more established
    /// minima algorithms can also be considered...

    int dirx = 0, dirz = 0, dira;
    int k, i, j;
    double v, vd, va;
    v = factor * sampleDoublePerlin(para, x+i0, 0, z+j0);
    if (func)
    {
        if (func(data, x+i0, z+j0, factor < 0 ? -v : v))
            return nan("");
    }

    i = i0; j = j0;
    for (k = 0; k < maxiter; k++)
    {
        if (dirx == 0) dirx = +1;
        if (i+dirx >= 0 && i+dirx < w)
            vd = factor * sampleDoublePerlin(para, x+i+dirx, 0, z+j);
        else vd = v;
        if (vd >= v)
        {
            dirx *= -1;
            if (i+dirx >= 0 && i+dirx < w)
                vd = factor * sampleDoublePerlin(para, x+i+dirx, 0, z+j);
            else vd = v;
            if (vd >= v)
                dirx = 0;
        }
        if (dirx)
        {
            dira = (int)(dirx * alpha * (v - vd));
            bool jumped = false;
            if (abs(dira) > 2 && i+dira >= 0 && i+dira < w)
            {   // try jumping by more than 1
                va = factor * sampleDoublePerlin(para, x+i+dira, 0, z+j);
                if (va < vd)
                {
                    i += dira;
                    v = va;
                    jumped = true;
                }
            }
            if (!jumped)
            {
                v = vd;
                i += dirx;
            }
            if (func)
            {
                if (func(data, x+i, z+j, factor < 0 ? -v : v))
                    return nan("");
            }
        }

        if (dirz == 0) dirz = +1;
        if (j+dirz >= 0 && j+dirz < h)
            vd = factor * sampleDoublePerlin(para, x+i, 0, z+j+dirz);
        else vd = v;
        if (vd >= v)
        {
            dirz *= -1;
            if (j+dirz >= 0 && j+dirz < h)
                vd = factor * sampleDoublePerlin(para, x+i, 0, z+j+dirz);
            else vd = v;
            if (vd >= v)
                dirz = 0;
        }
        if (dirz)
        {
            dira = (int)(dirz * alpha * (v - vd));
            bool jumped = false;
            if (abs(dira) > 2 && j+dira >= 0 && j+dira < h)
            {   // try jumping by more than 1
                va = factor * sampleDoublePerlin(para, x+i, 0, z+j+dira);
                if (va < vd)
                {
                    j += dira;
                    v = va;
                    jumped = true;
                }
            }
            if (!jumped)
            {
                j += dirz;
                v = vd;
            }
            if (func)
            {
                if (func(data, x+i, z+j, factor < 0 ? -v : v))
                    return nan("");
            }
        }
        if (dirx == 0 && dirz == 0)
        {   // this is very likely a fix point
            // but there could be a minimum along a diagonal path in rare cases
            int c;
            for (c = 0; c < 4; c++)
            {
                dirx = (c & 1) ? -1 : +1;
                dirz = (c & 2) ? -1 : +1;
                if (i+dirx < 0 || i+dirx >= w || j+dirz < 0 || j+dirz >= h)
                    continue;
                vd = factor * sampleDoublePerlin(para, x+i+dirx, 0, z+j+dirz);
                if (vd < v)
                {
                    v = vd;
                    i += dirx;
                    j += dirz;
                    break;
                }
            }
            if (c >= 4)
                break;
        }
        if (abs(i - i0) > maxrad || abs(j - j0) > maxrad)
            break; // we have gone too far from the origin
    }

    return v;
}


int getParaRange(const DoublePerlinNoise *para, double *pmin, double *pmax,
    int x, int z, int w, int h, void *data, int (*func)(void*,int,int,double))
{
    const double beta = 1.5;
    const double factor = 10000;
    const double perlin_grad = 2.0 * 1.875; // max perlin noise gradient
    double v, lmin, lmax, dr, vdif, small_regime;
    std::vector<char> skip{};
    int i, j, step, ii, jj, ww, hh, skipsiz;
    int maxrad, maxiter;
    int err = 1;
    const auto is_invalid = [](double value) { return std::isnan(value); };

    if (pmin) *pmin = DBL_MAX;
    if (pmax) *pmax = -DBL_MAX;

    lmin = DBL_MAX, lmax = 0;
    for (i = 0; i < para->octA.octcnt; i++)
    {
        double lac = para->octA.octaves[i].lacunarity;
        if (lac < lmin) lmin = lac;
        if (lac > lmax) lmax = lac;
    }

    // Sort out the small area cases where we are less likely to improve upon
    // checking all positions.
    small_regime = 1e3 * sqrt(lmax);
    if (w*h < small_regime)
    {
        for (j = 0; j < h; j++)
        {
            for (i = 0; i < w; i++)
            {
                v = factor * sampleDoublePerlin(para, x+i, 0, z+j);
                if (func)
                {
                    err = func(data, x+i, z+j, v);
                    if (err)
                        return err;
                }
                if (pmin && v < *pmin) *pmin = v;
                if (pmax && v > *pmax) *pmax = v;
            }
        }
        return 0;
    }

    // Start with the largest noise period to get some bounds for pmin, pmax
    step = (int) (0.5 / lmin - FLT_EPSILON) + 1;

    dr = lmax / lmin * beta;
    for (j = 0; j < h; j += step)
    {
        for (i = 0; i < w; i += step)
        {
            if (pmin)
            {
                v = getParaDescent(para, +factor, x, z, w, h, i, j,
                    step, step, dr, data, func);
                if (is_invalid(v))
                    return err;
                if (v < *pmin) *pmin = v;
            }
            if (pmax)
            {
                v = -getParaDescent(para, -factor, x, z, w, h, i, j,
                    step, step, dr, data, func);
                if (is_invalid(v))
                    return err;
                if (v > *pmax) *pmax = v;
            }
        }
    }

    //(*(double*)data) = -1e9+1; // testing

    step = (int) (1.0 / (perlin_grad * lmax + FLT_EPSILON)) + 1;

    /// We can determine the maximum contribution we expect from all noise
    /// periods for a distance of step. If this does not account for the
    /// necessary difference, we can skip that point.
    vdif = 0;
    for (i = 0; i < para->octA.octcnt; i++)
    {
        const PerlinNoise *p = para->octA.octaves + i;
        double contrib = step * p->lacunarity * 1.0;
        if (contrib > 1.0) contrib = 1;
        vdif += contrib * p->amplitude;
    }
    for (i = 0; i < para->octB.octcnt; i++)
    {
        const double lac_factB = 337.0 / 331.0;
        const PerlinNoise *p = para->octB.octaves + i;
        double contrib = step * p->lacunarity * lac_factB;
        if (contrib > 1.0) contrib = 1;
        vdif += contrib * p->amplitude;
    }
    vdif = fabs(factor * vdif * para->amplitude);
    //printf("%g %g %g\n", para->amplitude, 1./lmin, 1./lmax);
    //printf("first pass: [%g %g] diff=%g step:%d\n", *pmin, *pmax, vdif, step);

    maxrad = step;
    maxiter = step*2;
    ww = (w+step-1) / step;
    hh = (h+step-1) / step;
    skipsiz = (ww+1) * (hh+1);
    skip.resize(static_cast<std::size_t>(skipsiz));

    if (pmin)
    {   // look for minima
        std::fill(skip.begin(), skip.end(), 0);

        for (jj = 0; jj <= hh; jj++)
        {
            j = jj * step; if (j >= h) j = h-1;
            for (ii = 0; ii <= ww; ii++)
            {
                i = ii * step; if (i >= w) i = w-1;
                if (skip[static_cast<std::size_t>(jj*ww+ii)]) continue;

                v = factor * sampleDoublePerlin(para, x+i, 0, z+j);
                if (func)
                {
                    int e = func(data, x+i, z+j, v);
                    if (e)
                    {
                        err = e;
                        return err;
                    }
                }
                // not looking for maxima yet, but update the bounds anyway
                if (pmax && v > *pmax) *pmax = v;

                dr = beta * (v - *pmin) / vdif;
                if (dr > 1.0)
                {   // difference is too large -> mark visinity to be skipped
                    int a, b, r = (int) dr;
                    for (b = 0; b < r; b++)
                    {
                        if (b+jj < 0 || b+jj >= hh) continue;
                        for (a = -r+1; a < r; a++)
                        {
                            if (a+ii < 0 || a+ii >= ww) continue;
                            skip[static_cast<std::size_t>((b+jj)*ww + (a+ii))] = 1;
                        }
                    }
                    continue;
                }
                v = getParaDescent(para, +factor, x, z, w, h, i, j,
                    maxrad, maxiter, dr, data, func);
                if (is_invalid(v))
                    return err;
                if (v < *pmin) *pmin = v;
            }
        }
    }

    if (pmax)
    {   // look for maxima
        std::fill(skip.begin(), skip.end(), 0);

        for (jj = 0; jj <= hh; jj++)
        {
            j = jj * step; if (j >= h) j = h-1;
            for (ii = 0; ii <= ww; ii++)
            {
                i = ii * step; if (i >= w) i = w-1;
                if (skip[static_cast<std::size_t>(jj*ww+ii)]) continue;

                v = -factor * sampleDoublePerlin(para, x+i, 0, z+j);
                if (func)
                {
                    int e = func(data, x+i, z+j, -v);
                    if (e)
                    {
                        err = e;
                        return err;
                    }
                }

                dr = beta * (v + *pmax) / vdif;
                if (dr > 1.0)
                {   // difference too large -> mark visinity to be skipped
                    int a, b, r = (int) dr;
                    for (b = 0; b < r; b++)
                    {
                        if (b+jj < 0 || b+jj >= hh) continue;
                        for (a = -r+1; a < r; a++)
                        {
                            if (a+ii < 0 || a+ii >= ww) continue;
                            skip[static_cast<std::size_t>((b+jj)*ww + (a+ii))] = 1;
                        }
                    }
                    continue;
                }
                v = -getParaDescent(para, -factor, x, z, w, h, i, j,
                    maxrad, maxiter, dr, data, func);
                if (is_invalid(v))
                    return err;
                if (v > *pmax) *pmax = v;
            }
        }
    }

    err = 0;
    return err;
}

#define IMIN INT_MIN
#define IMAX INT_MAX
static const int g_biome_para_range_18[][13] = {
/// biome                   temperature  humidity     continental. erosion      depth        weirdness
{ocean                   , -1500, 2000,  IMIN, IMAX, -4550,-1900,  IMIN, IMAX,  IMIN, IMAX,  IMIN, IMAX},
{plains                  , -4500, 5500,  IMIN, 1000, -1900, IMAX,  IMIN, IMAX,  IMIN, IMAX,  IMIN, IMAX},
{desert                  ,  5500, IMAX,  IMIN, IMAX, -1900, IMAX,  IMIN, IMAX,  IMIN, IMAX,  IMIN, IMAX},
{windswept_hills         ,  IMIN, 2000,  IMIN, 1000, -1899, IMAX,  4500, 5500,  IMIN, IMAX,  IMIN, IMAX},
{forest                  , -4500, 5500, -1000, 3000, -1900, IMAX,  IMIN, IMAX,  IMIN, IMAX,  IMIN, IMAX},
{taiga                   ,  IMIN,-1500,  1000, IMAX, -1900, IMAX,  IMIN, IMAX,  IMIN, IMAX,  IMIN, IMAX},
{swamp                   , -4500, IMAX,  IMIN, IMAX, -1100, IMAX,  5500, IMAX,  IMIN, IMAX,  IMIN, IMAX},
{river                   , -4500, IMAX,  IMIN, IMAX, -1900, IMAX,  IMIN, IMAX,  IMIN, IMAX,  -500,  500},
{frozen_ocean            ,  IMIN,-4501,  IMIN, IMAX, -4550,-1900,  IMIN, IMAX,  IMIN, IMAX,  IMIN, IMAX},
{frozen_river            ,  IMIN,-4501,  IMIN, IMAX, -1900, IMAX,  IMIN, IMAX,  IMIN, IMAX,  -500,  500},
{snowy_plains            ,  IMIN,-4500,  IMIN, 1000, -1900, IMAX,  IMIN, IMAX,  IMIN, IMAX,  IMIN, IMAX},
{mushroom_fields         ,  IMIN, IMAX,  IMIN, IMAX, IMIN,-10500,  IMIN, IMAX,  IMIN, IMAX,  IMIN, IMAX},
{beach                   , -4500, 5500,  IMIN, IMAX, -1900,-1100, -2225, IMAX,  IMIN, IMAX,  IMIN, 2666},
{jungle                  ,  2000, 5500,  1000, IMAX, -1900, IMAX,  IMIN, IMAX,  IMIN, IMAX,  IMIN, IMAX},
{sparse_jungle           ,  2000, 5500,  1000, 3000, -1899, IMAX,  IMIN, IMAX,  IMIN, IMAX,  -500, IMAX},
{deep_ocean              , -1500, 2000,  IMIN, IMAX,-10500,-4551,  IMIN, IMAX,  IMIN, IMAX,  IMIN, IMAX},
{stony_shore             ,  IMIN, IMAX,  IMIN, IMAX, -1900,-1100,  IMIN,-2225,  IMIN, IMAX,  IMIN, IMAX},
{snowy_beach             ,  IMIN,-4500,  IMIN, IMAX, -1900,-1100, -2225, IMAX,  IMIN, IMAX,  IMIN, 2666},
{birch_forest            , -1500, 2000,  1000, 3000, -1900, IMAX,  IMIN, IMAX,  IMIN, IMAX,  IMIN, IMAX},
{dark_forest             , -1500, 2000,  3000, IMAX, -1900, IMAX,  IMIN, IMAX,  IMIN, IMAX,  IMIN, IMAX},
{snowy_taiga             ,  IMIN,-4500, -1000, IMAX, -1900, IMAX,  IMIN, IMAX,  IMIN, IMAX,  IMIN, IMAX},
{old_growth_pine_taiga   , -4500,-1500,  3000, IMAX, -1899, IMAX,  IMIN, IMAX,  IMIN, IMAX,  -500, IMAX},
{windswept_forest        ,  IMIN, 2000,  1000, IMAX, -1899, IMAX,  4500, 5500,  IMIN, IMAX,  IMIN, IMAX},
{savanna                 ,  2000, 5500,  IMIN,-1000, -1900, IMAX,  IMIN, IMAX,  IMIN, IMAX,  IMIN, IMAX},
{savanna_plateau         ,  2000, 5500,  IMIN,-1000, -1100, IMAX,  IMIN,  500,  IMIN, IMAX,  IMIN, IMAX},
{badlands                ,  5500, IMAX,  IMIN, 1000, -1899, IMAX,  IMIN,  500,  IMIN, IMAX,  IMIN, IMAX},
{wooded_badlands         ,  5500, IMAX,  1000, IMAX, -1899, IMAX,  IMIN,  500,  IMIN, IMAX,  IMIN, IMAX},
{warm_ocean              ,  5500, IMAX,  IMIN, IMAX,-10500,-1900,  IMIN, IMAX,  IMIN, IMAX,  IMIN, IMAX},
{lukewarm_ocean          ,  2001, 5500,  IMIN, IMAX, -4550,-1900,  IMIN, IMAX,  IMIN, IMAX,  IMIN, IMAX},
{cold_ocean              , -4500,-1501,  IMIN, IMAX, -4550,-1900,  IMIN, IMAX,  IMIN, IMAX,  IMIN, IMAX},
{deep_lukewarm_ocean     ,  2001, 5500,  IMIN, IMAX,-10500,-4551,  IMIN, IMAX,  IMIN, IMAX,  IMIN, IMAX},
{deep_cold_ocean         , -4500,-1501,  IMIN, IMAX,-10500,-4551,  IMIN, IMAX,  IMIN, IMAX,  IMIN, IMAX},
{deep_frozen_ocean       ,  IMIN,-4501,  IMIN, IMAX,-10500,-4551,  IMIN, IMAX,  IMIN, IMAX,  IMIN, IMAX},
{sunflower_plains        , -1500, 2000,  IMIN,-3500, -1899, IMAX,  IMIN, IMAX,  IMIN, IMAX,  -500, IMAX},
{windswept_gravelly_hills,  IMIN,-1500,  IMIN,-1000, -1899, IMAX,  4500, 5500,  IMIN, IMAX,  IMIN, IMAX},
{flower_forest           , -1500, 2000,  IMIN,-3500, -1900, IMAX,  IMIN, IMAX,  IMIN, IMAX,  IMIN, -500},
{ice_spikes              ,  IMIN,-4500,  IMIN,-3500, -1899, IMAX,  IMIN, IMAX,  IMIN, IMAX,  -500, IMAX},
{old_growth_birch_forest , -1500, 2000,  1000, 3000, -1899, IMAX,  IMIN, IMAX,  IMIN, IMAX,  -500, IMAX},
{old_growth_spruce_taiga , -4500,-1500,  3000, IMAX, -1900, IMAX,  IMIN, IMAX,  IMIN, IMAX,  IMIN, -500},
{windswept_savanna       , -1500, IMAX,  IMIN, 3000, -1899,  300,  4500, 5500,  IMIN, IMAX,   501, IMAX},
{eroded_badlands         ,  5500, IMAX,  IMIN,-1000, -1899, IMAX,  IMIN,  500,  IMIN, IMAX,  IMIN, IMAX},
{bamboo_jungle           ,  2000, 5500,  3000, IMAX, -1899, IMAX,  IMIN, IMAX,  IMIN, IMAX,  -500, IMAX},
{dripstone_caves         ,  IMIN, IMAX,  IMIN, 6999,  3001, IMAX,  IMIN, IMAX,  1000, 9500,  IMIN, IMAX},
{lush_caves              ,  IMIN, IMAX,  2001, IMAX,  IMIN, IMAX,  IMIN, IMAX,  1000, 9500,  IMIN, IMAX},
{meadow                  , -4500, 2000,  IMIN, 3000,   300, IMAX, -7799,  500,  IMIN, IMAX,  IMIN, IMAX},
{grove                   ,  IMIN, 2000, -1000, IMAX, -1899, IMAX,  IMIN,-3750,  IMIN, IMAX,  IMIN, IMAX},
{snowy_slopes            ,  IMIN, 2000,  IMIN,-1000, -1899, IMAX,  IMIN,-3750,  IMIN, IMAX,  IMIN, IMAX},
{jagged_peaks            ,  IMIN, 2000,  IMIN, IMAX, -1899, IMAX,  IMIN,-3750,  IMIN, IMAX, -9333,-4001},
{frozen_peaks            ,  IMIN, 2000,  IMIN, IMAX, -1899, IMAX,  IMIN,-3750,  IMIN, IMAX,  4000, 9333},
{stony_peaks             ,  2000, 5500,  IMIN, IMAX, -1899, IMAX,  IMIN,-3750,  IMIN, IMAX, -9333, 9333},
{-1,0,0,0,0,0,0,0,0,0,0,0,0}};

static const int g_biome_para_range_19_diff[][13] = {
{eroded_badlands         ,  5500, IMAX,  IMIN,-1000, -1899, IMAX,  IMIN,  500,  IMIN, IMAX,  -500, IMAX},
{grove                   ,  IMIN, 2000, -1000, IMAX, -1899, IMAX,  IMIN,-3750,  IMIN,10499,  IMIN, IMAX},
{snowy_slopes            ,  IMIN, 2000,  IMIN,-1000, -1899, IMAX,  IMIN,-3750,  IMIN,10499,  IMIN, IMAX},
{jagged_peaks            ,  IMIN, 2000,  IMIN, IMAX, -1899, IMAX,  IMIN,-3750,  IMIN,10499, -9333,-4001},
{deep_dark               ,  IMIN, IMAX,  IMIN, IMAX,  IMIN, IMAX,  IMIN, 1818, 10500, IMAX,  IMIN, IMAX},
{mangrove_swamp          ,  2000, IMAX,  IMIN, IMAX, -1100, IMAX,  5500, IMAX,  IMIN, IMAX,  IMIN, IMAX},
{-1,0,0,0,0,0,0,0,0,0,0,0,0}};

static const int g_biome_para_range_20_diff[][13] = {
{swamp                   , -4500, 2000,  IMIN, IMAX, -1100, IMAX,  5500, IMAX,  IMIN, IMAX,  IMIN, IMAX},
{grove                   ,  IMIN, 2000, -1000, IMAX, -1899, IMAX,  IMIN,-3750,  IMIN,10500,  IMIN, IMAX},
{snowy_slopes            ,  IMIN, 2000,  IMIN,-1000, -1899, IMAX,  IMIN,-3750,  IMIN,10500,  IMIN, IMAX},
{jagged_peaks            ,  IMIN, 2000,  IMIN, IMAX, -1899, IMAX,  IMIN,-3750,  IMIN,10500, -9333,-4000},
{frozen_peaks            ,  IMIN, 2000,  IMIN, IMAX, -1899, IMAX,  IMIN,-3750,  IMIN,10500,  4000, 9333},
{stony_peaks             ,  2000, 5500,  IMIN, IMAX, -1899, IMAX,  IMIN,-3750,  IMIN,10500, -9333, 9333},
{cherry_grove            , -4500, 2000,  IMIN,-1000,   300, IMAX, -7799,  500,  IMIN, IMAX,  2666, IMAX},
{-1,0,0,0,0,0,0,0,0,0,0,0,0}};

static const int g_biome_para_range_21wd_diff[][13] = {
{pale_garden             , -1500, 2000,  3000, IMAX,   300, IMAX, -7799,  500,  IMIN, IMAX,  2666, IMAX},
{-1,0,0,0,0,0,0,0,0,0,0,0,0}};


/**
 * Gets the min/max parameter values within which a biome change can occur.
 */
const int *getBiomeParaExtremes(int mc)
{
    if (mc <= MC_B1_7)
    {
        static const int extremes_beta[] = {
            0, 10000,
            0, 10000,
            0,0, 0,0, 0,0, 0,0,
        };
        return extremes_beta;
    }
    if (mc <= MC_1_17)
        return nullptr;
    static const int extremes[] = {
        -4501, 5500,
        -3500, 6999,
        -10500, 300,
        -7799, 5500,
        1000, 10500, // depth has more dependencies
        -9333, 9333,
    };
    return extremes;
}

/**
 * Gets the min/max possible noise parameter values at which the given biome
 * can generate. The values are in min/max pairs in order:
 * temperature, humidity, continentalness, erosion, depth, weirdness.
 */
const int *getBiomeParaLimits(int mc, int id)
{
    if (mc <= MC_1_17)
        return nullptr;
    int i;
    if (mc > MC_1_21_3)
    {
        for (i = 0; g_biome_para_range_21wd_diff[i][0] != -1; i++)
        {
            if (g_biome_para_range_21wd_diff[i][0] == id)
                return &g_biome_para_range_21wd_diff[i][1];
        }
    }
    if (mc > MC_1_19)
    {
        for (i = 0; g_biome_para_range_20_diff[i][0] != -1; i++)
        {
            if (g_biome_para_range_20_diff[i][0] == id)
                return &g_biome_para_range_20_diff[i][1];
        }
    }
    if (mc > MC_1_18)
    {
        for (i = 0; g_biome_para_range_19_diff[i][0] != -1; i++)
        {
            if (g_biome_para_range_19_diff[i][0] == id)
                return &g_biome_para_range_19_diff[i][1];
        }
    }
    for (i = 0; g_biome_para_range_18[i][0] != -1; i++)
    {
        if (g_biome_para_range_18[i][0] == id)
            return &g_biome_para_range_18[i][1];
    }
    return nullptr;
}

/**
 * Determines which biomes are able to generate given climate parameter limits.
 * Possible biomes are marked non-zero in the 'ids'.
 */
void getPossibleBiomesForLimits(char ids[256], int mc, int limits[6][2])
{
    int i, j;
    std::fill_n(ids, static_cast<std::size_t>(256), '\0');

    for (i = 0; i < 256; i++)
    {
        if (!isOverworld(mc, i))
            continue;
        const int *bp = getBiomeParaLimits(mc, i);
        if (!bp)
            continue;

        for (j = 0; j < 6; j++)
        {
            if (limits[j][0] > bp[2*j+1] || limits[j][1] < bp[2*j+0])
                break;
        }
        if (j >= 6)
            ids[bp[-1]] = 1;
    }
}

int getLargestRec(int match, const int *ids, int sx, int sz, Pos *p0, Pos *p1)
{
    typedef struct { int n, j, w; } entry_t;
    std::vector<entry_t> meta(static_cast<std::size_t>(sx > sz ? sx : sz), entry_t{});
    int i, j, w, m, ret;
    ret = m = 0;

    for (i = sx-1; i >= 0; i--)
    {
        for (j = 0; j < sz; j++)
        {
            if (ids[j*sx + i] == match)
                meta[j].n++;
            else
                meta[j].n = 0;
        }
        for (w = j = 0; j < sz; j++)
        {
            int n = meta[j].n;
            if (n > w)
            {
                meta[static_cast<std::size_t>(m)].j = j;
                meta[static_cast<std::size_t>(m)].w = w;
                m++;
                w = n;
            }
            if (n == w)
                continue;
            do
            {
                entry_t e = meta[static_cast<std::size_t>(--m)];
                int area = w * (j - e.j);
                if (area > ret)
                {
                    p0->x = i; p0->z = e.j;
                    p1->x = i+w-1; p1->z = j-1;
                    ret = area;
                }
                w = e.w;
            }
            while (n < w);
            if ((w = n))
                m++;
        }
    }
    return ret;
}
