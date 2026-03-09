#include "util.hpp"
#include "finders.hpp"

#include <algorithm>
#include <array>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ranges>
#include <string_view>
#include <utility>


namespace cubiomes::detail {

using McPair = std::pair<int, const char *>;
using StrPair = std::pair<std::string_view, int>;

constexpr auto kMcToStr = std::array{
    McPair{MC_B1_7, "Beta 1.7"},
    McPair{MC_B1_8, "Beta 1.8"},
    McPair{MC_1_0, "1.0"},
    McPair{MC_1_1, "1.1"},
    McPair{MC_1_2, "1.2"},
    McPair{MC_1_3, "1.3"},
    McPair{MC_1_4, "1.4"},
    McPair{MC_1_5, "1.5"},
    McPair{MC_1_6, "1.6"},
    McPair{MC_1_7, "1.7"},
    McPair{MC_1_8, "1.8"},
    McPair{MC_1_9, "1.9"},
    McPair{MC_1_10, "1.10"},
    McPair{MC_1_11, "1.11"},
    McPair{MC_1_12, "1.12"},
    McPair{MC_1_13, "1.13"},
    McPair{MC_1_14, "1.14"},
    McPair{MC_1_15, "1.15"},
    McPair{MC_1_16_1, "1.16.1"},
    McPair{MC_1_16, "1.16"},
    McPair{MC_1_17, "1.17"},
    McPair{MC_1_18, "1.18"},
    McPair{MC_1_19_2, "1.19.2"},
    McPair{MC_1_19, "1.19"},
    McPair{MC_1_20, "1.20"},
    McPair{MC_1_21_1, "1.21.1"},
    McPair{MC_1_21_3, "1.21.3"},
    McPair{MC_1_21_WD, "1.21 WD"},
};

constexpr auto kStrToMc = std::array{
    StrPair{"1.21", MC_1_21},
    StrPair{"1.21 WD", MC_1_21_WD},
    StrPair{"1.21.3", MC_1_21_3},
    StrPair{"1.21.2", MC_1_21_3}, // backwards compatibility
    StrPair{"1.21.1", MC_1_21_1},
    StrPair{"1.20", MC_1_20},
    StrPair{"1.20.6", MC_1_20_6},
    StrPair{"1.19", MC_1_19},
    StrPair{"1.19.4", MC_1_19_4},
    StrPair{"1.19.2", MC_1_19_2},
    StrPair{"1.18", MC_1_18},
    StrPair{"1.18.2", MC_1_18_2},
    StrPair{"1.17", MC_1_17},
    StrPair{"1.17.1", MC_1_17_1},
    StrPair{"1.16", MC_1_16},
    StrPair{"1.16.5", MC_1_16_5},
    StrPair{"1.16.1", MC_1_16_1},
    StrPair{"1.15", MC_1_15},
    StrPair{"1.15.2", MC_1_15_2},
    StrPair{"1.14", MC_1_14},
    StrPair{"1.14.4", MC_1_14_4},
    StrPair{"1.13", MC_1_13},
    StrPair{"1.13.2", MC_1_13_2},
    StrPair{"1.12", MC_1_12},
    StrPair{"1.12.2", MC_1_12_2},
    StrPair{"1.11", MC_1_11},
    StrPair{"1.11.2", MC_1_11_2},
    StrPair{"1.10", MC_1_10},
    StrPair{"1.10.2", MC_1_10_2},
    StrPair{"1.9", MC_1_9},
    StrPair{"1.9.4", MC_1_9_4},
    StrPair{"1.8", MC_1_8},
    StrPair{"1.8.9", MC_1_8_9},
    StrPair{"1.7", MC_1_7},
    StrPair{"1.7.10", MC_1_7_10},
    StrPair{"1.6", MC_1_6},
    StrPair{"1.6.4", MC_1_6_4},
    StrPair{"1.5", MC_1_5},
    StrPair{"1.5.2", MC_1_5_2},
    StrPair{"1.4", MC_1_4},
    StrPair{"1.4.7", MC_1_4_7},
    StrPair{"1.3", MC_1_3},
    StrPair{"1.3.2", MC_1_3_2},
    StrPair{"1.2", MC_1_2},
    StrPair{"1.2.5", MC_1_2_5},
    StrPair{"1.1", MC_1_1},
    StrPair{"1.1.0", MC_1_1_0},
    StrPair{"1.0", MC_1_0},
    StrPair{"1.0.0", MC_1_0_0},
    StrPair{"Beta 1.8", MC_B1_8},
    StrPair{"Beta 1.7", MC_B1_7},
};

auto set_color(unsigned char colors[256][3], int id, uint32_t hex) -> void;

} // namespace cubiomes::detail

