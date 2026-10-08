#include "engine/math/rotation.h"

#include <bx/math.h>

#include <cmath>

namespace engine {

namespace {

/// Below this cos(y), X and Z rotate about the same axis (gimbal lock).
constexpr float kGimbalEpsilon = 1e-6F;

}  // namespace

auto EulerToQuat(const bx::Vec3& euler) -> bx::Quaternion {
  // fromEuler gives the inverse of the rotation mtxSRT draws
  return bx::conjugate(bx::fromEuler(euler));
}

auto QuatToEuler(const bx::Quaternion& rotation) -> bx::Vec3 {
  // bx::toEuler does not invert fromEuler, so read the angles off the rotated
  // axes, matching mtxSRT's matrix terms
  const bx::Vec3 x_axis = bx::mul(bx::Vec3{1.0F, 0.0F, 0.0F}, rotation);
  const bx::Vec3 y_axis = bx::mul(bx::Vec3{0.0F, 1.0F, 0.0F}, rotation);
  const bx::Vec3 z_axis = bx::mul(bx::Vec3{0.0F, 0.0F, 1.0F}, rotation);
  const float sin_y = -z_axis.x;
  const float cos_y = std::hypot(x_axis.x, y_axis.x);
  const float y = std::atan2(sin_y, cos_y);
  // Gimbal lock: only X + Z is defined, so fold it all into X
  if (cos_y < kGimbalEpsilon) {
    return {std::atan2(sin_y * x_axis.y, y_axis.y), y, 0.0F};
  }
  return {std::atan2(z_axis.y, z_axis.z), y, std::atan2(y_axis.x, x_axis.x)};
}

auto YawPitchToQuat(YawPitch angles) -> bx::Quaternion {
  // Pitch about local X, then yaw about world Y; +X tips +Z down, so negate
  const bx::Quaternion yaw_q =
      bx::fromAxisAngle({0.0F, 1.0F, 0.0F}, angles.yaw);
  const bx::Quaternion pitch_q =
      bx::fromAxisAngle({1.0F, 0.0F, 0.0F}, -angles.pitch);
  return bx::mul(yaw_q, pitch_q);
}

}  // namespace engine
