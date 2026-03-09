#include "util.hpp"
#include "finders.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

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

template <typename Table, typename Key, typename Proj>
constexpr auto table_lookup(const Table &table, const Key &key, Proj proj)
    -> const typename Table::value_type*
{
    if (const auto it = std::ranges::find(table, key, proj); it != table.end()) {
        return std::addressof(*it);
    }
    return nullptr;
}

inline auto parse_seed_line(std::string_view line, std::uint64_t &seed) -> bool
{
    const auto trim_front = line.find_first_not_of(" \t\r\n");
    if (trim_front == std::string_view::npos) {
        return false;
    }
    line.remove_prefix(trim_front);

    const auto trim_back = line.find_last_not_of(" \t\r\n");
    line = line.substr(0, trim_back + 1);

    std::int64_t signed_seed = 0;
    const auto *first = line.data();
    const auto *last = line.data() + line.size();
    const auto [ptr, ec] = std::from_chars(first, last, signed_seed, 10);
    if (ec != std::errc{} || ptr != last) {
        return false;
    }
    seed = static_cast<std::uint64_t>(signed_seed);
    return true;
}

} // namespace cubiomes::detail

namespace {

class PpmWriter final {
public:
    explicit PpmWriter(std::string_view path)
    {
        const std::string path_string{path};
        fp_ = std::fopen(path_string.c_str(), "wb");
    }

    PpmWriter(const PpmWriter&) = delete;
    auto operator=(const PpmWriter&) -> PpmWriter& = delete;

    ~PpmWriter()
    {
        if (fp_ != nullptr) {
            std::fclose(fp_);
        }
    }

    auto write(
        std::span<const std::uint8_t> pixels,
        std::uint32_t sx,
        std::uint32_t sy
    ) -> cubiomes::cpp::SavePpmStatus
    {
        if (fp_ == nullptr) {
            return cubiomes::cpp::SavePpmStatus::OpenFailed;
        }

        const auto expected_pixels_len =
            3ULL * static_cast<std::size_t>(sx) * static_cast<std::size_t>(sy);
        if (pixels.size() < expected_pixels_len) {
            return cubiomes::cpp::SavePpmStatus::WriteFailed;
        }

        std::fprintf(fp_, "P6\n%u %u\n255\n", sx, sy);
        const auto written = std::fwrite(pixels.data(), sizeof pixels[0], expected_pixels_len, fp_);
        return written == expected_pixels_len
            ? cubiomes::cpp::SavePpmStatus::Ok
            : cubiomes::cpp::SavePpmStatus::WriteFailed;
    }

private:
    std::FILE *fp_{nullptr};
};

} // namespace

uint64_t *loadSavedSeeds(const char *fnam, uint64_t *scnt)
{
    if (scnt == nullptr || fnam == nullptr) {
        return nullptr;
    }
    *scnt = 0;

    std::FILE *fp = std::fopen(fnam, "r");
    if (fp == nullptr) {
        return nullptr;
    }

    std::vector<std::uint64_t> seeds{};
    std::array<char, 256> line{};
    while (std::fgets(line.data(), static_cast<int>(line.size()), fp) != nullptr) {
        std::uint64_t seed = 0;
        if (cubiomes::detail::parse_seed_line(line.data(), seed)) {
            seeds.push_back(seed);
        }
    }
    std::fclose(fp);

    if (seeds.empty()) {
        return nullptr;
    }

    auto *base_seeds = static_cast<std::uint64_t*>(
        std::calloc(seeds.size(), sizeof(std::uint64_t))
    );
    if (base_seeds == nullptr) {
        return nullptr;
    }
    std::ranges::copy(seeds, base_seeds);
    *scnt = static_cast<std::uint64_t>(seeds.size());
    return base_seeds;
}


const char* mc2str(int mc)
{
    return cubiomes::cpp::mc_to_string(static_cast<std::int32_t>(mc)).data();
}

int str2mc(const char *s)
{
    if (s == nullptr) {
        return MC_UNDEF;
    }
    return static_cast<int>(cubiomes::cpp::mc_from_string(std::string_view{s}));
}


