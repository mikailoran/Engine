# 0001: Use Jolt Physics

**Status:** accepted

## Context

The engine had hand-written physics: gravity, a fixed-step integrator and a
hard-coded floor. Nothing collided with anything else. We needed real rigid-body
collision (boxes, spheres, stacking, friction, bounce) in a C++20, CMake-only,
Linux project.

## Decision

Use [Jolt Physics](https://github.com/jrouwe/JoltPhysics), fetched and built
from source with CMake's `FetchContent`.

## Alternatives considered

- **PhysX 5.** Mature, but heavy, and it uses its own project generator and
  package manager rather than plain CMake, so it doesn't fit `FetchContent`.
- **Bullet 3.** Works, but its API is dated, its build drags in many extras and
  development has largely stalled.
- **ReactPhysics3D.** Small and easy to build, but slower and with fewer
  features.
- **Keep extending our own.** Fine for a few boxes; not for stacking, rotation
  from impacts or many bodies.

## Consequences

- Modern C++, plain CMake, MIT licensed, actively maintained, used in shipped
  games.
- It brings its own global setup, threading and memory allocation, which the
  engine has to own carefully (see `JoltRuntime` and
  [0003](0003-physics-behind-a-facade.md)).
- Some of its build options needed changing: no link-time optimisation, no
  compute backends, and the RTTI question ([0002](0002-no-rtti.md)).

## Revisit when

- The project needs something Jolt doesn't do well (it is rigid-body focused).
- Jolt stops being maintained.