uint64_t *loadSavedSeeds(const char *fnam, uint64_t *scnt)
{
    FILE *fp = fopen(fnam, "r");

    uint64_t seed, i;
    uint64_t *baseSeeds;

    if (fp == NULL)
        return NULL;

    *scnt = 0;

    while (!feof(fp))
    {
        if (fscanf(fp, "%" PRId64, (int64_t*)&seed) == 1) (*scnt)++;
        else while (!feof(fp) && fgetc(fp) != '\n');
    }

    if (*scnt == 0)
        return NULL;

    baseSeeds = (uint64_t*) calloc(*scnt, sizeof(*baseSeeds));

    rewind(fp);

    for (i = 0; i < *scnt && !feof(fp);)
    {
        if (fscanf(fp, "%" PRId64, (int64_t*)&baseSeeds[i]) == 1) i++;
        else while (!feof(fp) && fgetc(fp) != '\n');
    }

    fclose(fp);

    return baseSeeds;
}


const char* mc2str(int mc)
{
    if (const auto it = std::ranges::find(cubiomes::detail::kMcToStr, mc, &cubiomes::detail::McPair::first);
        it != cubiomes::detail::kMcToStr.end()) {
        return it->second;
    }
    return "?";
}

int str2mc(const char *s)
{
    if (s == nullptr) {
        return MC_UNDEF;
    }
    const std::string_view query{s};
    if (const auto it = std::ranges::find(cubiomes::detail::kStrToMc, query, &cubiomes::detail::StrPair::first);
        it != cubiomes::detail::kStrToMc.end()) {
        return it->second;
    }
    return MC_UNDEF;
}


