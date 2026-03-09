#pragma once

#include <cstdint>

/* Minecraft versions */
enum MCVersion
{   // MC_1_X refers to the latest patch of the respective 1.X release.
    // NOTE: Development effort focuses on just the newest patch for each major
    // release. Minor releases and major versions <= 1.0 are experimental.
    MC_UNDEF,
    MC_B1_7,
    MC_B1_8,
    MC_1_0_0,  MC_1_0  = MC_1_0_0,
    MC_1_1_0,  MC_1_1  = MC_1_1_0,
    MC_1_2_5,  MC_1_2  = MC_1_2_5,
    MC_1_3_2,  MC_1_3  = MC_1_3_2,
    MC_1_4_7,  MC_1_4  = MC_1_4_7,
    MC_1_5_2,  MC_1_5  = MC_1_5_2,
    MC_1_6_4,  MC_1_6  = MC_1_6_4,
    MC_1_7_10, MC_1_7  = MC_1_7_10,
    MC_1_8_9,  MC_1_8  = MC_1_8_9,
    MC_1_9_4,  MC_1_9  = MC_1_9_4,
    MC_1_10_2, MC_1_10 = MC_1_10_2,
    MC_1_11_2, MC_1_11 = MC_1_11_2,
    MC_1_12_2, MC_1_12 = MC_1_12_2,
    MC_1_13_2, MC_1_13 = MC_1_13_2,
    MC_1_14_4, MC_1_14 = MC_1_14_4,
    MC_1_15_2, MC_1_15 = MC_1_15_2,
    MC_1_16_1,
    MC_1_16_5, MC_1_16 = MC_1_16_5,
    MC_1_17_1, MC_1_17 = MC_1_17_1,
    MC_1_18_2, MC_1_18 = MC_1_18_2,
    MC_1_19_2,
    MC_1_19_4, MC_1_19 = MC_1_19_4,
    MC_1_20_6, MC_1_20 = MC_1_20_6,
    MC_1_21_1,
    MC_1_21_3,
    MC_1_21_WD, // Winter Drop, version TBA
    MC_1_21 = MC_1_21_WD,
    MC_NEWEST = MC_1_21,
};

enum Dimension
{
    DIM_NETHER      =   -1,
    DIM_OVERWORLD   =    0,
    DIM_END         =   +1,
    DIM_UNDEF       = 1000,
};

enum BiomeID
{
    none = -1,
    // 0
    ocean = 0,
    plains,
    desert,
    mountains,                  extremeHills = mountains,
    forest,
    taiga,
    swamp,                      swampland = swamp,
    river,
    nether_wastes,              hell = nether_wastes,
    the_end,                    sky = the_end,
    // 10
    frozen_ocean,               frozenOcean = frozen_ocean,
    frozen_river,               frozenRiver = frozen_river,
    snowy_tundra,               icePlains = snowy_tundra,
    snowy_mountains,            iceMountains = snowy_mountains,
    mushroom_fields,            mushroomIsland = mushroom_fields,
    mushroom_field_shore,       mushroomIslandShore = mushroom_field_shore,
    beach,
    desert_hills,               desertHills = desert_hills,
    wooded_hills,               forestHills = wooded_hills,
    taiga_hills,                taigaHills = taiga_hills,
    // 20
    mountain_edge,              extremeHillsEdge = mountain_edge,
    jungle,
    jungle_hills,               jungleHills = jungle_hills,
    jungle_edge,                jungleEdge = jungle_edge,
    deep_ocean,                 deepOcean = deep_ocean,
    stone_shore,                stoneBeach = stone_shore,
    snowy_beach,                coldBeach = snowy_beach,
    birch_forest,               birchForest = birch_forest,
    birch_forest_hills,         birchForestHills = birch_forest_hills,
    dark_forest,                roofedForest = dark_forest,
    // 30
    snowy_taiga,                coldTaiga = snowy_taiga,
    snowy_taiga_hills,          coldTaigaHills = snowy_taiga_hills,
    giant_tree_taiga,           megaTaiga = giant_tree_taiga,
    giant_tree_taiga_hills,     megaTaigaHills = giant_tree_taiga_hills,
    wooded_mountains,           extremeHillsPlus = wooded_mountains,
    savanna,
    savanna_plateau,            savannaPlateau = savanna_plateau,
    badlands,                   mesa = badlands,
    wooded_badlands_plateau,    mesaPlateau_F = wooded_badlands_plateau,
    badlands_plateau,           mesaPlateau = badlands_plateau,
    // 40  --  1.13
    small_end_islands,
    end_midlands,
    end_highlands,
    end_barrens,
    warm_ocean,                 warmOcean = warm_ocean,
    lukewarm_ocean,             lukewarmOcean = lukewarm_ocean,
    cold_ocean,                 coldOcean = cold_ocean,
    deep_warm_ocean,            warmDeepOcean = deep_warm_ocean,
    deep_lukewarm_ocean,        lukewarmDeepOcean = deep_lukewarm_ocean,
    deep_cold_ocean,            coldDeepOcean = deep_cold_ocean,
    // 50
    deep_frozen_ocean,          frozenDeepOcean = deep_frozen_ocean,
    // Alpha 1.2 - Beta 1.7
    seasonal_forest,
    rainforest,
    shrubland,


