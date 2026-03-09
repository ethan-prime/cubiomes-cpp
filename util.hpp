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

/* Loads a list of seeds from a file. The seeds should be written as decimal
 * ASCII numbers separated by newlines.
 * @fnam: file path
 * @scnt: number of valid seeds found in the file, which is also the number of
 *        elements in the returned buffer
 *
 * Return a pointer to a dynamically allocated seed list.
 */
std::uint64_t *loadSavedSeeds(const char *fnam, std::uint64_t *scnt);


/// convert between version enum and text
const char* mc2str(int mc);
int str2mc(const char *s);

/// get the resource id name for a biome (for versions 1.13+)
const char *biome2str(int mc, int id);

/// get the resource id name for a structure
const char *struct2str(int stype);

/// initialize a biome colormap with some defaults
void initBiomeColors(unsigned char biomeColors[256][3]);
void initBiomeTypeColors(unsigned char biomeColors[256][3]);

/* Attempts to parse a biome-color mappings from a text buffer.
 * The parser makes one attempt per line and is not very picky regarding a
 * combination of biomeID/name with a color, represented as either a single
 * number or as a triplet in decimal or as hex (preceeded by 0x or #).
 * Returns the number of successfully mapped biome ids
 */
int parseBiomeColors(unsigned char biomeColors[256][3], const char *buf);

int biomesToImage(unsigned char *pixels,
        unsigned char biomeColors[256][3], const int *biomes,
        const unsigned int sx, const unsigned int sy,
        const unsigned int pixscale, const int flip);

/* Save the pixel buffer (e.g. from biomesToImage) to the given path as an PPM
 * image file. Returns 0 if successful, or -1 if the file could not be opened,
 * or 1 if not all the pixel data could be written to the file.
 */
int savePPM(const char* path, const unsigned char *pixels,
        const unsigned int sx, const unsigned int sy);

namespace cubiomes::legacy {
using ::biome2str;
using ::biomesToImage;
using ::initBiomeColors;
using ::initBiomeTypeColors;
using ::loadSavedSeeds;
using ::mc2str;
using ::parseBiomeColors;
using ::savePPM;
using ::str2mc;
using ::struct2str;
} // namespace cubiomes::legacy
