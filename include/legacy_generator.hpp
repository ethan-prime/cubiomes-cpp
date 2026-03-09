#pragma once

#include "generator.hpp"

// Compatibility declarations for legacy global generator entry points.
void setupGenerator(Generator *g, int mc, uint32_t flags);
void applySeed(Generator *g, int dim, uint64_t seed);
size_t getMinCacheSize(const Generator *g, int scale, int sx, int sy, int sz);
int *allocCache(const Generator *g, Range r);
int genBiomes(const Generator *g, int *cache, Range r);
int getBiomeAt(const Generator *g, int scale, int x, int y, int z);
const Layer *getLayerForScale(const Generator *g, int scale);
void setupLayerStack(LayerStack *g, int mc, int largeBiomes);
size_t getMinLayerCacheSize(const Layer *layer, int sizeX, int sizeZ);
Layer *setupLayer(Layer *l, mapfunc_t *map, int mc,
    int8_t zoom, int8_t edge, uint64_t saltbase, Layer *p, Layer *p2);
int genArea(const Layer *layer, int *out, int areaX, int areaZ, int areaWidth, int areaHeight);
int mapApproxHeight(float *y, int *ids, const Generator *g,
    const SurfaceNoise *sn, int x, int z, int w, int h);

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