    the_void = 127,

    // mutated variants
    sunflower_plains                = plains+128,
    desert_lakes                    = desert+128,
    gravelly_mountains              = mountains+128,
    flower_forest                   = forest+128,
    taiga_mountains                 = taiga+128,
    swamp_hills                     = swamp+128,
    ice_spikes                      = snowy_tundra+128,
    modified_jungle                 = jungle+128,
    modified_jungle_edge            = jungle_edge+128,
    tall_birch_forest               = birch_forest+128,
    tall_birch_hills                = birch_forest_hills+128,
    dark_forest_hills               = dark_forest+128,
    snowy_taiga_mountains           = snowy_taiga+128,
    giant_spruce_taiga              = giant_tree_taiga+128,
    giant_spruce_taiga_hills        = giant_tree_taiga_hills+128,
    modified_gravelly_mountains     = wooded_mountains+128,
    shattered_savanna               = savanna+128,
    shattered_savanna_plateau       = savanna_plateau+128,
    eroded_badlands                 = badlands+128,
    modified_wooded_badlands_plateau = wooded_badlands_plateau+128,
    modified_badlands_plateau       = badlands_plateau+128,
    // 1.14
    bamboo_jungle                   = 168,
    bamboo_jungle_hills             = 169,
    // 1.16
    soul_sand_valley                = 170,
    crimson_forest                  = 171,
    warped_forest                   = 172,
    basalt_deltas                   = 173,
    // 1.17
    dripstone_caves                 = 174,
    lush_caves                      = 175,
    // 1.18
    meadow                          = 177,
    grove                           = 178,
    snowy_slopes                    = 179,
    jagged_peaks                    = 180,
    frozen_peaks                    = 181,
    stony_peaks                     = 182,
    old_growth_birch_forest         = tall_birch_forest,
    old_growth_pine_taiga           = giant_tree_taiga,
    old_growth_spruce_taiga         = giant_spruce_taiga,
    snowy_plains                    = snowy_tundra,
    sparse_jungle                   = jungle_edge,
    stony_shore                     = stone_shore,
    windswept_hills                 = mountains,
    windswept_forest                = wooded_mountains,
    windswept_gravelly_hills        = gravelly_mountains,
    windswept_savanna               = shattered_savanna,
    wooded_badlands                 = wooded_badlands_plateau,
    // 1.19
    deep_dark                       = 183,
    mangrove_swamp                  = 184,
    // 1.20
    cherry_grove                    = 185,
    // 1.21 Winter Drop
    pale_garden                     = 186,
};


//==============================================================================
// BiomeID Helper Functions
//==============================================================================

int biomeExists(int mc, int id);
int isOverworld(int mc, int id); // false for biomes that don't generate
int getDimension(int id);
int getMutated(int mc, int id);
int getCategory(int mc, int id);
int areSimilar(int mc, int id1, int id2);
int isMesa(int id);
int isShallowOcean(int id);
int isDeepOcean(int id);
int isOceanic(int id);
int isSnowy(int id);

