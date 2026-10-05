# 0002: Build the whole project without RTTI

**Status:** accepted

## Context

Jolt builds without RTTI (C++'s run-time type information, which powers
`dynamic_cast` and `typeid`). Our code was built with it. If a class compiled
with RTTI derives from one compiled without it, the link fails with a missing
`typeinfo` symbol. Using Jolt can mean deriving from its interfaces (its own
samples do, for collision layers), so the two sides had to agree.

## Decision

Turn RTTI off for every C++ target, dependencies included:
`add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>)` at the top of
`CMakeLists.txt`. Exceptions stay on.

## Alternatives considered

- **Build Jolt with RTTI** (`CPP_RTTI_ENABLED ON`). The smallest change and
  equally valid: standard C++ with RTTI available but unused. We started here.
- **Turn it off for the engine only.** Would have moved the mismatch to bgfx,
  which our build compiled with RTTI.

The C++ Core Guidelines and Google's style guide discourage *relying* on RTTI,
not compiling with it. Game engines often disable it for size. We chose one
consistent setting everywhere, so no library and its user can ever disagree.

## Consequences

- `dynamic_cast` and `typeid` don't compile anywhere in the project. Nothing
  used them; `TypeKey` already uses `std::source_location` instead.
- Exceptions still work (GCC and Clang keep type information for thrown types).
- googletest detects the setting and adapts by itself.

## Revisit when

- A dependency genuinely needs RTTI. That's a design decision, not a flag flip.