const char *biome2str(int mc, int id)
{
    using NamePair = std::pair<int, const char *>;
    static constexpr auto kBiomeRenamed118 = std::array{
        NamePair{old_growth_birch_forest, "old_growth_birch_forest"},
        NamePair{old_growth_pine_taiga, "old_growth_pine_taiga"},
        NamePair{old_growth_spruce_taiga, "old_growth_spruce_taiga"},
        NamePair{snowy_plains, "snowy_plains"},
        NamePair{sparse_jungle, "sparse_jungle"},
        NamePair{stony_shore, "stony_shore"},
        NamePair{windswept_hills, "windswept_hills"},
        NamePair{windswept_forest, "windswept_forest"},
        NamePair{windswept_gravelly_hills, "windswept_gravelly_hills"},
        NamePair{windswept_savanna, "windswept_savanna"},
        NamePair{wooded_badlands, "wooded_badlands"},
    };
    static constexpr auto kBiomeNames = std::array{
        NamePair{ocean, "ocean"}, NamePair{plains, "plains"}, NamePair{desert, "desert"},
        NamePair{mountains, "mountains"}, NamePair{forest, "forest"}, NamePair{taiga, "taiga"},
        NamePair{swamp, "swamp"}, NamePair{river, "river"}, NamePair{nether_wastes, "nether_wastes"},
        NamePair{the_end, "the_end"}, NamePair{frozen_ocean, "frozen_ocean"},
        NamePair{frozen_river, "frozen_river"}, NamePair{snowy_tundra, "snowy_tundra"},
        NamePair{snowy_mountains, "snowy_mountains"}, NamePair{mushroom_fields, "mushroom_fields"},
        NamePair{mushroom_field_shore, "mushroom_field_shore"}, NamePair{beach, "beach"},
        NamePair{desert_hills, "desert_hills"}, NamePair{wooded_hills, "wooded_hills"},
        NamePair{taiga_hills, "taiga_hills"}, NamePair{mountain_edge, "mountain_edge"},
        NamePair{jungle, "jungle"}, NamePair{jungle_hills, "jungle_hills"},
        NamePair{jungle_edge, "jungle_edge"}, NamePair{deep_ocean, "deep_ocean"},
        NamePair{stone_shore, "stone_shore"}, NamePair{snowy_beach, "snowy_beach"},
        NamePair{birch_forest, "birch_forest"}, NamePair{birch_forest_hills, "birch_forest_hills"},
        NamePair{dark_forest, "dark_forest"}, NamePair{snowy_taiga, "snowy_taiga"},
        NamePair{snowy_taiga_hills, "snowy_taiga_hills"}, NamePair{giant_tree_taiga, "giant_tree_taiga"},
        NamePair{giant_tree_taiga_hills, "giant_tree_taiga_hills"}, NamePair{wooded_mountains, "wooded_mountains"},
        NamePair{savanna, "savanna"}, NamePair{savanna_plateau, "savanna_plateau"},
        NamePair{badlands, "badlands"}, NamePair{wooded_badlands_plateau, "wooded_badlands_plateau"},
        NamePair{badlands_plateau, "badlands_plateau"}, NamePair{small_end_islands, "small_end_islands"},
        NamePair{end_midlands, "end_midlands"}, NamePair{end_highlands, "end_highlands"},
        NamePair{end_barrens, "end_barrens"}, NamePair{warm_ocean, "warm_ocean"},
        NamePair{lukewarm_ocean, "lukewarm_ocean"}, NamePair{cold_ocean, "cold_ocean"},
        NamePair{deep_warm_ocean, "deep_warm_ocean"}, NamePair{deep_lukewarm_ocean, "deep_lukewarm_ocean"},
        NamePair{deep_cold_ocean, "deep_cold_ocean"}, NamePair{deep_frozen_ocean, "deep_frozen_ocean"},
        NamePair{seasonal_forest, "seasonal_forest"}, NamePair{shrubland, "shrubland"},
        NamePair{rainforest, "rainforest"}, NamePair{the_void, "the_void"},
        NamePair{sunflower_plains, "sunflower_plains"}, NamePair{desert_lakes, "desert_lakes"},
        NamePair{gravelly_mountains, "gravelly_mountains"}, NamePair{flower_forest, "flower_forest"},
        NamePair{taiga_mountains, "taiga_mountains"}, NamePair{swamp_hills, "swamp_hills"},
        NamePair{ice_spikes, "ice_spikes"}, NamePair{modified_jungle, "modified_jungle"},
        NamePair{modified_jungle_edge, "modified_jungle_edge"}, NamePair{tall_birch_forest, "tall_birch_forest"},
        NamePair{tall_birch_hills, "tall_birch_hills"}, NamePair{dark_forest_hills, "dark_forest_hills"},
        NamePair{snowy_taiga_mountains, "snowy_taiga_mountains"}, NamePair{giant_spruce_taiga, "giant_spruce_taiga"},
        NamePair{giant_spruce_taiga_hills, "giant_spruce_taiga_hills"}, NamePair{modified_gravelly_mountains, "modified_gravelly_mountains"},
        NamePair{shattered_savanna, "shattered_savanna"}, NamePair{shattered_savanna_plateau, "shattered_savanna_plateau"},
        NamePair{eroded_badlands, "eroded_badlands"}, NamePair{modified_wooded_badlands_plateau, "modified_wooded_badlands_plateau"},
        NamePair{modified_badlands_plateau, "modified_badlands_plateau"}, NamePair{bamboo_jungle, "bamboo_jungle"},
        NamePair{bamboo_jungle_hills, "bamboo_jungle_hills"}, NamePair{soul_sand_valley, "soul_sand_valley"},
        NamePair{crimson_forest, "crimson_forest"}, NamePair{warped_forest, "warped_forest"},
        NamePair{basalt_deltas, "basalt_deltas"}, NamePair{dripstone_caves, "dripstone_caves"},
        NamePair{lush_caves, "lush_caves"}, NamePair{meadow, "meadow"}, NamePair{grove, "grove"},
        NamePair{snowy_slopes, "snowy_slopes"}, NamePair{stony_peaks, "stony_peaks"},
        NamePair{jagged_peaks, "jagged_peaks"}, NamePair{frozen_peaks, "frozen_peaks"},
        NamePair{deep_dark, "deep_dark"}, NamePair{mangrove_swamp, "mangrove_swamp"},
        NamePair{cherry_grove, "cherry_grove"}, NamePair{pale_garden, "pale_garden"},
    };

    if (mc >= MC_1_18) {
        if (const auto *entry = cubiomes::detail::table_lookup(kBiomeRenamed118, id, &NamePair::first); entry != nullptr) {
            return entry->second;
        }
    }
    if (const auto *entry = cubiomes::detail::table_lookup(kBiomeNames, id, &NamePair::first); entry != nullptr) {
        return entry->second;
    }
    return nullptr;
}

