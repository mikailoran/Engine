# Engine

A small 3D engine written from scratch in C++20 on top of
[bgfx](https://github.com/bkaradzic/bgfx), with a hand-written
Entity-Component-System.

<!-- TODO: screenshot or GIF of the debug scene -->

## About

This is one of the personal sandboxes I use to learn and apply modern C++ and software architecture.
It's not meant to be used by anyone else or have state of the art performance. Right now it renders a lit, data-driven test scene with a free-look camera and a debug UI.

## Highlights

- **ECS from scratch** ([src/ecs/core/](src/ecs/core/)): packed component
  arrays, multi-component views, and deferred entity destruction. It depends
  only on the standard library and has its own googletest suite.
- **RAII resource management**: every resource, GPU objects included, is
  released automatically when its owner goes out of scope, with no manual
  cleanup.
- **Modern C++20**: concepts, move semantics and compile-time type
  information keep the code type-safe and catch misuse at compile time.
- **Data-driven scenes**: entities and components are loaded from JSON
  ([assets/scenes/debug.json](assets/scenes/debug.json)).
- **Tooling**: clang-tidy, clang-format, and git hooks that run the tests.

## Building

Requires CMake 3.25+ and a C++20 compiler. Only tested on Linux (Wayland).

```bash
git clone --recurse-submodules <url>
cmake -S . -B build/Debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/Debug
./build/Debug/engine
```

The configure step needs network access because it fetches nlohmann/json and
googletest. Run the tests with:

```bash
ctest --test-dir build/Debug --output-on-failure
```

Move the camera with **WASD** and look around by holding the **left mouse button**.

## Acknowledgements

[bgfx](https://github.com/bkaradzic/bgfx) via
[bgfx.cmake](https://github.com/bkaradzic/bgfx.cmake),
[nlohmann/json](https://github.com/nlohmann/json),
[googletest](https://github.com/google/googletest), and the Stanford Bunny
from the [Stanford 3D Scanning Repository](https://graphics.stanford.edu/data/3Dscanrep/).