const char *biome2str(int mc, int id)
{
    if (mc >= MC_1_18)
    {
        // a bunch of 'new' biomes in 1.18 actually just got renamed
        // (based on their features and biome id conversion when upgrading)
        switch (id)
        {
        case old_growth_birch_forest: return "old_growth_birch_forest";
        case old_growth_pine_taiga: return "old_growth_pine_taiga";
        case old_growth_spruce_taiga: return "old_growth_spruce_taiga";
        case snowy_plains: return "snowy_plains";
        case sparse_jungle: return "sparse_jungle";
        case stony_shore: return "stony_shore";
        case windswept_hills: return "windswept_hills";
        case windswept_forest: return "windswept_forest";
        case windswept_gravelly_hills: return "windswept_gravelly_hills";
        case windswept_savanna: return "windswept_savanna";
        case wooded_badlands: return "wooded_badlands";
        }
    }

    switch (id)
    {
    case ocean: return "ocean";
    case plains: return "plains";
    case desert: return "desert";
    case mountains: return "mountains";
    case forest: return "forest";
    case taiga: return "taiga";
    case swamp: return "swamp";
    case river: return "river";
    case nether_wastes: return "nether_wastes";
    case the_end: return "the_end";
    // 10
    case frozen_ocean: return "frozen_ocean";
    case frozen_river: return "frozen_river";
    case snowy_tundra: return "snowy_tundra";
    case snowy_mountains: return "snowy_mountains";
    case mushroom_fields: return "mushroom_fields";
    case mushroom_field_shore: return "mushroom_field_shore";
    case beach: return "beach";
    case desert_hills: return "desert_hills";
    case wooded_hills: return "wooded_hills";
    case taiga_hills: return "taiga_hills";
    // 20
    case mountain_edge: return "mountain_edge";
    case jungle: return "jungle";
    case jungle_hills: return "jungle_hills";
    case jungle_edge: return "jungle_edge";
    case deep_ocean: return "deep_ocean";
    case stone_shore: return "stone_shore";
    case snowy_beach: return "snowy_beach";
    case birch_forest: return "birch_forest";
    case birch_forest_hills: return "birch_forest_hills";
    case dark_forest: return "dark_forest";
    // 30
    case snowy_taiga: return "snowy_taiga";
    case snowy_taiga_hills: return "snowy_taiga_hills";
    case giant_tree_taiga: return "giant_tree_taiga";
    case giant_tree_taiga_hills: return "giant_tree_taiga_hills";
    case wooded_mountains: return "wooded_mountains";
    case savanna: return "savanna";
    case savanna_plateau: return "savanna_plateau";
    case badlands: return "badlands";
    case wooded_badlands_plateau: return "wooded_badlands_plateau";
    case badlands_plateau: return "badlands_plateau";
    // 40  --  1.13
    case small_end_islands: return "small_end_islands";
    case end_midlands: return "end_midlands";
    case end_highlands: return "end_highlands";
    case end_barrens: return "end_barrens";
    case warm_ocean: return "warm_ocean";
    case lukewarm_ocean: return "lukewarm_ocean";
    case cold_ocean: return "cold_ocean";
    case deep_warm_ocean: return "deep_warm_ocean";
    case deep_lukewarm_ocean: return "deep_lukewarm_ocean";
    case deep_cold_ocean: return "deep_cold_ocean";
    // 50
    case deep_frozen_ocean: return "deep_frozen_ocean";
    // Alpha 1.2 - Beta 1.7
    case seasonal_forest: return "seasonal_forest";
    case shrubland: return "shrubland";
    case rainforest: return "rainforest";

    case the_void: return "the_void";

    // mutated variants
    case sunflower_plains: return "sunflower_plains";
    case desert_lakes: return "desert_lakes";
    case gravelly_mountains: return "gravelly_mountains";
    case flower_forest: return "flower_forest";
    case taiga_mountains: return "taiga_mountains";
    case swamp_hills: return "swamp_hills";
    case ice_spikes: return "ice_spikes";
    case modified_jungle: return "modified_jungle";
    case modified_jungle_edge: return "modified_jungle_edge";
    case tall_birch_forest: return "tall_birch_forest";
    case tall_birch_hills: return "tall_birch_hills";
    case dark_forest_hills: return "dark_forest_hills";
    case snowy_taiga_mountains: return "snowy_taiga_mountains";
    case giant_spruce_taiga: return "giant_spruce_taiga";
    case giant_spruce_taiga_hills: return "giant_spruce_taiga_hills";
    case modified_gravelly_mountains: return "modified_gravelly_mountains";
    case shattered_savanna: return "shattered_savanna";
    case shattered_savanna_plateau: return "shattered_savanna_plateau";
    case eroded_badlands: return "eroded_badlands";
    case modified_wooded_badlands_plateau: return "modified_wooded_badlands_plateau";
    case modified_badlands_plateau: return "modified_badlands_plateau";
    // 1.14
    case bamboo_jungle: return "bamboo_jungle";
    case bamboo_jungle_hills: return "bamboo_jungle_hills";
    // 1.16
    case soul_sand_valley: return "soul_sand_valley";
    case crimson_forest: return "crimson_forest";
    case warped_forest: return "warped_forest";
    case basalt_deltas: return "basalt_deltas";
    // 1.17
    case dripstone_caves: return "dripstone_caves";
    case lush_caves: return "lush_caves";
    // 1.18
    case meadow: return "meadow";
    case grove: return "grove";
    case snowy_slopes: return "snowy_slopes";
    case stony_peaks: return "stony_peaks";
    case jagged_peaks: return "jagged_peaks";
    case frozen_peaks: return "frozen_peaks";
    // 1.19
    case deep_dark: return "deep_dark";
    case mangrove_swamp: return "mangrove_swamp";
    // 1.20
    case cherry_grove: return "cherry_grove";
    // 1.21.4 (Winter Drop)
    case pale_garden: return "pale_garden";
    }
    return NULL;
}

