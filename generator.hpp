#pragma once

#include "layers.hpp"
#include "biomenoise.hpp"

#include <cstdint>
#include <span>
#include <vector>

// generator flags
enum
{
    LARGE_BIOMES            = 0x1,
    NO_BETA_OCEAN           = 0x2,
    FORCE_OCEAN_VARIANTS    = 0x4,
};

STRUCT(Generator)
{
    int mc;
    int dim;
    uint32_t flags;
    uint64_t seed;
    uint64_t sha;

    union {
        struct { // MC 1.0 - 1.17
            LayerStack ls;
            Layer xlayer[5]; // buffer for custom entry layers @{1,4,16,64,256}
            Layer *entry;
        } layered;
        BiomeNoise bn; // MC 1.18+
        BiomeNoiseBeta bnb; // MC A1.2 - B1.7
        //SurfaceNoiseBeta snb;
    };
    NetherNoise nn; // MC 1.16
    EndNoise en; // MC 1.9
};

#ifdef __cplusplus
static_assert(offsetof(Generator, layered) == offsetof(Generator, bn),
    "Generator union members must stay overlaid");
static_assert(offsetof(Generator, layered) == offsetof(Generator, bnb),
    "Generator union members must stay overlaid");
#endif


///=============================================================================
/// Biome Generation
///=============================================================================

/**
 * Sets up a biome generator for a given MC version. The 'flags' can be used to
 * control LARGE_BIOMES or to FORCE_OCEAN_VARIANTS to enable ocean variants at
 * scales higher than normal.
 */
void setupGenerator(Generator *g, int mc, uint32_t flags);

/**
 * Initializes the generator for a given dimension and seed.
 * dim=0:   Overworld
 * dim=-1:  Nether
 * dim=+1:  End
 */
void applySeed(Generator *g, int dim, uint64_t seed);

/**
 * Calculates the buffer size (number of ints) required to generate a cuboidal
 * volume of size (sx, sy, sz). If 'sy' is zero the buffer is calculated for a
 * 2D plane (which is equivalent to sy=1 here).
 * The function allocCache() can be used to allocate the corresponding int
 * buffer using malloc().
 */
size_t getMinCacheSize(const Generator *g, int scale, int sx, int sy, int sz);
int *allocCache(const Generator *g, Range r);

/**
 * Generates the biomes for a cuboidal scaled range given by 'r'.
 * (See description of Range for more detail.)
 *
 * The output is generated inside the cache. Upon success the biome ids can be
 * accessed by indexing as:
 *  cache[ y*r.sx*r.sz + z*r.sx + x ]
 * where (x,y,z) is an relative position inside the range cuboid.
 *
 * The required length of the cache can be determined with getMinCacheSize().
 *
 * The return value is zero upon success.
 */
int genBiomes(const Generator *g, int *cache, Range r);
/**
 * Gets the biome for a specified scaled position. Note that the scale should
 * be either 1 or 4, for block or biome coordinates respectively.
 * Returns none (-1) upon failure.
 */
int getBiomeAt(const Generator *g, int scale, int x, int y, int z);

/**
 * Returns the default layer that corresponds to the given scale.
 * Supported scales are {0, 1, 4, 16, 64, 256}. A scale of zero indicates the
 * custom entry layer 'g->layered.entry'.
 * (Overworld, MC <= 1.17)
 */
const Layer *getLayerForScale(const Generator *g, int scale);


///=============================================================================
/// Layered Biome Generation (old interface up to 1.17)
///=============================================================================

/* Initialize an instance of a layered generator. */
void setupLayerStack(LayerStack *g, int mc, int largeBiomes);

/* Calculates the minimum size of the buffers required to generate an area of
 * dimensions 'sizeX' by 'sizeZ' at the specified layer.
 */
size_t getMinLayerCacheSize(const Layer *layer, int sizeX, int sizeZ);

/* Set up custom layers. */
Layer *setupLayer(Layer *l, mapfunc_t *map, int mc,
    int8_t zoom, int8_t edge, uint64_t saltbase, Layer *p, Layer *p2);

/* Generates the specified area using the current generator settings and stores
 * the biomeIDs in 'out'.
 * The biomeIDs will be indexed in the form: out[x + z*areaWidth]
 * It is recommended that 'out' is allocated using allocCache() for the correct
 * buffer size.
 */
int genArea(const Layer *layer, int *out, int areaX, int areaZ, int areaWidth, int areaHeight);

/**
 * Map an approximation of the Overworld surface height.
 * The horizontal scaling is 1:4. If non-null, the ids are filled with the
 * biomes of the area. The height (written to y) is in blocks.
 */
int mapApproxHeight(float *y, int *ids, const Generator *g,
    const SurfaceNoise *sn, int x, int z, int w, int h);

namespace cubiomes::cpp {

struct GenerateBiomesResult {
    std::int32_t status{};
    std::vector<std::int32_t> biomes{};
};

constexpr auto normalized_sy(std::int32_t sy) -> std::int32_t
{
    return sy == 0 ? 1 : sy;
}

constexpr auto biome_count(const Range &r) -> std::size_t
{
    return static_cast<std::size_t>(r.sx) *
        static_cast<std::size_t>(r.sz) *
        static_cast<std::size_t>(normalized_sy(r.sy));
}

auto setup_generator(Generator &g, std::int32_t mc, std::uint32_t flags) -> void;
auto apply_seed(Generator &g, std::int32_t dim, std::uint64_t seed) -> void;
auto min_cache_size(
    const Generator &g,
    std::int32_t scale,
    std::int32_t sx,
    std::int32_t sy,
    std::int32_t sz
) -> std::size_t;
auto generate_biomes(const Generator &g, Range r) -> GenerateBiomesResult;
auto biome_at(
    const Generator &g,
    std::int32_t scale,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z
) -> std::int32_t;

auto generate_biomes_into(
    const Generator &g,
    Range r,
    std::span<std::int32_t> out_biomes
) -> std::int32_t;

class GeneratorEngine final {
public:
    explicit GeneratorEngine(std::int32_t mc, std::uint32_t flags = 0U);

    auto reset(std::int32_t mc, std::uint32_t flags = 0U) -> void;
    auto apply_seed(std::int32_t dim, std::uint64_t seed) -> void;
    auto generate(Range r) const -> GenerateBiomesResult;
    auto generate_into(Range r, std::span<std::int32_t> out_biomes) const -> std::int32_t;
    auto biome_at(std::int32_t scale, std::int32_t x, std::int32_t y, std::int32_t z) const -> std::int32_t;

    [[nodiscard]] auto c_generator() const noexcept -> const Generator&;
    [[nodiscard]] auto c_generator() noexcept -> Generator&;

private:
    Generator generator_{};
};

} // namespace cubiomes::cpp

namespace cubiomes::legacy {
using ::allocCache;
using ::applySeed;
using ::genArea;
using ::genBiomes;
using ::getBiomeAt;
using ::getLayerForScale;
using ::getMinCacheSize;
using ::mapApproxHeight;
using ::setupGenerator;
using ::setupLayer;
using ::setupLayerStack;
} // namespace cubiomes::legacy
