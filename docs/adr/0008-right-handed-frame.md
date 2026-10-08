# 0008: Use a right-handed, +Y up coordinate frame

**Status:** accepted

## Context

The engine was left-handed only because bx's matrix helpers (`mtxLookAt`,
`mtxProj`) default to `Handedness::Left`. bgfx itself has no handedness.
Everything around the engine is right-handed:

- glTF (by its spec), and Blender's glTF export;
- `.obj` files, by convention;
- Jolt's triangle winding: its simulation ignores triangle back faces, and a
  triangle's front follows the right-hand rule.

So data crossing into the engine had to be converted. geometryc mirrored every
mesh, and the planned triangle-mesh colliders would need their winding
reversed for Jolt. M2 and M3 add more such edges: a glTF importer that reads
node transforms, and an editor showing positions next to Blender's. Each edge
is a place for a sign mistake.

## Decision

Right-handed, +Y up, the frame glTF uses. Cameras look along local −Z with +X
to the right and +Y up.

- `kHandedness` in `ecs/components/camera.h` feeds `mtxLookAt` and `mtxProj`.
- `YawPitchToQuat`: zero looks along −Z, positive yaw turns left
  (counter-clockwise seen from above), positive pitch looks up.
- geometryc compiles with `RH_UP_Y`, so meshes keep their source frame and
  winding.
- `Renderable::state` defaults to `BGFX_STATE_DEFAULT`, which culls clockwise:
  front faces are counter-clockwise on screen.

## Alternatives considered

- **Stay left-handed and convert at each edge.** No migration, and the edges
  existing today were cheap (a geometryc default, one winding swap for Jolt).
  But the edges were about to multiply, and the editor would show coordinates
  whose Z sign differs from Blender's for the same object.

## Consequences

- Meshes, Jolt and future glTF data share one frame; nothing is mirrored.
- `debug.json` and the physics tests' ramp were mirrored on Z, so the scene
  looks as before.
- **The mesh path never used `BGFX_STATE_DEFAULT` before.** `Renderable::state`
  was `BGFX_STATE_MASK`, so `meshSubmit` substituted its own state, which culls
  counter-clockwise. That only suited the mirrored meshes.
- `math_tests` pins the frame: local +X lands on the right of the screen, +Y at
  the top, −Z ahead.
- bgfx's `debugdraw` builds its solid shapes for its left-handed examples, so
  the solid cone heads of velocity arrows are probably culled from the wrong
  side. Lines and wireframes are unaffected.

## Revisit when

- Never for the frame itself. Fix the debugdraw solids with
  `DebugDrawEncoder::setState` (flip `_clockwise`) when they matter.
