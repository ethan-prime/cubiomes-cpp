#include "biomes.hpp"

#include <array>
#include <cstdint>

namespace {

template <std::size_t N>
constexpr auto make_flag_table(std::initializer_list<int> ids) -> std::array<bool, N>
{
    std::array<bool, N> table{};
    for (const auto id : ids) {
        if (id >= 0 && static_cast<std::size_t>(id) < N) {
            table[static_cast<std::size_t>(id)] = true;
        }
    }
    return table;
}

constexpr auto has_id(const std::array<bool, 256> &table, int id) -> bool
{
    return id >= 0 && id < 256 && table[static_cast<std::size_t>(id)];
}

constexpr auto is_between(const int value, const int min_value, const int max_value) -> bool
{
    return value >= min_value && value <= max_value;
}

constexpr auto kModern18Biomes = make_flag_table<256>({
    ocean, plains, desert, mountains, forest, taiga, swamp, river, nether_wastes, the_end,
    frozen_ocean, frozen_river, snowy_tundra, mushroom_fields, beach, jungle, jungle_edge,
    deep_ocean, stone_shore, snowy_beach, birch_forest, dark_forest, snowy_taiga,
    giant_tree_taiga, wooded_mountains, savanna, savanna_plateau, badlands,
    wooded_badlands_plateau, warm_ocean, lukewarm_ocean, cold_ocean, deep_warm_ocean,
    deep_lukewarm_ocean, deep_cold_ocean, deep_frozen_ocean, sunflower_plains,
    gravelly_mountains, flower_forest, ice_spikes, tall_birch_forest,
    giant_spruce_taiga, shattered_savanna, eroded_badlands, bamboo_jungle,
    dripstone_caves, lush_caves, meadow, grove, snowy_slopes, stony_peaks,
    jagged_peaks, frozen_peaks, soul_sand_valley, crimson_forest, warped_forest,
    basalt_deltas, small_end_islands, end_midlands, end_highlands, end_barrens
});

constexpr auto kBeta17Biomes = make_flag_table<256>({
    plains, desert, forest, taiga, swamp, snowy_tundra, savanna, seasonal_forest,
    rainforest, shrubland, ocean, frozen_ocean
});

constexpr auto kPreB18Excluded = make_flag_table<256>({
    frozen_ocean, frozen_river, snowy_tundra, mushroom_fields, mushroom_field_shore, the_end
});

constexpr auto kPre10Excluded = make_flag_table<256>({
    snowy_mountains, beach, desert_hills, wooded_hills, taiga_hills, mountain_edge
});

constexpr auto kMutatedSince17 = make_flag_table<256>({
    sunflower_plains, desert_lakes, gravelly_mountains, flower_forest, taiga_mountains,
    swamp_hills, ice_spikes, modified_jungle, modified_jungle_edge, tall_birch_forest,
    tall_birch_hills, dark_forest_hills, snowy_taiga_mountains, giant_spruce_taiga,
    giant_spruce_taiga_hills, modified_gravelly_mountains, shattered_savanna,
    shattered_savanna_plateau, eroded_badlands, modified_wooded_badlands_plateau,
    modified_badlands_plateau
});

constexpr auto kMesaBiomes = make_flag_table<256>({
    badlands, eroded_badlands, modified_wooded_badlands_plateau, modified_badlands_plateau,
    wooded_badlands_plateau, badlands_plateau
});

constexpr auto kSnowyBiomes = make_flag_table<256>({
    frozen_ocean, frozen_river, snowy_tundra, snowy_mountains, snowy_beach, snowy_taiga,
    snowy_taiga_hills, ice_spikes, snowy_taiga_mountains
});

constexpr std::uint64_t kShallowOceanBits =
    (1ULL << ocean) |
    (1ULL << frozen_ocean) |
    (1ULL << warm_ocean) |
    (1ULL << lukewarm_ocean) |
    (1ULL << cold_ocean);

constexpr std::uint64_t kDeepOceanBits =
    (1ULL << deep_ocean) |
    (1ULL << deep_warm_ocean) |
    (1ULL << deep_lukewarm_ocean) |
    (1ULL << deep_cold_ocean) |
    (1ULL << deep_frozen_ocean);

constexpr std::uint64_t kOceanBits = kShallowOceanBits | kDeepOceanBits;

constexpr auto has_legacy_ocean_bit(const int id, const std::uint64_t mask) -> bool
{
    return static_cast<std::uint32_t>(id) < 64U && ((1ULL << id) & mask) != 0;
}

constexpr auto kMutatedByBase = [] {
    std::array<int, 256> table{};
    table.fill(none);
    table[plains] = sunflower_plains;
    table[desert] = desert_lakes;
    table[mountains] = gravelly_mountains;
    table[forest] = flower_forest;
    table[taiga] = taiga_mountains;
    table[swamp] = swamp_hills;
    table[snowy_tundra] = ice_spikes;
    table[jungle] = modified_jungle;
    table[jungle_edge] = modified_jungle_edge;
    table[dark_forest] = dark_forest_hills;
    table[snowy_taiga] = snowy_taiga_mountains;
    table[giant_tree_taiga] = giant_spruce_taiga;
    table[giant_tree_taiga_hills] = giant_spruce_taiga_hills;
    table[wooded_mountains] = modified_gravelly_mountains;
    table[savanna] = shattered_savanna;
    table[savanna_plateau] = shattered_savanna_plateau;
    table[badlands] = eroded_badlands;
    table[wooded_badlands_plateau] = modified_wooded_badlands_plateau;
    table[badlands_plateau] = modified_badlands_plateau;
    return table;
}();

constexpr auto kCategoryByBiome = [] {
    std::array<int, 256> table{};
    table.fill(none);

    const auto set = [&table](std::initializer_list<int> ids, const int category) {
        for (const auto id : ids) {
            if (id >= 0 && id < 256) {
                table[static_cast<std::size_t>(id)] = category;
            }
        }
    };

    set({beach, snowy_beach}, beach);
    set({desert, desert_hills, desert_lakes}, desert);
    set({mountains, mountain_edge, wooded_mountains, gravelly_mountains, modified_gravelly_mountains}, mountains);
    set({forest, wooded_hills, birch_forest, birch_forest_hills, dark_forest, flower_forest, tall_birch_forest, tall_birch_hills, dark_forest_hills}, forest);
    set({snowy_tundra, snowy_mountains, ice_spikes}, snowy_tundra);
    set({jungle, jungle_hills, jungle_edge, modified_jungle, modified_jungle_edge, bamboo_jungle, bamboo_jungle_hills}, jungle);
    set({badlands, eroded_badlands, modified_wooded_badlands_plateau, modified_badlands_plateau}, mesa);
    set({wooded_badlands_plateau, badlands_plateau}, badlands_plateau);
    set({mushroom_fields, mushroom_field_shore}, mushroom_fields);
    set({stone_shore}, stone_shore);
    set({ocean, frozen_ocean, deep_ocean, warm_ocean, lukewarm_ocean, cold_ocean, deep_warm_ocean, deep_lukewarm_ocean, deep_cold_ocean, deep_frozen_ocean}, ocean);
    set({plains, sunflower_plains}, plains);
    set({river, frozen_river}, river);
    set({savanna, savanna_plateau, shattered_savanna, shattered_savanna_plateau}, savanna);
    set({swamp, swamp_hills}, swamp);
    set({taiga, taiga_hills, snowy_taiga, snowy_taiga_hills, giant_tree_taiga, giant_tree_taiga_hills, taiga_mountains, snowy_taiga_mountains, giant_spruce_taiga, giant_spruce_taiga_hills}, taiga);
    set({nether_wastes, soul_sand_valley, crimson_forest, warped_forest, basalt_deltas}, nether_wastes);
    return table;
}();

} // namespace