const char* struct2str(int stype)
{
    switch (stype)
    {
    case Desert_Pyramid:    return "desert_pyramid";
    case Jungle_Temple:     return "jungle_pyramid";
    case Swamp_Hut:         return "swamp_hut";
    case Igloo:             return "igloo";
    case Village:           return "village";
    case Ocean_Ruin:        return "ocean_ruin";
    case Shipwreck:         return "shipwreck";
    case Monument:          return "monument";
    case Mansion:           return "mansion";
    case Outpost:           return "pillager_outpost";
    case Treasure:          return "buried_treasure";
    case Mineshaft:         return "mineshaft";
    case Desert_Well:       return "desert_well";
    case Ruined_Portal:     return "ruined_portal";
    case Ruined_Portal_N:   return "ruined_portal_nether";
    case Geode:             return "amethyst_geode";
    case Ancient_City:      return "ancient_city";
    case Trail_Ruins:       return "trail_ruins";
    case Trial_Chambers:    return "trial_chambers";
    case Fortress:          return "fortress";
    case Bastion:           return "bastion_remnant";
    case End_City:          return "end_city";
    case End_Gateway:       return "end_gateway";
    }
    return NULL;
}

auto cubiomes::detail::set_color(unsigned char colors[256][3], int id, uint32_t hex) -> void
{
    colors[id][0] = (hex >> 16) & 0xff;
    colors[id][1] = (hex >>  8) & 0xff;
    colors[id][2] = (hex >>  0) & 0xff;
}

