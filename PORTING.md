# C++ Porting Status

## Current state
- Core library sources are compiled as C++ (`*.cpp`).
- Public headers use `#pragma once` and remain C-compatible using `extern "C"` guards.
- CI validates release and debug builds and runs regression hashes.
- CI compiles and runs both C and C++ consumer smoke programs against the static library.
- CI includes an ABI guard check for key public structs on x86_64 Linux.
- CMake install exports package config files so downstream projects can use `find_package(cubiomes CONFIG REQUIRED)`.

## Guardrails
- Keep behavior parity first; avoid algorithmic changes in mechanical port PRs.
- Keep C API signatures and header compatibility stable.
- Run `./build/cubiomes-tests` after each migration step.

## Remaining migration work
- Resolve legacy anonymous struct/union patterns in headers with explicit C/C++-safe layout.
- Gradually raise warning strictness once warning backlog is resolved.
- Expand dedicated benchmark coverage beyond the current smoke checkpoint.
