#pragma once

#include "rng.hpp"
#include <cmath>

STRUCT(PerlinNoise)
{
    uint8_t d[256+1];
    uint8_t h2;
    double a, b, c;
    double amplitude;
    double lacunarity;
    double d2;
    double t2;
};

STRUCT(OctaveNoise)
{
    int octcnt;
    PerlinNoise *octaves;
};

STRUCT(DoublePerlinNoise)
{
    double amplitude;
    OctaveNoise octA;
    OctaveNoise octB;
};

/// Helper
static inline ATTR(hot, const)
double maintainPrecision(double x)
{   // This is a highly performance critical function that is used to correct
    // progressing errors from float-maths. However, since cubiomes uses
    // doubles anyway, this seems useless in practice.

    //return x - round(x / 33554432.0) * 33554432.0;
    return x;
}

/// Perlin noise
void perlinInit(PerlinNoise *noise, uint64_t *seed);
void xPerlinInit(PerlinNoise *noise, Xoroshiro *xr);

double samplePerlin(const PerlinNoise *noise, double x, double y, double z,
        double yamp, double ymin);
double sampleSimplex2D(const PerlinNoise *noise, double x, double y);

/// Perlin Octaves
void octaveInit(OctaveNoise *noise, uint64_t *seed, PerlinNoise *octaves,
        int omin, int len);
void octaveInitBeta(OctaveNoise *noise, uint64_t *seed, PerlinNoise *octaves,
        int octcnt, double lac, double lacMul, double persist, double persistMul);
int xOctaveInit(OctaveNoise *noise, Xoroshiro *xr, PerlinNoise *octaves,
        const double *amplitudes, int omin, int len, int nmax);

double sampleOctave(const OctaveNoise *noise, double x, double y, double z);
double sampleOctaveAmp(const OctaveNoise *noise, double x, double y, double z,
        double yamp, double ymin, int ydefault);
double sampleOctave2D(const OctaveNoise *noise, double x, double z);
double sampleOctaveBeta17Biome(const OctaveNoise *noise, double x, double z);
void sampleOctaveBeta17Terrain(const OctaveNoise *noise, double *v,
        double x, double z, int yLacFlag, double lacmin);

/// Double Perlin
void doublePerlinInit(DoublePerlinNoise *noise, uint64_t *seed,
        PerlinNoise *octavesA, PerlinNoise *octavesB, int omin, int len);
int xDoublePerlinInit(DoublePerlinNoise *noise, Xoroshiro *xr,
        PerlinNoise *octaves, const double *amplitudes, int omin, int len, int nmax);

double sampleDoublePerlin(const DoublePerlinNoise *noise,
        double x, double y, double z);

namespace cubiomes::cpp {

inline auto perlin_init(PerlinNoise &noise, uint64_t &seed) -> void
{
    perlinInit(&noise, &seed);
}

inline auto x_perlin_init(PerlinNoise &noise, Xoroshiro &xr) -> void
{
    xPerlinInit(&noise, &xr);
}

inline auto sample_perlin(
    const PerlinNoise &noise,
    double x,
    double y,
    double z,
    double yamp,
    double ymin
) -> double
{
    return samplePerlin(&noise, x, y, z, yamp, ymin);
}

inline auto sample_simplex_2d(const PerlinNoise &noise, double x, double y) -> double
{
    return sampleSimplex2D(&noise, x, y);
}

inline auto sample_octave(const OctaveNoise &noise, double x, double y, double z) -> double
{
    return sampleOctave(&noise, x, y, z);
}

inline auto sample_double_perlin(const DoublePerlinNoise &noise, double x, double y, double z) -> double
{
    return sampleDoublePerlin(&noise, x, y, z);
}

} // namespace cubiomes::cpp