void initBiomeColors(unsigned char colors[256][3])
{
    // This coloring scheme is largely inspired by the AMIDST program:
    // https://github.com/toolbox4minecraft/amidst/wiki/Biome-Color-Table
    // but with additional biomes for 1.18+, and with some subtle changes to
    // improve contrast for the new world generation.

    memset(colors, 0, 256*3);
    
    //               biome                             hex color     AMIDST
    cubiomes::detail::set_color(colors, ocean,                            0x000070);
    cubiomes::detail::set_color(colors, plains,                           0x8db360);
    cubiomes::detail::set_color(colors, desert,                           0xfa9418);
    cubiomes::detail::set_color(colors, windswept_hills,                  0x606060);
    cubiomes::detail::set_color(colors, forest,                           0x056621);
    cubiomes::detail::set_color(colors, taiga,                            0x0b6a5f); // 0b6659
    cubiomes::detail::set_color(colors, swamp,                            0x07f9b2);
    cubiomes::detail::set_color(colors, river,                            0x0000ff);
    cubiomes::detail::set_color(colors, nether_wastes,                    0x572526); // bf3b3b
    cubiomes::detail::set_color(colors, the_end,                          0x8080ff);
    cubiomes::detail::set_color(colors, frozen_ocean,                     0x7070d6);
    cubiomes::detail::set_color(colors, frozen_river,                     0xa0a0ff);
    cubiomes::detail::set_color(colors, snowy_plains,                     0xffffff);
    cubiomes::detail::set_color(colors, snowy_mountains,                  0xa0a0a0);
    cubiomes::detail::set_color(colors, mushroom_fields,                  0xff00ff);
    cubiomes::detail::set_color(colors, mushroom_field_shore,             0xa000ff);
    cubiomes::detail::set_color(colors, beach,                            0xfade55);
    cubiomes::detail::set_color(colors, desert_hills,                     0xd25f12);
    cubiomes::detail::set_color(colors, wooded_hills,                     0x22551c);
    cubiomes::detail::set_color(colors, taiga_hills,                      0x163933);
    cubiomes::detail::set_color(colors, mountain_edge,                    0x72789a);
    cubiomes::detail::set_color(colors, jungle,                           0x507b0a); // 537b09
    cubiomes::detail::set_color(colors, jungle_hills,                     0x2c4205);
    cubiomes::detail::set_color(colors, sparse_jungle,                    0x60930f); // 628b17
    cubiomes::detail::set_color(colors, deep_ocean,                       0x000030);
    cubiomes::detail::set_color(colors, stony_shore,                      0xa2a284);
    cubiomes::detail::set_color(colors, snowy_beach,                      0xfaf0c0);
    cubiomes::detail::set_color(colors, birch_forest,                     0x307444);
    cubiomes::detail::set_color(colors, birch_forest_hills,               0x1f5f32);
    cubiomes::detail::set_color(colors, dark_forest,                      0x40511a);
    cubiomes::detail::set_color(colors, snowy_taiga,                      0x31554a);
    cubiomes::detail::set_color(colors, snowy_taiga_hills,                0x243f36);
    cubiomes::detail::set_color(colors, old_growth_pine_taiga,            0x596651);
    cubiomes::detail::set_color(colors, giant_tree_taiga_hills,           0x454f3e);
    cubiomes::detail::set_color(colors, windswept_forest,                 0x5b7352); // 507050
    cubiomes::detail::set_color(colors, savanna,                          0xbdb25f);
    cubiomes::detail::set_color(colors, savanna_plateau,                  0xa79d64);
    cubiomes::detail::set_color(colors, badlands,                         0xd94515);
    cubiomes::detail::set_color(colors, wooded_badlands,                  0xb09765);
    cubiomes::detail::set_color(colors, badlands_plateau,                 0xca8c65);
    cubiomes::detail::set_color(colors, small_end_islands,                0x4b4bab); // 8080ff
    cubiomes::detail::set_color(colors, end_midlands,                     0xc9c959); // 8080ff
    cubiomes::detail::set_color(colors, end_highlands,                    0xb5b536); // 8080ff
    cubiomes::detail::set_color(colors, end_barrens,                      0x7070cc); // 8080ff
    cubiomes::detail::set_color(colors, warm_ocean,                       0x0000ac);
    cubiomes::detail::set_color(colors, lukewarm_ocean,                   0x000090);
    cubiomes::detail::set_color(colors, cold_ocean,                       0x202070);
    cubiomes::detail::set_color(colors, deep_warm_ocean,                  0x000050);
    cubiomes::detail::set_color(colors, deep_lukewarm_ocean,              0x000040);
    cubiomes::detail::set_color(colors, deep_cold_ocean,                  0x202038);
    cubiomes::detail::set_color(colors, deep_frozen_ocean,                0x404090);
    cubiomes::detail::set_color(colors, seasonal_forest,                  0x2f560f); // -
    cubiomes::detail::set_color(colors, rainforest,                       0x47840e); // -
    cubiomes::detail::set_color(colors, shrubland,                        0x789e31); // -
    cubiomes::detail::set_color(colors, the_void,                         0x000000);
    cubiomes::detail::set_color(colors, sunflower_plains,                 0xb5db88);
    cubiomes::detail::set_color(colors, desert_lakes,                     0xffbc40);
    cubiomes::detail::set_color(colors, windswept_gravelly_hills,         0x888888);
    cubiomes::detail::set_color(colors, flower_forest,                    0x2d8e49);
    cubiomes::detail::set_color(colors, taiga_mountains,                  0x339287); // 338e81
    cubiomes::detail::set_color(colors, swamp_hills,                      0x2fffda);
    cubiomes::detail::set_color(colors, ice_spikes,                       0xb4dcdc);
    cubiomes::detail::set_color(colors, modified_jungle,                  0x78a332); // 7ba331
    cubiomes::detail::set_color(colors, modified_jungle_edge,             0x88bb37); // 8ab33f
    cubiomes::detail::set_color(colors, old_growth_birch_forest,          0x589c6c);
    cubiomes::detail::set_color(colors, tall_birch_hills,                 0x47875a);
    cubiomes::detail::set_color(colors, dark_forest_hills,                0x687942);
    cubiomes::detail::set_color(colors, snowy_taiga_mountains,            0x597d72);
    cubiomes::detail::set_color(colors, old_growth_spruce_taiga,          0x818e79);
    cubiomes::detail::set_color(colors, giant_spruce_taiga_hills,         0x6d7766);
    cubiomes::detail::set_color(colors, modified_gravelly_mountains,      0x839b7a); // 789878
    cubiomes::detail::set_color(colors, windswept_savanna,                0xe5da87);
    cubiomes::detail::set_color(colors, shattered_savanna_plateau,        0xcfc58c);
    cubiomes::detail::set_color(colors, eroded_badlands,                  0xff6d3d);
    cubiomes::detail::set_color(colors, modified_wooded_badlands_plateau, 0xd8bf8d);
    cubiomes::detail::set_color(colors, modified_badlands_plateau,        0xf2b48d);
    cubiomes::detail::set_color(colors, bamboo_jungle,                    0x849500); // 768e14
    cubiomes::detail::set_color(colors, bamboo_jungle_hills,              0x5c6c04); // 3b470a
    cubiomes::detail::set_color(colors, soul_sand_valley,                 0x4d3a2e); // 5e3830
    cubiomes::detail::set_color(colors, crimson_forest,                   0x981a11); // dd0808
    cubiomes::detail::set_color(colors, warped_forest,                    0x49907b);
    cubiomes::detail::set_color(colors, basalt_deltas,                    0x645f63); // 403636
    cubiomes::detail::set_color(colors, dripstone_caves,                  0x4e3012); // -
    cubiomes::detail::set_color(colors, lush_caves,                       0x283c00); // -
    cubiomes::detail::set_color(colors, meadow,                           0x60a445); // -
    cubiomes::detail::set_color(colors, grove,                            0x47726c); // -
    cubiomes::detail::set_color(colors, snowy_slopes,                     0xc4c4c4); // -
    cubiomes::detail::set_color(colors, jagged_peaks,                     0xdcdcc8); // -
    cubiomes::detail::set_color(colors, frozen_peaks,                     0xb0b3ce); // -
    cubiomes::detail::set_color(colors, stony_peaks,                      0x7b8f74); // -
    cubiomes::detail::set_color(colors, deep_dark,                        0x031f29); // -
    cubiomes::detail::set_color(colors, mangrove_swamp,                   0x2ccc8e); // -
    cubiomes::detail::set_color(colors, cherry_grove,                     0xff91c8); // -
    cubiomes::detail::set_color(colors, pale_garden,                      0x696d95); // -
}