const char* struct2str(int stype)
{
    using NamePair = std::pair<int, const char *>;
    static constexpr auto kStructureNames = std::array{
        NamePair{Desert_Pyramid, "desert_pyramid"},
        NamePair{Jungle_Temple, "jungle_pyramid"},
        NamePair{Swamp_Hut, "swamp_hut"},
        NamePair{Igloo, "igloo"},
        NamePair{Village, "village"},
        NamePair{Ocean_Ruin, "ocean_ruin"},
        NamePair{Shipwreck, "shipwreck"},
        NamePair{Monument, "monument"},
        NamePair{Mansion, "mansion"},
        NamePair{Outpost, "pillager_outpost"},
        NamePair{Treasure, "buried_treasure"},
        NamePair{Mineshaft, "mineshaft"},
        NamePair{Desert_Well, "desert_well"},
        NamePair{Ruined_Portal, "ruined_portal"},
        NamePair{Ruined_Portal_N, "ruined_portal_nether"},
        NamePair{Geode, "amethyst_geode"},
        NamePair{Ancient_City, "ancient_city"},
        NamePair{Trail_Ruins, "trail_ruins"},
        NamePair{Trial_Chambers, "trial_chambers"},
        NamePair{Fortress, "fortress"},
        NamePair{Bastion, "bastion_remnant"},
        NamePair{End_City, "end_city"},
        NamePair{End_Gateway, "end_gateway"},
    };

    if (const auto *entry = cubiomes::detail::table_lookup(kStructureNames, stype, &NamePair::first); entry != nullptr) {
        return entry->second;
    }
    return nullptr;
}

