#include "cpp_api.hpp"
#include "util.hpp"

#include <cstdint>
#include <iostream>
#include <vector>

int main()
{
    using cubiomes::cpp::BiomeGenerator;
    using cubiomes::cpp::Dimension;

    constexpr std::int32_t width = 256;
    constexpr std::int32_t height = 256;
    constexpr std::int32_t scale = 4;
    constexpr std::int32_t y_level = 64;
    constexpr std::uint64_t seed = 123456789ULL;

    BiomeGenerator generator{MC_1_20};
    generator.apply_seed(Dimension::Overworld, seed);

    const Range area{scale, -width / 2, -height / 2, width, height, y_level, 1};
    const std::vector<int> biome_ids = generator.generate(area);

    std::vector<std::int32_t> biome_ids_i32{};
    biome_ids_i32.reserve(biome_ids.size());
    for (const auto id : biome_ids) {
        biome_ids_i32.push_back(static_cast<std::int32_t>(id));
    }

    auto colors = cubiomes::cpp::default_biome_colors();
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) *
                                     static_cast<std::size_t>(height) * 3U);

    const auto render = cubiomes::cpp::biomes_to_image(
        pixels,
        colors,
        biome_ids_i32,
        static_cast<std::uint32_t>(width),
        static_cast<std::uint32_t>(height),
        1,
        false
    );

    const auto save = cubiomes::cpp::save_ppm(
        "trivial_biomes.ppm",
        pixels,
        static_cast<std::uint32_t>(width),
        static_cast<std::uint32_t>(height)
    );

    std::cout
        << "seed=" << seed
        << " size=" << width << "x" << height
        << " invalid_biomes=" << (render.contains_invalid_biomes ? "yes" : "no")
        << " save_status=" << static_cast<int>(save)
        << '\n';

    return save == cubiomes::cpp::SavePpmStatus::Ok ? 0 : 1;
}
