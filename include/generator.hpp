#pragma once

#include "layers.hpp"
#include "biomenoise.hpp"

#include <cstddef>
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

static_assert(offsetof(Generator, layered) == offsetof(Generator, bn),
    "Generator union members must stay overlaid");
static_assert(offsetof(Generator, layered) == offsetof(Generator, bnb),
    "Generator union members must stay overlaid");

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