namespace cubiomes::detail {

using HexColorTable = std::array<std::uint32_t, 256>;
using RgbTable = std::array<std::array<unsigned char, 3>, 256>;

constexpr auto hex_to_rgb(std::uint32_t hex) -> std::array<unsigned char, 3>
{
    return {
        static_cast<unsigned char>((hex >> 16U) & 0xffU),
        static_cast<unsigned char>((hex >> 8U) & 0xffU),
        static_cast<unsigned char>(hex & 0xffU),
    };
}

constexpr auto make_biome_color_hex_table() -> HexColorTable
{
    HexColorTable hex_colors{};
    hex_colors[ocean] = 0x000070U;
    hex_colors[plains] = 0x8db360U;
    hex_colors[desert] = 0xfa9418U;
    hex_colors[windswept_hills] = 0x606060U;
    hex_colors[forest] = 0x056621U;
    hex_colors[taiga] = 0x0b6a5fU; // 0b6659
    hex_colors[swamp] = 0x07f9b2U;
    hex_colors[river] = 0x0000ffU;
    hex_colors[nether_wastes] = 0x572526U; // bf3b3b
    hex_colors[the_end] = 0x8080ffU;
    hex_colors[frozen_ocean] = 0x7070d6U;
    hex_colors[frozen_river] = 0xa0a0ffU;
    hex_colors[snowy_plains] = 0xffffffU;
    hex_colors[snowy_mountains] = 0xa0a0a0U;
    hex_colors[mushroom_fields] = 0xff00ffU;
    hex_colors[mushroom_field_shore] = 0xa000ffU;
    hex_colors[beach] = 0xfade55U;
    hex_colors[desert_hills] = 0xd25f12U;
    hex_colors[wooded_hills] = 0x22551cU;
    hex_colors[taiga_hills] = 0x163933U;
    hex_colors[mountain_edge] = 0x72789aU;
    hex_colors[jungle] = 0x507b0aU; // 537b09
    hex_colors[jungle_hills] = 0x2c4205U;
    hex_colors[sparse_jungle] = 0x60930fU; // 628b17
    hex_colors[deep_ocean] = 0x000030U;
    hex_colors[stony_shore] = 0xa2a284U;
    hex_colors[snowy_beach] = 0xfaf0c0U;
    hex_colors[birch_forest] = 0x307444U;
    hex_colors[birch_forest_hills] = 0x1f5f32U;
    hex_colors[dark_forest] = 0x40511aU;
    hex_colors[snowy_taiga] = 0x31554aU;
    hex_colors[snowy_taiga_hills] = 0x243f36U;
    hex_colors[old_growth_pine_taiga] = 0x596651U;
    hex_colors[giant_tree_taiga_hills] = 0x454f3eU;
    hex_colors[windswept_forest] = 0x5b7352U; // 507050
    hex_colors[savanna] = 0xbdb25fU;
    hex_colors[savanna_plateau] = 0xa79d64U;
    hex_colors[badlands] = 0xd94515U;
    hex_colors[wooded_badlands] = 0xb09765U;
    hex_colors[badlands_plateau] = 0xca8c65U;
    hex_colors[small_end_islands] = 0x4b4babU; // 8080ff
    hex_colors[end_midlands] = 0xc9c959U; // 8080ff
    hex_colors[end_highlands] = 0xb5b536U; // 8080ff
    hex_colors[end_barrens] = 0x7070ccU; // 8080ff
    hex_colors[warm_ocean] = 0x0000acU;
    hex_colors[lukewarm_ocean] = 0x000090U;
    hex_colors[cold_ocean] = 0x202070U;
    hex_colors[deep_warm_ocean] = 0x000050U;
    hex_colors[deep_lukewarm_ocean] = 0x000040U;
    hex_colors[deep_cold_ocean] = 0x202038U;
    hex_colors[deep_frozen_ocean] = 0x404090U;
    hex_colors[seasonal_forest] = 0x2f560fU; // -
    hex_colors[rainforest] = 0x47840eU; // -
    hex_colors[shrubland] = 0x789e31U; // -
    hex_colors[the_void] = 0x000000U;
    hex_colors[sunflower_plains] = 0xb5db88U;
    hex_colors[desert_lakes] = 0xffbc40U;
    hex_colors[windswept_gravelly_hills] = 0x888888U;
    hex_colors[flower_forest] = 0x2d8e49U;
    hex_colors[taiga_mountains] = 0x339287U; // 338e81
    hex_colors[swamp_hills] = 0x2fffdaU;
    hex_colors[ice_spikes] = 0xb4dcdcU;
    hex_colors[modified_jungle] = 0x78a332U; // 7ba331
    hex_colors[modified_jungle_edge] = 0x88bb37U; // 8ab33f
    hex_colors[old_growth_birch_forest] = 0x589c6cU;
    hex_colors[tall_birch_hills] = 0x47875aU;
    hex_colors[dark_forest_hills] = 0x687942U;
    hex_colors[snowy_taiga_mountains] = 0x597d72U;
    hex_colors[old_growth_spruce_taiga] = 0x818e79U;
    hex_colors[giant_spruce_taiga_hills] = 0x6d7766U;
    hex_colors[modified_gravelly_mountains] = 0x839b7aU; // 789878
    hex_colors[windswept_savanna] = 0xe5da87U;
    hex_colors[shattered_savanna_plateau] = 0xcfc58cU;
    hex_colors[eroded_badlands] = 0xff6d3dU;
    hex_colors[modified_wooded_badlands_plateau] = 0xd8bf8dU;
    hex_colors[modified_badlands_plateau] = 0xf2b48dU;
    hex_colors[bamboo_jungle] = 0x849500U; // 768e14
    hex_colors[bamboo_jungle_hills] = 0x5c6c04U; // 3b470a
    hex_colors[soul_sand_valley] = 0x4d3a2eU; // 5e3830
    hex_colors[crimson_forest] = 0x981a11U; // dd0808
    hex_colors[warped_forest] = 0x49907bU;
    hex_colors[basalt_deltas] = 0x645f63U; // 403636
    hex_colors[dripstone_caves] = 0x4e3012U; // -
    hex_colors[lush_caves] = 0x283c00U; // -
    hex_colors[meadow] = 0x60a445U; // -
    hex_colors[grove] = 0x47726cU; // -
    hex_colors[snowy_slopes] = 0xc4c4c4U; // -
    hex_colors[jagged_peaks] = 0xdcdcc8U; // -
    hex_colors[frozen_peaks] = 0xb0b3ceU; // -
    hex_colors[stony_peaks] = 0x7b8f74U; // -
    hex_colors[deep_dark] = 0x031f29U; // -
    hex_colors[mangrove_swamp] = 0x2ccc8eU; // -
    hex_colors[cherry_grove] = 0xff91c8U; // -
    hex_colors[pale_garden] = 0x696d95U; // -
    return hex_colors;
}

constexpr auto make_biome_type_color_hex_table() -> HexColorTable
{
    HexColorTable hex_colors{};
    hex_colors[Oceanic] = 0x0000a0U;
    hex_colors[Warm] = 0xffc000U;
    hex_colors[Lush] = 0x00a000U;
    hex_colors[Cold] = 0x606060U;
    hex_colors[Freezing] = 0xffffffU;
    return hex_colors;
}

constexpr auto kBiomeColorHexTable = make_biome_color_hex_table();
constexpr auto kBiomeTypeColorHexTable = make_biome_type_color_hex_table();

auto apply_hex_color_table(
    unsigned char colors[256][3],
    const HexColorTable &hex_table
) -> void
{
    for (std::size_t i = 0; i < hex_table.size(); ++i)
    {
        if (const auto rgb = hex_to_rgb(hex_table[i]); true)
        {
            colors[i][0] = rgb[0];
            colors[i][1] = rgb[1];
            colors[i][2] = rgb[2];
        }
    }
}

class ColorMap final {
public:
    constexpr explicit ColorMap(const HexColorTable &hex_table)
    {
        for (std::size_t i = 0; i < hex_table.size(); ++i) {
            rgb_[i] = hex_to_rgb(hex_table[i]);
        }
    }

