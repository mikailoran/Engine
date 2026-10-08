# Physics

This engine simulates rigid bodies with [Jolt Physics](https://github.com/jrouwe/JoltPhysics)
(v5.6.0). This document explains how Jolt is wired into the engine, why it is
wired that way, and the surprises we hit along the way, so you don't have to
rediscover them.

If you only want to *use* physics, read [Using physics](#using-physics) and stop
there. The rest is for changing it.

Decisions that shaped this design, with the alternatives we turned down, are
recorded in [`docs/adr/`](adr/README.md).

---

## Using physics

Physics works entirely through ECS components. You never call Jolt.

| You want | Give the entity |
|---|---|
| Something that blocks other things (floor, wall, prop) | `Transform` + `Collider` |
| Something that falls, bounces and gets pushed | `Transform` + `Collider` + `RigidBody` |
| Level geometry from a model (static only) | `Transform` + a mesh `Collider` |

That's it. The physics system notices the components and creates, updates and
destroys the matching body by itself, every frame.

**In a scene file**, a static box the size of its unit mesh is just:

```json
"collider": {}
```

The defaults (a box with half-extents 0.5, or a sphere of radius 0.5) fit the
engine's unit primitive meshes: cube, cylinder, cone and sphere all span
`[-0.5, 0.5]`. Sizes are in the entity's own space; `Transform::scale` is
applied on top. More options:

```jsonc
"collider": {
  "shape": "sphere",        // "box" (default), "sphere" or "mesh"
  "radius": 0.5,            // sphere only
  "half_extents": [1, 1, 1],// box only
  "mesh": "assets/meshes/level.bin", // mesh only, and required for it
  "offset": [0, 0.5, 0],    // shape centre relative to the entity
  "restitution": 0.6,       // bounciness, 0 to 1
  "friction": 0.2           // 0 or more
}
```

Physics never looks at an entity's `Renderable`. If a mesh isn't centred on
its origin (the bunny sits on top of it), fit a box to it once when you create
the entity:

```cpp
ecs.AddComponent(entity, BoxColliderAround(assets.GetMeshBounds(mesh)));
```

### Mesh colliders

For level geometry, a collider can use the triangles of a compiled mesh file,
the same `.bin` the renderer draws or a separate, simpler one:

```json
"collider": { "shape": "mesh", "mesh": "assets/meshes/placeholder_shell.bin" }
```

- **Static only.** Jolt can't compute a mass for a triangle mesh (it has no
  volume), and two meshes can't collide with each other. A `RigidBody` on a
  mesh entity is ignored. Moving props use boxes and spheres; a convex hull
  shape is the planned answer for odd-shaped ones.
- **One-sided.** Jolt's simulation ignores a triangle's back face, so things
  pass through a mesh from behind. Triangles are counter-clockwise seen from
  the front, as glTF, Blender and the renderer wind them; model closed
  geometry with outward faces and it just works.
- **Built once per file.** `AssetRegistry::LoadCollisionMesh` reads the
  triangles and builds them in the `PhysicsWorld`, which owns the result.
  Every collider naming that file shares it, each at its own scale, and
  rescaling never rebuilds it.
- **Not drawn by debug draw yet**, and the inspector shows the shape
  read-only.

### Things that may surprise you

- **Editing a moving body's `Transform` teleports it.** While a body is
  dynamic, physics owns its position, rotation and velocity, and writes them
  back every frame. Changing them from outside (the inspector, gameplay code)
  is treated as "put it here now", not as a force.
- **Bounciness takes the higher of the two surfaces.** A ball with restitution
  0 still bounces on a floor with 0.6. To make something land dead, lower the
  floor's restitution too.
- **Friction is the geometric mean** (`sqrt(a * b)`), so one frictionless
  surface makes every contact with it frictionless.
- **Slow impacts never bounce.** Below 1 m/s, Jolt ignores restitution. That is
  what lets bodies come to rest instead of jittering.
- **Resting bodies sink up to 2 cm** into what they rest on. Jolt allows that
  overlap on purpose to keep contacts stable.
- **Box edges are rounded by 5 cm** (Jolt's "convex radius"), and as a result
  a box collider is never thinner than 10 cm. See [Known issues](#known-issues).
- **Spheres can't stretch.** A sphere on a non-uniformly scaled entity uses the
  largest scale axis.
- **Mesh colliders block from the front only,** and never move. See
  [Mesh colliders](#mesh-colliders).

---

## How it fits together

```
 ECS components                  ECS system                  Physics module (src/engine/physics/)
 ───────────────                 ──────────                  ────────────────────────────────────
 Transform  ┐                                                PhysicsWorld   ← the simulation
 Collider   ├─ authored ──────►  PhysicsSystem  ──calls──►   (engine types only;
 RigidBody  ┘                    (translates, every frame)    Jolt hidden inside)
 PhysicsLink ◄── runtime, written only by the system         JoltRuntime    ← Jolt's global setup
```

There are three layers, and each one only knows about the one below it.

1. **Components** say what you *want*. `Collider` holds a shape and a
   material, `RigidBody` makes the body dynamic and holds its velocity,
   acceleration and gravity switch. `PhysicsLink` is the odd one out: it's
   runtime state (see below) and you never write it.

2. **`PhysicsSystem`** (`src/engine/ecs/systems/physics_system.*`) translates between
   the ECS and the physics world. It has no physics knowledge of its own. Each
   frame it runs these steps, in order:

   1. **Sweep orphans.** Destroy any body whose entity is gone, or whose entity
      id has been reused by a new entity.
   2. **Detach.** If an entity lost its `Collider` or `Transform`, destroy its
      body and remove its link.
   3. **Attach.** Create a body for every new `Collider` + `Transform`.
   4. **Push edits.** Compare each entity's components with the copies stored
      in its `PhysicsLink`. Anything different was edited outside physics, so
      send it to the world.
   5. **Step.** Advance the world in fixed 1/60 s steps, carrying any leftover
      time to the next frame (capped at 0.25 s so a long hitch can't queue
      hundreds of steps).
   6. **Pull results.** Copy dynamic bodies' new position, rotation and
      velocity back into `Transform` and `RigidBody`.

3. **`PhysicsWorld`** (`src/engine/physics/physics_world.*`) is the simulation, behind
   an API that uses only engine types: `BodyHandle`, `Pose`, `ShapeDesc`,
   `Material`, `Motion`. Jolt never appears in its header. It owns the Jolt
   world and everything Jolt needs while stepping, and it encodes every Jolt
   rule we learned the hard way (wake-ups, layers, shape building, unit
   conversions).

### The link between an entity and its body

Each linked entity has a `PhysicsLink`. It holds the body's handle and a copy of
`Transform`, `Collider` and `RigidBody` as they were last synced. Those copies
are how the system spots edits: if your `Collider` no longer matches the copy,
you changed it.

The link is **opaque**: its data is private and only `PhysicsSystem` (a
`friend`) can create or change one. Everyone else can read `Body()`. The
compiler enforces this.

Each body also stores its entity, as Jolt "user data". That back-reference is
what makes the sync safe without any help from the ECS:

- An entity is destroyed → its link disappears with it → the sweep finds a body
  nobody links to and destroys it.
- An entity id is reused → the new entity has no link yet → same thing.
- A link is copied onto another entity → the body's stored entity doesn't match
  → the copy is dropped and the entity gets its own body.

### Edits change bodies in place

When you resize an entity or add or remove its `RigidBody`, the existing body
is reshaped (`SetShape`) or switched between static and dynamic
(`SetMotionType`), rather than destroyed and rebuilt. The body keeps its
handle, velocity, spin and contacts. For a static body to be able to become
dynamic later, every body is created with Jolt's `mAllowDynamicOrKinematic`.

---

## Jolt behaviours we depend on

### Sleeping bodies need waking

Jolt puts bodies that have stopped moving to sleep, and a sleeping body costs
nothing. It wakes when another body touches it, or when its velocity is set.

**It does not wake when what it rests on changes.** Remove a floor, move it,
shrink it or change its friction, and the bodies sleeping on it stay put, in
mid-air. Changing a body's gravity factor doesn't wake it either.

`PhysicsWorld` handles this in one place: destroying, moving, reshaping or
changing the material of a body wakes everything touching it (before and after
the change), and turning gravity on wakes the body. `physics_tests` has one test
per case, and each fails if its wake-up is removed.

### Layers

Jolt filters collisions in two stages. Every body has an **object layer**
(ours: `kMoving`, `kNonMoving`), and object layers map to **broad-phase layers**
(separate acceleration trees, so the static tree rarely needs rebuilding). We
use Jolt's ready-made table classes (`src/engine/physics/layers.*`): moving bodies
collide with everything, static bodies never test against each other.

### Global setup

Jolt has process-wide state: an allocator, a factory and a type registry.
`JoltRuntime` sets it up in its constructor and tears it down in its
destructor. It must exist before any other Jolt object and outlive all of
them, and there may only be one (a second one asserts, or throws in Release).
`Engine` owns it right after bgfx, and passes it to `PhysicsWorld`.

### Capacity limits

The world is created for `kMaxEntities` (5000) bodies, 65,536 body pairs and
10,240 contact constraints, with a 10 MB scratch allocator. Jolt silently drops
pairs or contacts past those limits; a Debug build asserts when that happens.

---

## Rotations

`Transform::rotation` is a **unit quaternion**, applied as v' = q·v·q\*. That's
the same convention as `bx::mul(Vec3, Quaternion)`, Jolt and glTF, so physics
copies rotations straight to and from Jolt.

Euler angles only appear at the edges: `rotation_deg` in scene files and the
inspector (both in degrees). They're converted with `EulerToQuat` and
`QuatToEuler` in `src/engine/math/rotation.h`.

bx has three traps here, all pinned down by `math_tests`:

1. **bx multiplies a row vector by its matrices** (`v * M`), which applies the
   *inverse* of a rotation matrix. That's why `ModelMatrix` builds its matrix
   from the quaternion's conjugate.
2. **`bx::fromEuler` returns the inverse** of the rotation `bx::mtxSRT` draws,
   and **`bx::toEuler` is not the inverse of `bx::fromEuler`**. Use our
   `EulerToQuat` / `QuatToEuler` instead.
3. **`bx::mtxFromQuaternion(m, q, translation)` stores the translation of the
   inverse matrix** (it's meant for cameras). For a model matrix, use the
   rotation-only overload and write the position yourself.

These were found by measuring, not by reading documentation. Getting them wrong
draws objects in a different orientation from their colliders.

---

## Build setup

Jolt is fetched with CMake's `FetchContent` and built from source. A few of its
options are forced in `CMakeLists.txt`:

| Setting | Why |
|---|---|
| `INTERPROCEDURAL_OPTIMIZATION OFF` | Link-time optimisation on Jolt alone breaks the Release link of an engine built without it. |
| `JPH_USE_VK OFF`, `JPH_USE_CPU_COMPUTE OFF` | Only Jolt's hair simulation uses them, and Vulkan would pull in shader compilation. |
| `-fno-rtti`, project-wide | Jolt builds without RTTI, and mixing the two breaks linking when you subclass a Jolt class. See [ADR 0002](adr/0002-no-rtti.md). |
| `SYSTEM` on the fetch | Warnings and lint checks ignore Jolt's headers. |

Never define `JPH_*` macros yourself. The `Jolt` CMake target exports them, and
if the engine's copy differs from Jolt's, `RegisterTypes()` fails a version
check at startup.

**Jolt stays inside `src/engine/physics/`.** The `engine_physics` library links Jolt
privately, so no other part of the engine even gets Jolt's include paths. A
Jolt include anywhere else fails to compile, which is the point. Inside the
module, every `.cpp` includes `<Jolt/Jolt.h>` first (Jolt's other headers
depend on it), marked `// IWYU pragma: keep` so the include checker leaves it
alone.

| Library | Contents |
|---|---|
| `engine_physics` | `src/engine/physics/`: `PhysicsWorld`, `JoltRuntime`, layers, shared shape types |
| `engine_physics_system` | `PhysicsSystem`, the ECS side |
| `engine_math` | Rotation conventions |

---

## Testing

```bash
ctest --test-dir build/Debug
./build/Debug/tests/physics_tests --gtest_filter='Physics.Ramp*'
```

Physics needs no window and no graphics, so `physics_tests` runs the real
`PhysicsSystem` and Jolt end to end in well under a second. Each test builds a
`PhysicsHarness` (runtime, world, ECS and system, in the same order as `Engine`)
plus whatever scene pieces it needs, with values copied from
`assets/scenes/debug.json`.

The tests cover body lifetime (destruction, reused ids, copied links, no
leaks), the simulation (bouncing, friction, walls, rolling spheres) and every
wake-up rule. The most important one is `RampContactMatchesTheRenderedTilt`: it
drops a box on the tilted ramp and checks it lands on the surface as the
renderer draws it. It fails if the rotation convention between rendering and
physics ever drifts apart.

**When you learn a new rule the hard way, add a test that fails without it.**

---

## Upgrading Jolt

Most of what's written here can quietly break on a Jolt upgrade. Checklist:

1. Read the release notes, and Jolt's `Build/CMakeLists.txt` for new options
   that default to on (compute backends were one).
2. Update the URL and `URL_HASH` in `CMakeLists.txt`.
3. Don't add any `JPH_*` defines by hand.
4. Build both Debug **and** Release; the LTO problem only showed in Release.
5. Run `physics_tests`. The wake-up tests will tell you if Jolt changed how
   sleeping works; the ramp test catches convention changes.
6. Recheck the Jolt features we lean on: the layer table classes,
   `mAllowDynamicOrKinematic`, `GetBodies`/`BodyIDVector`, body user data.

---

## Known issues

- **Boxes are at least 10 cm thick.** `PhysicsWorld` grows each half-extent to
  cover Jolt's 5 cm rounded edges instead of shrinking the rounding for thin
  boxes, so a thin platform collides thicker than it looks.
- **The inspector's rotation can jump near ±90° pitch.** It shows Euler angles
  derived from the quaternion; at gimbal lock the X and Z numbers can flip while
  you drag. The rotation itself stays correct.
- **The scene loader isn't covered by tests.** Its loaders reach the asset
  registry, which needs bgfx. That includes reading a mesh collider's
  triangles from its file.
- **Debug draw skips mesh colliders.** A level-sized wireframe would hide the
  scene; drawing through Jolt's own debug renderer, filtered to highlighted
  entities, is the planned fix.
- **A mesh file used for both rendering and collision is read twice,** the
  second time with a throwaway GPU upload. It costs milliseconds at level load;
  building collision data at build time is the fix if load times grow.
- **The per-frame sync takes a few Jolt locks per body.** Negligible today; a
  TODO in `physics_world.cpp` notes the lock-free alternative if it ever shows
  up in a profile.
