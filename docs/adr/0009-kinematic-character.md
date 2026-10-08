# 0009: A kinematic character on Jolt's CharacterVirtual

**Status:** accepted

## Context

The game is first person: the player walks, sprints, jumps, climbs ramps up to
a slope limit and pushes physics props. A player driven by forces feels
floaty and imprecise; players expect to stop the moment they let go of a key,
never to tip over and never to bounce. Jolt offers two characters:

- `Character`: a rigid body with its rotation locked, moved by the solver;
- `CharacterVirtual`: not a body at all, but a capsule swept through the world
  with collision queries, moving exactly at the velocity it is given and
  sliding along what it hits.

## Decision

Use `CharacterVirtual`, behind the `PhysicsWorld` facade like everything else
in Jolt (ADR 0003).

- `PhysicsWorld` gains characters (`CreateCharacter`, `SetCharacterVelocity`,
  `GetCharacter`...). Each `Step` moves them before stepping the bodies, so a
  push acts in the same step.
- **Gameplay sets the velocity; physics adds gravity.** Each step drops the
  fall while the character stands on the ground and adds `gravity * dt`
  otherwise. A jump is an upward velocity. Wall hits don't reset the velocity:
  Jolt slides the capsule instead.
- The ECS side mirrors bodies (ADR 0004): `CharacterBody` is authored and
  gameplay-written, `CharacterLink` is runtime state only `PhysicsSystem`
  writes, synced by comparing against copies. The `Transform`'s position is
  the feet.
- Stair stepping is off; sticking to the floor on the way down slopes is on.

## Alternatives considered

- **Jolt's `Character` (a rigid body).** Bodies would collide with it for
  free, but its motion comes from the solver: acceleration, friction and
  bounces that feel wrong for a player and need constant correction.
- **Our own kinematic controller** from shape casts. The same idea as
  `CharacterVirtual`, minus years of edge cases (steep slopes, sticking,
  penetration recovery, pushing bodies).

## Consequences

- The character pushes dynamic bodies with an impulse capped by
  `push_force`, but **bodies don't collide with it**: a falling prop passes
  through the player, and raycasts miss it.
- Pushing is only as good as props' masses. Every body has the density of
  water, so a 1 m crate weighs a tonne and doesn't move under the default
  200 N.
- Two characters would pass through each other until character-vs-character
  collision is configured; the game has one.
- `physics_tests` covers the character through the facade and through the
  ECS, with no bgfx.

## Revisit when

- A prop falling through the player looks wrong: give the character Jolt's
  inner body (`mInnerBodyShape`), tagged so `PhysicsSystem`'s orphan sweep
  doesn't destroy it.
- The level needs stairs, or moving platforms (add the ground velocity).
- A second character appears (NPCs, multiplayer).
