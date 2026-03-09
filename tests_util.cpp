#include "util.hpp"
#include "biomes.hpp"
#include "finders.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

int main()
{
    const auto require = [](bool ok) -> bool { return ok; };

    {
        const char *path = "tests_util_seeds.txt";
        std::FILE *fp = std::fopen(path, "w");
        if (!require(fp != nullptr)) return 1;
        std::fputs("1\n", fp);
        std::fputs("-2\n", fp);
        std::fputs("bad\n", fp);
        std::fputs("3\n", fp);
        std::fclose(fp);

        uint64_t count = 0;
        uint64_t *seeds = cubiomes::legacy::loadSavedSeeds(path, &count);
        if (!require(seeds != nullptr)) return 1;
        if (!require(count == 3)) return 1;
        if (!require(seeds[0] == 1)) return 1;
        if (!require(seeds[1] == static_cast<uint64_t>(-2))) return 1;
        if (!require(seeds[2] == 3)) return 1;
        std::free(seeds);
        std::remove(path);
    }

    {
        const char *path = "tests_util_seeds_cpp.txt";
        std::FILE *fp = std::fopen(path, "w");
        if (!require(fp != nullptr)) return 1;
        std::fputs("11\n", fp);
        std::fputs("22\n", fp);
        std::fclose(fp);
        const auto loaded = cubiomes::cpp::load_saved_seeds(path);
        if (!require(loaded.size() == 2)) return 1;
        if (!require(loaded[0] == 11 && loaded[1] == 22)) return 1;
        std::remove(path);
    }

    {
        if (!require(cubiomes::cpp::mc_to_string(MC_1_20) == "1.20")) return 1;
        if (!require(cubiomes::cpp::mc_from_string("1.21.2") == MC_1_21_3)) return 1;
        if (!require(cubiomes::cpp::biome_to_string(MC_1_21, plains) == "plains")) return 1;
        if (!require(cubiomes::cpp::structure_to_string(Village) == "village")) return 1;
    }

    {
        unsigned char colors[256][3] = {};
        cubiomes::legacy::initBiomeColors(colors);
        const char *buf =
            "plains #112233\n"
            "5 7 8 9\n"
            "mangrove_swamp 0x010203\n";

        const int mapped = cubiomes::legacy::parseBiomeColors(colors, buf);
        if (!require(mapped == 3)) return 1;
        if (!require(colors[plains][0] == 0x11 && colors[plains][1] == 0x22 && colors[plains][2] == 0x33)) return 1;
        if (!require(colors[5][0] == 7 && colors[5][1] == 8 && colors[5][2] == 9)) return 1;
        if (!require(colors[mangrove_swamp][0] == 1 && colors[mangrove_swamp][1] == 2 && colors[mangrove_swamp][2] == 3)) return 1;
    }

    {
        unsigned char colors[256][3] = {};
        cubiomes::legacy::initBiomeColors(colors);
        const int biomes[] = {plains, -1, ocean, 999};
        unsigned char pixels[4 * 3] = {};
        const int invalid = cubiomes::legacy::biomesToImage(pixels, colors, biomes, 2, 2, 1, 0);
        if (!require(invalid == 1)) return 1;
    }

    {
        const unsigned char pixels[3 * 2 * 1] = {
            255, 0, 0,
            0, 255, 0,
        };
        const char *path = "tests_util.ppm";
        if (!require(cubiomes::legacy::savePPM(path, pixels, 2, 1) == 0)) return 1;

        std::FILE *fp = std::fopen(path, "rb");
        if (!require(fp != nullptr)) return 1;
        char magic[3] = {};
        const size_t n = std::fread(magic, 1, 2, fp);
        std::fclose(fp);
        if (!require(n == 2)) return 1;
        if (!require(std::memcmp(magic, "P6", 2) == 0)) return 1;
        std::remove(path);
    }

    return 0;
}
