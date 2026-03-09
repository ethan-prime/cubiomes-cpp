#pragma once

#include "generator.hpp"

#include <cstdint>
#include <vector>

namespace cubiomes::cpp {

enum class Dimension : int {
    Nether = DIM_NETHER,
    Overworld = DIM_OVERWORLD,
    End = DIM_END,
};

class BiomeGenerator {
public:
    explicit BiomeGenerator(int mc, uint32_t flags = 0);

    void reset(int mc, uint32_t flags = 0);
    void apply_seed(Dimension dim, uint64_t seed);

    [[nodiscard]] int biome_at(int scale, int x, int y, int z) const;
    [[nodiscard]] int biome_at_block(int x, int y, int z) const;

    [[nodiscard]] std::vector<int> generate(Range r) const;

    [[nodiscard]] const ::Generator& c_generator() const noexcept;
    [[nodiscard]] ::Generator& c_generator() noexcept;

private:
    ::Generator generator_{};
};

} // namespace cubiomes::cpp
