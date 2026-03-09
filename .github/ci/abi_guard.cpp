#include "generator.hpp"
#include "layers.hpp"
#include <cstddef>

#if defined(__x86_64__) && __SIZEOF_POINTER__ == 8
static_assert(sizeof(Generator) == 27592, "Generator ABI size changed");
static_assert(alignof(Generator) == 8, "Generator ABI alignment changed");
static_assert(offsetof(Generator, layered) == 32, "Generator union offset changed");
static_assert(offsetof(Generator, bn) == 32, "Generator bn offset changed");
static_assert(offsetof(Generator, bnb) == 32, "Generator bnb offset changed");
static_assert(offsetof(Generator, nn) == 24624, "Generator nn offset changed");
static_assert(offsetof(Generator, en) == 27264, "Generator en offset changed");
static_assert(sizeof(Layer) == 72, "Layer ABI size changed");
static_assert(sizeof(LayerStack) == 4752, "LayerStack ABI size changed");
static_assert(sizeof(BiomeNoise) == 24592, "BiomeNoise ABI size changed");
static_assert(sizeof(BiomeNoiseBeta) == 3256, "BiomeNoiseBeta ABI size changed");
#endif

int main()
{
    return 0;
}
