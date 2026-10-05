# 0003: Hide Jolt behind a `PhysicsWorld` facade

**Status:** accepted

## Context

The first integration put Jolt calls straight into the ECS physics system.
That one class owned Jolt's global setup, the world, body creation and
destruction, syncing in both directions, fixed stepping, unit conversions and
wake-up rules. Jolt's headers reached `main.cpp`, physics needed the renderer's
meshes, and nothing could be tested without a window.

## Decision

Put Jolt behind `PhysicsWorld` (`src/physics/`), a class whose header uses only
engine types (`BodyHandle`, `Pose`, `ShapeDesc`, `Material`, `Motion`). Its
Jolt members live in a private struct defined in the `.cpp` (the "pimpl"
idiom), and its library links Jolt privately. The ECS system only translates
components into calls on it.

`JoltRuntime`, Jolt's process-wide setup, is owned by `Game` like `BgfxContext`
and passed to the world, so a world can't exist without it.

## Alternatives considered

- **A virtual interface** (`IPhysicsBackend`) with Jolt as one implementation.
  Rejected: with a single backend it adds indirection and buys nothing a
  concrete class doesn't. A fake backend for tests would test little, since the
  sync's correctness depends on how Jolt really behaves.
- **Leave Jolt in the system**, just split into more functions. Cheaper, but
  keeps Jolt everywhere and keeps physics untestable.

## Consequences

- Only `src/physics/*.cpp` sees Jolt; the compiler enforces it.
- Jolt-specific rules (wake-ups, layers, shape building) live in one file.
- Physics and its ECS system build without bgfx, which made `physics_tests`
  possible.
- Each new physics feature needs a method on the facade as well as a component
  field: one extra hop compared with calling Jolt directly.

## Revisit when

- A second physics backend is genuinely needed (that's when an interface pays).
