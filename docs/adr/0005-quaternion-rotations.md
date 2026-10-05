# 0005: Store rotations as quaternions

**Status:** accepted

## Context

`Transform::rotation` used to be three Euler angles, fed to `bx::mtxSRT` for
rendering. Jolt works in quaternions, so every frame physics converted Euler to
quaternion and back.

Measuring those conversions turned up three surprises in bx: `bx::fromEuler`
returns the inverse of what `mtxSRT` draws, `bx::toEuler` doesn't undo
`fromEuler`, and bx's row-vector matrices invert rotations. Euler angles also
lose precision near ±90° pitch (gimbal lock), which a tumbling body hits
constantly.

## Decision

Store a unit quaternion, applied as v' = q·v·q\* (the same convention as
`bx::mul(Vec3, Quaternion)`, Jolt and glTF). Euler angles stay only where
people type them: `rotation_deg` in scene files, and the inspector, in degrees.
`EulerToQuat` and `QuatToEuler` (`src/math/rotation.h`) convert at those edges,
and `ModelMatrix` builds from the conjugate to match bx's matrix convention.

## Alternatives considered

- **Keep Euler angles and convert at the physics boundary.** Smaller change,
  and what we had first. It works, but every frame converts twice, and the bx
  pitfalls sit in the middle of physics code.

## Consequences

- Physics copies rotations to and from Jolt with no conversion.
- `ModelMatrix` no longer calls `sin`/`cos`.
- `math_tests` pins the convention: the quaternion `ModelMatrix` reproduces the
  old `mtxSRT` output exactly, so existing scenes draw identically.
- The inspector shows angles derived from the quaternion; near gimbal lock the
  displayed X and Z can jump while dragging.

## Revisit when

- Never for storage. The inspector display could keep the angles you typed
  while dragging, if the gimbal jump becomes annoying.