int biomeExists(int mc, int id)
{
    if (mc >= MC_1_18)
    {
        if (has_id(kModern18Biomes, id))
            return 1;

        if (id == pale_garden)
            return mc >= MC_1_21_WD;

        if (id == cherry_grove)
            return mc >= MC_1_20;

        if (id == deep_dark || id == mangrove_swamp)
            return mc >= MC_1_19_2;

        return 0;
    }

    if (mc <= MC_B1_7)
        return has_id(kBeta17Biomes, id);

    if (mc <= MC_B1_8)
        if (has_id(kPreB18Excluded, id))
            return 0;

    if (mc <= MC_1_0)
        if (has_id(kPre10Excluded, id))
            return 0;

    if (is_between(id, ocean, mountain_edge)) return 1;
    if (is_between(id, jungle, jungle_hills)) return mc >= MC_1_2;
    if (is_between(id, jungle_edge, badlands_plateau)) return mc >= MC_1_7;
    if (is_between(id, small_end_islands, end_barrens)) return mc >= MC_1_9;
    if (is_between(id, warm_ocean, deep_frozen_ocean)) return mc >= MC_1_13;

    if (id == the_void)
        return mc >= MC_1_9;
    if (has_id(kMutatedSince17, id))
        return mc >= MC_1_7;
    if (id == bamboo_jungle || id == bamboo_jungle_hills)
        return mc >= MC_1_14;
    if (is_between(id, soul_sand_valley, basalt_deltas))
        return mc >= MC_1_16_1;
    if (id == dripstone_caves || id == lush_caves)
        return mc >= MC_1_17;
    return 0;
}

