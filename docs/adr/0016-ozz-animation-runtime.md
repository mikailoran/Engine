# 0016: Sample and blend skeletal animation with ozz-animation

**Status:** accepted 2026-10-16

## Context

Characters must play CC0 animation packs (Quaternius' Universal Animation
Library): looping locomotion, a walk/run blend by speed, an upper-body
aim and fire layered over running legs, and one-shots. glTF already brings
the skeleton and the keyframes ([0010](0010-gltf-runtime-format.md)), and
cgltf reads them. What's missing is the runtime: sampling keys at a time,
blending several clips with per-joint weights, and turning local joint
transforms into the model-space matrices skinning needs, every frame, for
65-joint rigs.

## Decision

Use [ozz-animation](https://github.com/guillaumeblanc/ozz-animation)
0.17.0, fetched with `FetchContent` like Jolt, behind a facade.

- `engine_animation` (`src/engine/animation/`, namespace
  `engine::animation`) links ozz **PRIVATE**, as `engine_physics` links
  Jolt. Its headers speak engine types (`Skeleton`, `AnimationClip`,
  `Mat4`), so no other target sees an ozz header.
- glTF stays the only format: clips are read by `engine_assets` and built
  into ozz's runtime form at load with ozz's offline builders
  (`RawSkeleton`, `RawAnimation`), so there is no `.ozz` file and no
  conversion step.
- Only `ozz_base`, `ozz_animation` and `ozz_animation_offline` are built
  (`EXCLUDE_FROM_ALL`, tools, samples and tests off), and ozz's
  warnings-as-errors is switched off for them.

## Alternatives considered

- **Hand-written sampling and blending.** Sampling, a crossfade and
  local-to-model are a few hundred lines. Masked layers, additive poses,
  IK (aiming, hands on a gun) and keyframe compression are where it gets
  expensive, and they are all in ozz, tested.
- **Jolt's `SkeletalAnimation`.** Already linked, but it samples one clip
  with no blending jobs; it exists to feed ragdolls.
- **A bigger animation system** (state graphs, retargeting tools). Animation
  authoring is out of scope ([objectives](../objectives.md)); game code
  decides what plays.

## Consequences

- One more fetched dependency (MIT). It builds with the project-wide
  `-fno-rtti` ([0002](0002-no-rtti.md)) and C++17.
- ozz's math is SIMD and structure-of-arrays inside the facade; matrices
  leave it in bx's memory layout, which matches ozz's `Float4x4` exactly.
- ozz orders joints depth-first; the facade remaps to the glTF skin's order,
  which the rest of the engine uses so vertex joint indices need no remap.
- Clip packs are retargeted by joint name before they reach ozz
  (`RetargetClip`): ozz has no retargeting of its own.

## Revisit when

- Characters need something ozz lacks (e.g. motion matching, procedural
  physics-driven poses beyond ragdolls).
- ozz stops being maintained, or a C++20 module-friendly alternative
  becomes the obvious choice.
