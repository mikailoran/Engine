#pragma once

#include <bx/math.h>

// Rotations are unit quaternions applied as v' = q v q*, the convention of
// bx::mul(Vec3, Quaternion), Jolt and glTF. Euler angles appear only at the
// edges (scene files, the inspector) and use bx::mtxSRT's angle order.

/** @brief Converts Euler angles in radians to the rotation mtxSRT draws. */
auto EulerToQuat(const bx::Vec3& euler) -> bx::Quaternion;

/**
 * @brief Converts a rotation to Euler angles in radians that EulerToQuat maps
 * back to it. Y is within [-pi/2, pi/2]; X and Z within [-pi, pi].
 */
auto QuatToEuler(const bx::Quaternion& rotation) -> bx::Vec3;