void initBiomeTypeColors(unsigned char colors[256][3])
{
    memset(colors, 0, 256*3);

    cubiomes::detail::set_color(colors, Oceanic,  0x0000a0);
    cubiomes::detail::set_color(colors, Warm,     0xffc000);
    cubiomes::detail::set_color(colors, Lush,     0x00a000);
    cubiomes::detail::set_color(colors, Cold,     0x606060);
    cubiomes::detail::set_color(colors, Freezing, 0xffffff);
}


// find the longest biome name contained in 's'
static int _str2id(const char *s)
{
    if (*s == 0)
        return -1;
    const char *f = NULL;
    int ret = -1, id;
    for (id = 0; id < 256; id++)
    {
        const char *p = biome2str(MC_NEWEST, id);
        if (p && (!f || strlen(f) < strlen(p)))
            if (strstr(s, p)) {f = p; ret = id;}

        const char *t = biome2str(MC_1_17, id);
        if (t && t != p && (!f || strlen(f) < strlen(t)))
            if (strstr(s, t)) {f = t; ret = id;}
    }
    return ret;
}

int parseBiomeColors(unsigned char biomeColors[256][3], const char *buf)
{
    const char *p = buf;
    char bstr[64];
    int id, col[4], n, ib, ic;
    n = 0;
    while (*p)
    {
        for (ib = ic = 0; *p && *p != '\n' && *p != ';'; p++)
        {
            if ((size_t)ib+1 < sizeof(bstr))
            {
                if ((*p >= 'a' && *p <= 'z') || *p == '_')
                    bstr[ib++] = *p;
                else if (*p >= 'A' && *p <= 'Z')
                    bstr[ib++] = (*p - 'A') + 'a';
            }
            if (ic < 4 && (*p == '#' || (p[0] == '0' && p[1] == 'x')))
                col[ic++] = strtol(p+1+(*p=='0'), (char**)&p, 16);
            else if (ic < 4 && *p >= '0' && *p <= '9')
                col[ic++] = strtol(p, (char**)&p, 10);
            if (*p == '\n' || *p == ';')
                break;
        }
        while (*p && *p != '\n') p++;
        while (*p == '\n') p++;

        bstr[ib] = 0;
        id = _str2id(bstr);
        if (id >= 0 && id < 256)
        {
            if (ic == 3)
            {
                biomeColors[id][0] = col[0] & 0xff;
                biomeColors[id][1] = col[1] & 0xff;
                biomeColors[id][2] = col[2] & 0xff;
                n++;
            }
            else if (ic == 1)
            {
                biomeColors[id][0] = (col[0] >> 16) & 0xff;
                biomeColors[id][1] = (col[0] >>  8) & 0xff;
                biomeColors[id][2] = (col[0] >>  0) & 0xff;
                n++;
            }
        }
        else if (ic == 4)
        {
            id = col[0] & 0xff;
            biomeColors[id][0] = col[1] & 0xff;
            biomeColors[id][1] = col[2] & 0xff;
            biomeColors[id][2] = col[3] & 0xff;
            n++;
        }
        else if (ic == 2)
        {
            id = col[0] & 0xff;
            biomeColors[id][0] = (col[1] >> 16) & 0xff;
            biomeColors[id][1] = (col[1] >>  8) & 0xff;
            biomeColors[id][2] = (col[1] >>  0) & 0xff;
            n++;
        }
    }
    return n;
}