namespace cubiomes::cpp {

enum class Version : std::int32_t {
    Undef = MC_UNDEF,
    B1_7 = MC_B1_7,
    B1_8 = MC_B1_8,
    V1_0 = MC_1_0,
    V1_1 = MC_1_1,
    V1_2 = MC_1_2,
    V1_3 = MC_1_3,
    V1_4 = MC_1_4,
    V1_5 = MC_1_5,
    V1_6 = MC_1_6,
    V1_7 = MC_1_7,
    V1_8 = MC_1_8,
    V1_9 = MC_1_9,
    V1_10 = MC_1_10,
    V1_11 = MC_1_11,
    V1_12 = MC_1_12,
    V1_13 = MC_1_13,
    V1_14 = MC_1_14,
    V1_15 = MC_1_15,
    V1_16_1 = MC_1_16_1,
    V1_16 = MC_1_16,
    V1_17 = MC_1_17,
    V1_18 = MC_1_18,
    V1_19_2 = MC_1_19_2,
    V1_19 = MC_1_19,
    V1_20 = MC_1_20,
    V1_21_1 = MC_1_21_1,
    V1_21_3 = MC_1_21_3,
    V1_21 = MC_1_21,
};

enum class Biome : std::int32_t {
    None = none,
    Ocean = ocean,
    Plains = plains,
    Desert = desert,
    Mountains = mountains,
    Forest = forest,
    Taiga = taiga,
    Swamp = swamp,
    River = river,
    NetherWastes = nether_wastes,
    TheEnd = the_end,
    FrozenOcean = frozen_ocean,
    FrozenRiver = frozen_river,
    SnowyTundra = snowy_tundra,
    SnowyMountains = snowy_mountains,
    MushroomFields = mushroom_fields,
    MushroomFieldShore = mushroom_field_shore,
    Beach = beach,
    DesertHills = desert_hills,
    WoodedHills = wooded_hills,
    TaigaHills = taiga_hills,
    MountainEdge = mountain_edge,
    Jungle = jungle,
    JungleHills = jungle_hills,
    JungleEdge = jungle_edge,
    DeepOcean = deep_ocean,
    StoneShore = stone_shore,
    SnowyBeach = snowy_beach,
    BirchForest = birch_forest,
    BirchForestHills = birch_forest_hills,
    DarkForest = dark_forest,
    SnowyTaiga = snowy_taiga,
    SnowyTaigaHills = snowy_taiga_hills,
    GiantTreeTaiga = giant_tree_taiga,
    GiantTreeTaigaHills = giant_tree_taiga_hills,
    WoodedMountains = wooded_mountains,
    Savanna = savanna,
    SavannaPlateau = savanna_plateau,
    Badlands = badlands,
    WoodedBadlandsPlateau = wooded_badlands_plateau,
    BadlandsPlateau = badlands_plateau,
    SmallEndIslands = small_end_islands,
    EndMidlands = end_midlands,
    EndHighlands = end_highlands,
    EndBarrens = end_barrens,
    WarmOcean = warm_ocean,
    LukewarmOcean = lukewarm_ocean,
    ColdOcean = cold_ocean,
    DeepWarmOcean = deep_warm_ocean,
    DeepLukewarmOcean = deep_lukewarm_ocean,
    DeepColdOcean = deep_cold_ocean,
    DeepFrozenOcean = deep_frozen_ocean,
    SeasonalForest = seasonal_forest,
    Rainforest = rainforest,
    Shrubland = shrubland,
    TheVoid = the_void,
    SunflowerPlains = sunflower_plains,
    DesertLakes = desert_lakes,
    GravellyMountains = gravelly_mountains,
    FlowerForest = flower_forest,
    TaigaMountains = taiga_mountains,
    SwampHills = swamp_hills,
    IceSpikes = ice_spikes,
    ModifiedJungle = modified_jungle,
    ModifiedJungleEdge = modified_jungle_edge,
    TallBirchForest = tall_birch_forest,
    TallBirchHills = tall_birch_hills,
    DarkForestHills = dark_forest_hills,
    SnowyTaigaMountains = snowy_taiga_mountains,
    GiantSpruceTaiga = giant_spruce_taiga,
    GiantSpruceTaigaHills = giant_spruce_taiga_hills,
    ModifiedGravellyMountains = modified_gravelly_mountains,
    ShatteredSavanna = shattered_savanna,
    ShatteredSavannaPlateau = shattered_savanna_plateau,
    ErodedBadlands = eroded_badlands,
    ModifiedWoodedBadlandsPlateau = modified_wooded_badlands_plateau,
    ModifiedBadlandsPlateau = modified_badlands_plateau,
    BambooJungle = bamboo_jungle,
    BambooJungleHills = bamboo_jungle_hills,
    SoulSandValley = soul_sand_valley,
    CrimsonForest = crimson_forest,
    WarpedForest = warped_forest,
    BasaltDeltas = basalt_deltas,
    DripstoneCaves = dripstone_caves,
    LushCaves = lush_caves,
    Meadow = meadow,
    Grove = grove,
    SnowySlopes = snowy_slopes,
    JaggedPeaks = jagged_peaks,
    FrozenPeaks = frozen_peaks,
    StonyPeaks = stony_peaks,
    DeepDark = deep_dark,
    MangroveSwamp = mangrove_swamp,
    CherryGrove = cherry_grove,
    PaleGarden = pale_garden,
};

constexpr auto to_raw(Version version) -> int { return static_cast<int>(version); }
constexpr auto to_raw(Biome biome) -> int { return static_cast<int>(biome); }

static_assert(to_raw(Version::V1_20) == MC_1_20);
static_assert(to_raw(Biome::Plains) == plains);
static_assert(to_raw(Biome::NetherWastes) == nether_wastes);
static_assert(to_raw(Biome::TheEnd) == the_end);

enum class DimensionKind : std::int32_t {
    Nether = DIM_NETHER,
    Overworld = DIM_OVERWORLD,
    End = DIM_END,
    Undefined = DIM_UNDEF,
};

constexpr auto to_dimension_kind(const int raw) -> DimensionKind { return static_cast<DimensionKind>(raw); }

inline auto biome_exists(int mc, int id) -> bool { return biomeExists(mc, id) != 0; }
inline auto is_overworld(int mc, int id) -> bool { return isOverworld(mc, id) != 0; }
inline auto dimension_of(int id) -> int { return getDimension(id); }
inline auto mutated_biome(int mc, int id) -> int { return getMutated(mc, id); }
inline auto biome_category(int mc, int id) -> int { return getCategory(mc, id); }
inline auto are_similar_biomes(int mc, int id1, int id2) -> bool { return areSimilar(mc, id1, id2) != 0; }
inline auto is_mesa_biome(int id) -> bool { return isMesa(id) != 0; }
inline auto is_shallow_ocean_biome(int id) -> bool { return isShallowOcean(id) != 0; }
inline auto is_deep_ocean_biome(int id) -> bool { return isDeepOcean(id) != 0; }
inline auto is_oceanic_biome(int id) -> bool { return isOceanic(id) != 0; }
inline auto is_snowy_biome(int id) -> bool { return isSnowy(id) != 0; }
inline auto biome_exists(Version version, Biome biome) -> bool { return biome_exists(to_raw(version), to_raw(biome)); }
inline auto is_overworld(Version version, Biome biome) -> bool { return is_overworld(to_raw(version), to_raw(biome)); }
inline auto dimension_kind(Biome biome) -> DimensionKind { return to_dimension_kind(dimension_of(to_raw(biome))); }
inline auto mutated_biome(Version version, Biome biome) -> Biome { return static_cast<Biome>(mutated_biome(to_raw(version), to_raw(biome))); }
inline auto biome_category(Version version, Biome biome) -> Biome { return static_cast<Biome>(biome_category(to_raw(version), to_raw(biome))); }
inline auto are_similar_biomes(Version version, Biome biome_a, Biome biome_b) -> bool {
    return are_similar_biomes(to_raw(version), to_raw(biome_a), to_raw(biome_b));
}
inline auto is_mesa_biome(Biome biome) -> bool { return is_mesa_biome(to_raw(biome)); }
inline auto is_shallow_ocean_biome(Biome biome) -> bool { return is_shallow_ocean_biome(to_raw(biome)); }
inline auto is_deep_ocean_biome(Biome biome) -> bool { return is_deep_ocean_biome(to_raw(biome)); }
inline auto is_oceanic_biome(Biome biome) -> bool { return is_oceanic_biome(to_raw(biome)); }
inline auto is_snowy_biome(Biome biome) -> bool { return is_snowy_biome(to_raw(biome)); }

} // namespace cubiomes::cpp
