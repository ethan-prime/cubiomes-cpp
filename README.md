# cubiomes-cpp

`cubiomes-cpp` is a C++23 library for Minecraft Java Edition worldgen and seedfinding.

This repo now provides modern C++ APIs under `cubiomes::cpp` while keeping high-performance core generation code.

## What You Get

- C++23-first API (`include/cpp_api.hpp`, `include/generator.hpp`, `include/finders.hpp`, etc.)
- Library targets for static/shared linking
- Installable CMake package (`find_package(cubiomes CONFIG REQUIRED)`)
- Seedfinding helpers for structures, biomes, strongholds, spawn, and more

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Key output libraries:

- `build/libcubiomes.so` (shared)
- `build/libcubiomes_static.a` (core static)
- `build/libcubiomes_cpp.a` (C++ convenience API layer)

Run tests:

```bash
./build/cubiomes-tests
./build/cubiomes-cpp-api-tests
./build/cubiomes-util-tests
```

## Link As A Library

### Option A: Add As Subdirectory (recommended for monorepos)

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_seedfinder LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(path/to/cubiomes-cpp)

add_executable(my_seedfinder src/main.cpp)
target_link_libraries(my_seedfinder PRIVATE cubiomes::cubiomes_cpp)
```

### Option B: Install + `find_package`

Install:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
cmake --install build --prefix /usr/local
```

Consume:

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_seedfinder LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(cubiomes CONFIG REQUIRED)

add_executable(my_seedfinder src/main.cpp)
target_link_libraries(my_seedfinder PRIVATE cubiomes::cubiomes_cpp)
```

### Option C: Direct Compile/Link

```bash
c++ -std=c++23 -O3 -Iinclude examples/seedfind_demo.cpp \
  build/libcubiomes_cpp.a -lm -pthread -o build/seedfind_demo
```

## Proof-of-Concept Seedfinder (C++ API)

```cpp
#include "cpp_api.hpp"
#include "finders.hpp"

#include <cstdint>
#include <cstdlib>
#include <print>

int main() {
    constexpr int mc = MC_1_20;

    cubiomes::cpp::BiomeGenerator gen(mc);
    auto filter = cubiomes::cpp::BiomeFilterBuilder(mc)
        .require(plains)
        .match_any(forest)
        .build();

    const std::uint64_t begin = 0;
    const std::uint64_t end = 1'000'000;

    for (std::uint64_t seed = begin; seed < end; ++seed) {
        // Fast prefilter: candidate village attempt in region (0, 0)
        const auto village = cubiomes::cpp::structure_position(Village, mc, seed, 0, 0);
        if (!village) {
            continue;
        }
        if (std::abs(village->x) + std::abs(village->z) > 1200) {
            continue;
        }

        // Full-seed checks
        gen.apply_seed(cubiomes::cpp::Dimension::Overworld, seed);

        // Example biome filter in a small window near origin
        Range r{4, -64, -64, 128, 128, 63, 1};
        const auto chk = cubiomes::cpp::check_for_biomes(
            gen.c_generator(), r, DIM_OVERWORLD, seed, filter);
        if (chk.status <= 0) {
            continue;
        }

        const auto spawn = cubiomes::cpp::spawn(gen.c_generator());
        std::println(
            "seed={} spawn=({},{}) village=({},{})",
            seed, spawn.x, spawn.z, village->x, village->z
        );
    }

    return 0;
}
```

## Practical Notes

- For many structures, position prefiltering depends on lower 48 seed bits; biome viability requires full 64-bit checks.
- Use `BiomeGenerator::apply_seed(...)` once per candidate seed, then run as many checks as possible before moving on.
- Prefer range generation (`generate(...)`, `check_for_biomes(...)`) over many single-point calls for throughput.

## API References In This Repo

- C++ facade: `include/cpp_api.hpp`
- Generator wrappers: `include/generator.hpp`
- Finder wrappers: `include/finders.hpp`
- Biome noise wrappers: `include/biomenoise.hpp`
- Usage examples/tests: `tests/tests_cpp_api.cpp`