    auto copy_to(unsigned char colors[256][3]) const -> void
    {
        for (std::size_t i = 0; i < rgb_.size(); ++i) {
            colors[i][0] = rgb_[i][0];
            colors[i][1] = rgb_[i][1];
            colors[i][2] = rgb_[i][2];
        }
    }

private:
    RgbTable rgb_{};
};

constexpr auto kBiomeColorMap = ColorMap{kBiomeColorHexTable};
constexpr auto kBiomeTypeColorMap = ColorMap{kBiomeTypeColorHexTable};

} // namespace cubiomes::detail

void initBiomeColors(unsigned char colors[256][3])
{
    namespace detail = cubiomes::detail;

    // This coloring scheme is largely inspired by the AMIDST program:
    // https://github.com/toolbox4minecraft/amidst/wiki/Biome-Color-Table
    // but with additional biomes for 1.18+, and with some subtle changes to
    // improve contrast for the new world generation.

    detail::kBiomeColorMap.copy_to(colors);
}

void initBiomeTypeColors(unsigned char colors[256][3])
{
    namespace detail = cubiomes::detail;
    detail::kBiomeTypeColorMap.copy_to(colors);
}


// find the longest biome name contained in 's'
static int _str2id(const char *s)
{
    if (s == nullptr || *s == '\0') {
        return -1;
    }

    const std::string_view haystack{s};
    std::size_t best_len = 0;
    int best_id = -1;

    for (int id = 0; id < 256; ++id) {
        for (const int mc : {MC_NEWEST, MC_1_17}) {
            if (const auto *name = biome2str(mc, id); name != nullptr) {
                const std::string_view needle{name};
                if (needle.size() > best_len && haystack.find(needle) != std::string_view::npos) {
                    best_len = needle.size();
                    best_id = id;
                }
            }
        }
    }
    return best_id;
}

