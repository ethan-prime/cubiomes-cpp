#include "cpp_api.hpp"

int main()
{
    cubiomes::cpp::BiomeGenerator gen(MC_1_20);
    gen.apply_seed(cubiomes::cpp::Dimension::Overworld, 262);
    return gen.biome_at_block(0, 63, 0) == mushroom_fields ? 0 : 1;
}
