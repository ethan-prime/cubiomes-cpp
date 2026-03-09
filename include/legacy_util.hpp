#pragma once

#include <cstdint>

// Compatibility declarations for legacy global utility entry points.
std::uint64_t *loadSavedSeeds(const char *fnam, std::uint64_t *scnt);
const char *mc2str(int mc);
int str2mc(const char *s);
const char *biome2str(int mc, int id);
const char *struct2str(int stype);
void initBiomeColors(unsigned char biomeColors[256][3]);
void initBiomeTypeColors(unsigned char biomeColors[256][3]);
int parseBiomeColors(unsigned char biomeColors[256][3], const char *buf);
int biomesToImage(
    unsigned char *pixels,
    unsigned char biomeColors[256][3],
    const int *biomes,
    unsigned int sx,
    unsigned int sy,
    unsigned int pixscale,
    int flip
);
int savePPM(const char *path, const unsigned char *pixels, unsigned int sx, unsigned int sy);

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
