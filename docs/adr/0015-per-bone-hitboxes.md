# 0015: Per-bone convex hitboxes for characters

**Status:** superseded by [0017](0017-ragdoll-hitboxes.md)

## Context

Hitscan weapons find their target with `Engine::CastRay`, which tested each
character's movement capsule ([0009](0009-kinematic-character.md)). The
enemy, Quaternius' `Male_Casual.glb`, stands in a T-pose 1.82 m across the
arms, around a 0.3 m capsule. Shots at its arms passed through, and a shot
just beside its body could still hit the capsule.

The mesh is skinned: 31 joints, with `JOINTS_0`/`WEIGHTS_0` on every vertex.
It isn't animated yet, but will be.

## Decision

A character can carry a **hitbox**: one convex hull per bone, in bind pose,
hit only by ray casts.

- `ReadGltf` records each vertex's **heaviest joint** in `CpuMesh::bones`.
  Unskinned vertices record the node holding their mesh, so a prop made of
  several Blender objects gets one hull per object.
- `SplitByBone` groups positions by bone. `AssetRegistry::LoadHitbox` hands
  the groups to `PhysicsWorld::CreateHitbox`, which builds a hull per group
  into a static compound. The world owns it, and characters share it like
  collision meshes. Groups that enclose no volume are skipped.
- `CharacterBody::hitbox` names it (scene key `"hitbox"`, a `.glb` path).
  The hitbox stands at the character's feet. `PhysicsSystem` pushes the
  `Transform`'s rotation and scale to it every frame. The capsule still does
  all the moving.
- `PhysicsWorld::CastRay` tests a character's hitbox when it has one,
  otherwise its capsule.

## Alternatives considered

- **One hull around the whole mesh.** Simplest, but in a T-pose it fills the
  wedge between each arm and the hips, so shots there still hit.
- **The triangle mesh itself** (`MeshShape`, as level collision uses). Exact
  and already built for levels, but one-sided, and a single rigid shape can't
  follow bones once the character animates.
- **A capsule or box per bone,** fitted by hand or from the skeleton. These
  are cheaper to test than hulls, but need fitting data the file doesn't
  have. Hulls fit the mesh automatically.
- **A kinematic Jolt body per bone,** each an entity under a hierarchy. Rays
  would go through the narrow phase like any body, and props would collide
  with characters. But it means 31 bodies and entities per character, an
  object layer to keep the capsule off its own bones, and an entity hierarchy
  the ECS doesn't have yet.

## Consequences

- **Bind pose only.** The hulls don't move with animation; each keeps its
  bone's node index (`BonePart::bone`) for when they must.
- **Bones with too few vertices get no hull.** On `Male_Casual.glb`, 26 of 27
  parts build. `Hips` owns 4 flat vertices, and the abdomen and leg hulls
  cover most of that area.
- **Hulls overlap at joints and over-cover concave parts,** such as between
  fingers. That's close enough for hitscan.
- **A hit names the character, not the bone,** so there is no headshot
  multiplier yet.
- **Only rays see hitboxes.** Bodies still pass through characters
  ([0009](0009-kinematic-character.md)).
- The `.glb` is read once more at load: for rendering, and again for the
  hitbox. It isn't drawn by debug draw.
- `ray_cast_test.cpp` covers the facade, and `physics_system_test.cpp`
  covers the ECS sync. `asset_tests` covers bone reading and splitting on the
  real mesh.

## Revisit when

- **Characters animate:** switch to a `MutableCompoundShape` and pose each
  hull from its bone's joint matrix every frame.
- **Damage depends on where a shot lands:** report the hit sub-shape and map
  it to `BonePart::bone`.
- **Props must collide with characters:** compare per-bone bodies with
  Jolt's inner character body (ADR 0009's revisit).