int biomesToImage(unsigned char *pixels,
        unsigned char biomeColors[256][3], const int *biomes,
        const unsigned int sx, const unsigned int sy,
        const unsigned int pixscale, const int flip)
{
    unsigned int i, j;
    int containsInvalidBiomes = 0;

    for (j = 0; j < sy; j++)
    {
        for (i = 0; i < sx; i++)
        {
            int id = biomes[j*sx+i];
            unsigned int r, g, b;

            if (id < 0 || id >= 256)
            {
                // This may happen for some intermediate layers
                containsInvalidBiomes = 1;
                r = biomeColors[id&0x7f][0]-40; r = (r>0xff) ? 0x00 : r&0xff;
                g = biomeColors[id&0x7f][1]-40; g = (g>0xff) ? 0x00 : g&0xff;
                b = biomeColors[id&0x7f][2]-40; b = (b>0xff) ? 0x00 : b&0xff;
            }
            else
            {
                r = biomeColors[id][0];
                g = biomeColors[id][1];
                b = biomeColors[id][2];
            }

            unsigned int m, n;
            for (m = 0; m < pixscale; m++) {
                for (n = 0; n < pixscale; n++) {
                    int idx = pixscale * i + n;
                    if (flip)
                        idx += (sx * pixscale) * ((pixscale * j) + m);
                    else
                        idx += (sx * pixscale) * ((pixscale * (sy-1-j)) + m);

                    unsigned char *pix = pixels + 3*idx;
                    pix[0] = (unsigned char)r;
                    pix[1] = (unsigned char)g;
                    pix[2] = (unsigned char)b;
                }
            }
        }
    }

    return containsInvalidBiomes;
}

int savePPM(const char *path, const unsigned char *pixels, const unsigned int sx, const unsigned int sy)
{
    FILE *fp = fopen(path, "wb");
    if (!fp)
        return -1;
    fprintf(fp, "P6\n%d %d\n255\n", sx, sy);
    size_t pixelsLen = 3 * sx * sy;
    size_t written = fwrite(pixels, sizeof pixels[0], pixelsLen, fp);
    fclose(fp);
    return written != pixelsLen;
}
