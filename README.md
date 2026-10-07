# Engine

A small 3D engine written from scratch in C++20 on top of
[bgfx](https://github.com/bkaradzic/bgfx), with a hand-written
Entity-Component-System.

<!-- TODO: screenshot or GIF of the debug scene -->

## About

This is one of the personal sandboxes I use to learn and apply modern C++ and software architecture.
It's not meant to be used by anyone else or have state of the art performance. Right now it renders a lit, data-driven test scene with rigid-body physics, a free-look camera and a debug UI for spawning and editing entities live.

## Highlights

- **Physics with [Jolt](https://github.com/jrouwe/JoltPhysics)**
  ([docs/physics.md](docs/physics.md)): boxes and spheres that collide, bounce,
  slide and roll, editable live in the inspector. Jolt sits behind a small
  engine-typed facade and is compiled into one module only; the ECS stays
  unaware of it, and an ECS system keeps the two in sync every frame. The
  design choices and the alternatives turned down are written up as
  [decision records](docs/adr/README.md).
- **ECS from scratch** ([src/engine/ecs/core/](src/engine/ecs/core/)): packed component
  arrays, multi-component views, and deferred entity destruction. It depends
  only on the standard library.
- **RAII resource management**: every resource, GPU objects included, is
  released automatically when its owner goes out of scope, with no manual
  cleanup.
- **Modern C++20**: concepts, move semantics and compile-time type
  information keep the code type-safe and catch misuse at compile time.
- **Data-driven scenes**: entities and components are loaded from JSON
  ([assets/scenes/debug.json](assets/scenes/debug.json)).
- **Tested**: googletest suites for the ECS, the rotation maths and physics
  end to end. Physics runs headless, so its tests need no window.
- **Tooling**: clang-tidy, clang-format, and git hooks that run the tests.

## Building

Requires CMake 3.25+ and a C++20 compiler. Only tested on Linux (Wayland).

```bash
git clone --recurse-submodules <url>
cmake -S . -B build/Debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/Debug
./build/Debug/engine
```

The configure step needs network access because it fetches Jolt Physics,
nlohmann/json and googletest. Run the tests with:

```bash
ctest --test-dir build/Debug --output-on-failure
```

Move the camera with **WASD** and look around by holding the **left mouse button**.

## Acknowledgements

[bgfx](https://github.com/bkaradzic/bgfx) via
[bgfx.cmake](https://github.com/bkaradzic/bgfx.cmake),
[Jolt Physics](https://github.com/jrouwe/JoltPhysics),
[nlohmann/json](https://github.com/nlohmann/json),
[googletest](https://github.com/google/googletest), and the Stanford Bunny
from the [Stanford 3D Scanning Repository](https://graphics.stanford.edu/data/3Dscanrep/).