int parseBiomeColors(unsigned char biomeColors[256][3], const char *buf)
{
    if (buf == nullptr) {
        return 0;
    }

    auto write_color = [&](int id, long r, long g, long b) -> void {
        biomeColors[id][0] = static_cast<unsigned char>(r & 0xffL);
        biomeColors[id][1] = static_cast<unsigned char>(g & 0xffL);
        biomeColors[id][2] = static_cast<unsigned char>(b & 0xffL);
    };

    int mapped = 0;
    const char *p = buf;
    while (*p != '\0') {
        std::array<char, 64> biome_name{};
        std::array<long, 4> values{};
        int value_count = 0;
        std::size_t biome_name_len = 0;

        for (; *p != '\0' && *p != '\n' && *p != ';'; ++p) {
            if (biome_name_len + 1 < biome_name.size()) {
                if ((*p >= 'a' && *p <= 'z') || *p == '_') {
                    biome_name[biome_name_len++] = *p;
                } else if (*p >= 'A' && *p <= 'Z') {
                    biome_name[biome_name_len++] = static_cast<char>((*p - 'A') + 'a');
                }
            }

            if (value_count < static_cast<int>(values.size()) &&
                (*p == '#' || (p[0] == '0' && p[1] == 'x')))
            {
                values[value_count++] = std::strtol(p + 1 + (*p == '0'), const_cast<char**>(&p), 16);
            } else if (value_count < static_cast<int>(values.size()) && *p >= '0' && *p <= '9') {
                values[value_count++] = std::strtol(p, const_cast<char**>(&p), 10);
            }

            if (*p == '\n' || *p == ';') {
                break;
            }
        }

        while (*p != '\0' && *p != '\n') {
            ++p;
        }
        while (*p == '\n') {
            ++p;
        }

        biome_name[biome_name_len] = '\0';
        const int biome_id = _str2id(biome_name.data());
        if (biome_id >= 0 && biome_id < 256) {
            if (value_count == 3) {
                write_color(biome_id, values[0], values[1], values[2]);
                ++mapped;
            } else if (value_count == 1) {
                write_color(biome_id, values[0] >> 16, values[0] >> 8, values[0]);
                ++mapped;
            }
            continue;
        }

        if (value_count == 4) {
            const int id = static_cast<int>(values[0] & 0xffL);
            write_color(id, values[1], values[2], values[3]);
            ++mapped;
        } else if (value_count == 2) {
            const int id = static_cast<int>(values[0] & 0xffL);
            write_color(id, values[1] >> 16, values[1] >> 8, values[1]);
            ++mapped;
        }
    }
    return mapped;
}


