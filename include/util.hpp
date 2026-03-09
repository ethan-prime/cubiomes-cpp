#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace cubiomes::cpp {

using BiomeColorTable = std::array<std::array<std::uint8_t, 3>, 256>;

struct ParseBiomeColorsResult {
    std::int32_t mapped_count{};
};

struct ImageRenderResult {
    bool contains_invalid_biomes{};
};

enum class SavePpmStatus : std::int32_t {
    OpenFailed = -1,
    Ok = 0,
    WriteFailed = 1,
};

auto load_saved_seeds(std::string_view path) -> std::vector<std::uint64_t>;
auto mc_to_string(std::int32_t mc) -> std::string_view;
auto mc_from_string(std::string_view value) -> std::int32_t;
auto biome_to_string(std::int32_t mc, std::int32_t id) -> std::string_view;
auto structure_to_string(std::int32_t structure_type) -> std::string_view;
auto default_biome_colors() -> BiomeColorTable;
auto default_biome_type_colors() -> BiomeColorTable;
auto parse_biome_colors(BiomeColorTable &colors, std::string_view buffer) -> ParseBiomeColorsResult;
auto biomes_to_image(
    std::span<std::uint8_t> pixels,
    const BiomeColorTable &biome_colors,
    std::span<const std::int32_t> biomes,
    std::uint32_t sx,
    std::uint32_t sy,
    std::uint32_t pixscale,
    bool flip
) -> ImageRenderResult;
auto save_ppm(
    std::string_view path,
    std::span<const std::uint8_t> pixels,
    std::uint32_t sx,
    std::uint32_t sy
) -> SavePpmStatus;

} // namespace cubiomes::cpp