int isOverworld(int mc, int id)
{
    if (!biomeExists(mc, id))
        return 0;

    if (id >= small_end_islands && id <= end_barrens) return 0;
    if (id >= soul_sand_valley && id <= basalt_deltas) return 0;

    switch (id)
    {
    case nether_wastes:
    case the_end:
        return 0;
    case frozen_ocean:
        return mc <= MC_1_6 || mc >= MC_1_13;
    case mountain_edge:
        return mc <= MC_1_6;
    case deep_warm_ocean:
    case the_void:
        return 0;
    case tall_birch_forest:
        return mc <= MC_1_8 || mc >= MC_1_11;
    case dripstone_caves:
    case lush_caves:
        return mc >= MC_1_18;
    }
    return 1;
}

int getDimension(int id)
{
    if (id >= small_end_islands && id <= end_barrens) return DIM_END;
    if (id >= soul_sand_valley && id <= basalt_deltas) return DIM_NETHER;
    if (id == the_end) return DIM_END;
    if (id == nether_wastes) return DIM_NETHER;
    return DIM_OVERWORLD;
}

int getMutated(int mc, int id)
{
    if (id == birch_forest) {
        // emulate MC-98995
        return (mc >= MC_1_9 && mc <= MC_1_10) ? tall_birch_hills : tall_birch_forest;
    }
    if (id == birch_forest_hills)
        return (mc >= MC_1_9 && mc <= MC_1_10) ? none : tall_birch_hills;
    if (id < 0 || id >= 256)
        return none;
    return kMutatedByBase[static_cast<std::size_t>(id)];
}

int getCategory(int mc, int id)
{
    if (id == wooded_badlands_plateau || id == badlands_plateau)
        return mc <= MC_1_15 ? mesa : badlands_plateau;
    if (id < 0 || id >= 256)
        return none;
    return kCategoryByBiome[static_cast<std::size_t>(id)];
}

int areSimilar(int mc, int id1, int id2)
{
    if (id1 == id2) return 1;

    if (mc <= MC_1_15)
    {
        if (id1 == wooded_badlands_plateau || id1 == badlands_plateau)
            return id2 == wooded_badlands_plateau || id2 == badlands_plateau;
    }

    return getCategory(mc, id1) == getCategory(mc, id2);
}

int isMesa(int id)
{
    return has_id(kMesaBiomes, id);
}

int isShallowOcean(int id)
{
    return has_legacy_ocean_bit(id, kShallowOceanBits);
}

int isDeepOcean(int id)
{
    return has_legacy_ocean_bit(id, kDeepOceanBits);
}

int isOceanic(int id)
{
    return has_legacy_ocean_bit(id, kOceanBits);
}

int isSnowy(int id)
{
    return has_id(kSnowyBiomes, id);
}