int biomesToImage(unsigned char *pixels,
        unsigned char biomeColors[256][3], const int *biomes,
        const unsigned int sx, const unsigned int sy,
        const unsigned int pixscale, const int flip)
{
    auto darken_component = [](unsigned char c) -> unsigned char {
        return c > 40 ? static_cast<unsigned char>(c - 40) : 0;
    };

    int contains_invalid_biomes = 0;
    for (std::size_t j = 0; j < sy; ++j) {
        for (std::size_t i = 0; i < sx; ++i) {
            const int biome_id = biomes[j * sx + i];
            std::array<unsigned char, 3> rgb{};

            if (biome_id < 0 || biome_id >= 256) {
                contains_invalid_biomes = 1;
                const auto idx = static_cast<std::size_t>(biome_id & 0x7f);
                rgb[0] = darken_component(biomeColors[idx][0]);
                rgb[1] = darken_component(biomeColors[idx][1]);
                rgb[2] = darken_component(biomeColors[idx][2]);
            } else {
                rgb[0] = biomeColors[biome_id][0];
                rgb[1] = biomeColors[biome_id][1];
                rgb[2] = biomeColors[biome_id][2];
            }

            for (std::size_t m = 0; m < pixscale; ++m) {
                for (std::size_t n = 0; n < pixscale; ++n) {
                    std::size_t idx = (pixscale * i) + n;
                    if (flip != 0) {
                        idx += (sx * pixscale) * ((pixscale * j) + m);
                    } else {
                        idx += (sx * pixscale) * ((pixscale * (sy - 1 - j)) + m);
                    }

                    auto *pix = pixels + (3 * idx);
                    pix[0] = rgb[0];
                    pix[1] = rgb[1];
                    pix[2] = rgb[2];
                }
            }
        }
    }

    return contains_invalid_biomes;
}

int savePPM(const char *path, const unsigned char *pixels, const unsigned int sx, const unsigned int sy)
{
    if (path == nullptr || pixels == nullptr) {
        return -1;
    }
    const auto status = cubiomes::cpp::save_ppm(
        std::string_view{path},
        std::span<const std::uint8_t>{pixels, 3ULL * static_cast<std::size_t>(sx) * static_cast<std::size_t>(sy)},
        sx,
        sy
    );
    return static_cast<int>(status);
}

