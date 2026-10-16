# 0017: One per-bone ragdoll for hitboxes and death

**Status:** accepted 2026-10-16. Supersedes [0015](0015-per-bone-hitboxes.md).

## Context

ADR 0015 gave characters a hitbox of convex hulls, one per bone, standing
in bind pose. It said to revisit once characters animate: the hulls didn't
move with the limbs. Characters now animate (ADR
[0016](0016-ozz-animation-runtime.md)), and dying characters should fall as
ragdolls. Both need rigid shapes that follow the skeleton's bones.

## Decision

Each character has one Jolt ragdoll, a capsule per major bone, that does
both jobs.

- A ragdoll file per rig (`assets/ragdolls/<rig>.json`) names its parts by
  joint, with an end joint or length, radius, mass, parent and swing and
  twist limits. `FitRagdoll` fits the capsules to the rig's rest pose;
  `PhysicsWorld::CreateRagdollSettings` builds them once per rig.
- While alive the ragdoll is **kinematic**: after animation,
  `PhysicsSystem::PoseRagdolls` teleports each part to its joint, so ray
  casts hit the limbs where they are drawn, and props bump off them. The
  character's capsule still does all the moving but is no ray target.
- On death gameplay sets `Ragdoll::simulate`: the parts turn dynamic with
  the velocity they last moved at, and `AnimationSystem` poses the skeleton
  from them, every other joint keeping its last local transform.
- Parts live on their own object layer, which characters ignore, so a
  character never trips on its own limbs. A ragdoll's parts never collide
  with each other.

The convex-hull hitbox code (`CreateHitbox`, `LoadHitbox`, `SplitByBone`,
`CpuMesh::bones`) is removed.

## Alternatives considered

- **Pose ADR 0015's hulls every frame** in a `MutableCompoundShape`, and
  build a ragdoll only at death. Hulls fit the mesh more tightly than
  capsules, but that is two sets of per-bone shapes to keep in step, and
  the ragdoll would start from a different shape than the one being shot.
- **Per-bone bodies as entities**, as 0015 considered. Still 11+ entities
  per character and an entity hierarchy; a Jolt ragdoll is one object
  inside the physics module.
- **Fitting capsules from the mesh's vertices.** Automatic, but a radius
  from skin weights is noisy at joints; a few authored numbers per rig are
  easy to tune with debug draw.

## Consequences

- Capsules cover the arms, legs, chest, hips and head; hands and feet have
  none, so shots there miss.
- Ray hits name the ragdoll part (`physics::RayHit::part`), ready for
  per-bone damage; `Engine::CastRay` doesn't pass it on yet.
- Characters walk through ragdolls, their own and dead ones. Ragdolls
  assume their entity's scale is 1.
- Kinematic parts are teleported, not moved, so props they touch are pushed
  out but get no momentum from a swinging limb.
- `tests/physics/ragdoll_test.cpp` covers the facade and
  `physics_system_test.cpp` the ECS sync; `ragdoll_fit_test.cpp` the
  fitting.

## Revisit when

- Hits need to be tighter than capsules (hands, weapons): hulls per part,
  or extra parts.
- Dead bodies should block characters, or kinematic limbs should hit props
  with momentum (`MoveKinematic` instead of teleporting).
- Ragdolls should blend back into animation (getting up), or react to hits
  while alive (powered ragdolls driven to the pose).
