# 0004: Sync the ECS and physics by comparing, not with ECS hooks

**Status:** accepted

## Context

Two systems keep their own state: the ECS (components) and Jolt (bodies). They
must agree. Our ECS can't tell anyone when an entity is destroyed or a
component removed, and entity ids get reused.

## Decision

`PhysicsSystem` reconciles every frame:

- Each linked entity has a **`PhysicsLink`** component holding its body's
  handle and copies of its components as last synced. A component that differs
  from its copy was edited, and the change is pushed to the body.
- Each body stores its entity as Jolt user data. A sweep over all bodies
  destroys any whose entity no longer links to it, which covers destroyed
  entities, reused ids and copied links.
- `PhysicsLink` is opaque: private data, `friend class PhysicsSystem`, a
  read-only `Body()`. Only the system can create or change one.

## Alternatives considered

- **ECS hooks** (callbacks or event logs on add, remove and destroy, like
  EnTT's signals or Bevy's `RemovedComponents`). They would remove the sweep,
  but creating bodies and pushing edits would still need a per-frame pass. They
  also need changes to the dependency-free ECS core, and immediate callbacks
  would run physics code in the middle of other systems' updates.
- **Change tracking** (marking components as changed when written). Would
  remove the comparisons, but every write path would have to remember to mark
  them, and a forgotten mark is a silent desync.
- **An edit API** (the inspector calls `physics.SetVelocity(...)`). Removes the
  comparisons but ties the UI to physics.
- **A map inside the system** (entity → body). What we had first; it went stale
  when entity ids were reused, and duplicated the link.

## Consequences

- Correct by construction, with no changes to the ECS core.
- Every frame touches every body (three uncontended Jolt locks each). Fine at
  the current scale; see "Revisit when".
- The UI and gameplay code don't know physics exists: they just write
  components.

## Revisit when

- **A second system needs to react to entity or component removal** (audio,
  networking, a scene hierarchy). Then add deferred removal logs to the ECS,
  and replace the sweep with a drain of the log.
- **The sync shows up in a profile.** First try reading through Jolt's
  `GetBodyInterfaceNoLock()` (a TODO marks the spot in `physics_world.cpp`);
  then consider change tracking.