namespace cubiomes::cpp {

namespace {

auto to_legacy_colors(const BiomeColorTable &colors, unsigned char out[256][3]) -> void
{
    for (std::size_t i = 0; i < colors.size(); ++i) {
        out[i][0] = colors[i][0];
        out[i][1] = colors[i][1];
        out[i][2] = colors[i][2];
    }
}

auto from_legacy_colors(const unsigned char in[256][3]) -> BiomeColorTable
{
    BiomeColorTable colors{};
    for (std::size_t i = 0; i < colors.size(); ++i) {
        colors[i][0] = in[i][0];
        colors[i][1] = in[i][1];
        colors[i][2] = in[i][2];
    }
    return colors;
}

} // namespace

auto load_saved_seeds(std::string_view path) -> std::vector<std::uint64_t>
{
    if (path.empty()) {
        return {};
    }

    const std::string path_string{path};
    std::uint64_t count = 0;
    auto *raw = ::loadSavedSeeds(path_string.c_str(), &count);
    if (raw == nullptr || count == 0) {
        return {};
    }

    auto deleter = [](std::uint64_t *p) { std::free(p); };
    std::unique_ptr<std::uint64_t, decltype(deleter)> guard{raw, deleter};
    return std::vector<std::uint64_t>{raw, raw + count};
}

auto mc_to_string(std::int32_t mc) -> std::string_view
{
    constexpr auto kUnknownVersion = std::string_view{"?"};
    if (const auto *entry = cubiomes::detail::table_lookup(
            cubiomes::detail::kMcToStr, static_cast<int>(mc), &cubiomes::detail::McPair::first
        ); entry != nullptr) {
        return std::string_view{entry->second};
    }
    return kUnknownVersion;
}

auto mc_from_string(std::string_view value) -> std::int32_t
{
    if (value.empty()) {
        return MC_UNDEF;
    }
    if (const auto *entry = cubiomes::detail::table_lookup(
            cubiomes::detail::kStrToMc, value, &cubiomes::detail::StrPair::first
        ); entry != nullptr) {
        return static_cast<std::int32_t>(entry->second);
    }
    return MC_UNDEF;
}

auto biome_to_string(std::int32_t mc, std::int32_t id) -> std::string_view
{
    if (const char *name = ::biome2str(static_cast<int>(mc), static_cast<int>(id)); name != nullptr) {
        return std::string_view{name};
    }
    return {};
}

auto structure_to_string(std::int32_t structure_type) -> std::string_view
{
    if (const char *name = ::struct2str(static_cast<int>(structure_type)); name != nullptr) {
        return std::string_view{name};
    }
    return {};
}

auto default_biome_colors() -> BiomeColorTable
{
    unsigned char legacy[256][3]{};
    ::initBiomeColors(legacy);
    return from_legacy_colors(legacy);
}

auto default_biome_type_colors() -> BiomeColorTable
{
    unsigned char legacy[256][3]{};
    ::initBiomeTypeColors(legacy);
    return from_legacy_colors(legacy);
}

auto parse_biome_colors(BiomeColorTable &colors, std::string_view buffer) -> ParseBiomeColorsResult
{
    ParseBiomeColorsResult result{};
    unsigned char legacy[256][3]{};
    to_legacy_colors(colors, legacy);
    const std::string tmp{buffer};
    result.mapped_count = ::parseBiomeColors(legacy, tmp.c_str());
    colors = from_legacy_colors(legacy);
    return result;
}

auto biomes_to_image(
    std::span<std::uint8_t> pixels,
    const BiomeColorTable &biome_colors,
    std::span<const std::int32_t> biomes,
    std::uint32_t sx,
    std::uint32_t sy,
    std::uint32_t pixscale,
    bool flip
) -> ImageRenderResult
{
    ImageRenderResult result{};
    unsigned char legacy_colors[256][3]{};
    to_legacy_colors(biome_colors, legacy_colors);

    std::vector<int> legacy_biomes{};
    legacy_biomes.reserve(biomes.size());
    for (const auto biome : biomes) {
        legacy_biomes.push_back(static_cast<int>(biome));
    }

    const std::size_t expected_pixels =
        3ULL * static_cast<std::size_t>(sx) * static_cast<std::size_t>(sy) *
        static_cast<std::size_t>(pixscale) * static_cast<std::size_t>(pixscale);
    if (pixels.size() < expected_pixels) {
        result.contains_invalid_biomes = true;
        return result;
    }

    result.contains_invalid_biomes = (::biomesToImage(
        pixels.data(),
        legacy_colors,
        legacy_biomes.data(),
        sx,
        sy,
        pixscale,
        flip ? 1 : 0
    ) != 0);
    return result;
}

auto save_ppm(
    std::string_view path,
    std::span<const std::uint8_t> pixels,
    std::uint32_t sx,
    std::uint32_t sy
) -> SavePpmStatus
{
    PpmWriter writer{path};
    return writer.write(pixels, sx, sy);
}

} // namespace cubiomes::cpp
