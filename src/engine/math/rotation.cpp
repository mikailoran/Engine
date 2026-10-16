#include "engine/math/rotation.h"

#include <bx/math.h>

#include <array>
#include <cmath>
#include <cstddef>

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
  // Pitch about local X, then yaw about world Y
  const bx::Quaternion yaw_q =
      bx::fromAxisAngle({0.0F, 1.0F, 0.0F}, angles.yaw);
  const bx::Quaternion pitch_q =
      bx::fromAxisAngle({1.0F, 0.0F, 0.0F}, angles.pitch);
  return bx::mul(yaw_q, pitch_q);
}

auto RotationOf(const std::array<float, 16>& matrix) -> bx::Quaternion {
  // Each basis column, unscaled; r(row, col) is the rotation matrix
  const bx::Vec3 x = bx::normalize(bx::load<bx::Vec3>(&matrix.at(0)));
  const bx::Vec3 y = bx::normalize(bx::load<bx::Vec3>(&matrix.at(4)));
  const bx::Vec3 z = bx::normalize(bx::load<bx::Vec3>(&matrix.at(8)));
  const std::array<std::array<float, 3>, 3> r{
      {{x.x, y.x, z.x}, {x.y, y.y, z.y}, {x.z, y.z, z.z}}};
  const auto at = [&r](int row, int col) -> float {
    return r.at(static_cast<std::size_t>(row))
        .at(static_cast<std::size_t>(col));
  };

  // Shepperd's method: divide by the largest of w, x, y, z for accuracy
  bx::Quaternion q{0.0F, 0.0F, 0.0F, 1.0F};
  const float trace = at(0, 0) + at(1, 1) + at(2, 2);
  if (trace > 0.0F) {
    const float s = std::sqrt(trace + 1.0F) * 2.0F;
    q = {(at(2, 1) - at(1, 2)) / s, (at(0, 2) - at(2, 0)) / s,
         (at(1, 0) - at(0, 1)) / s, 0.25F * s};
  } else if (at(0, 0) > at(1, 1) && at(0, 0) > at(2, 2)) {
    const float s = std::sqrt(1.0F + at(0, 0) - at(1, 1) - at(2, 2)) * 2.0F;
    q = {0.25F * s, (at(0, 1) + at(1, 0)) / s, (at(0, 2) + at(2, 0)) / s,
         (at(2, 1) - at(1, 2)) / s};
  } else if (at(1, 1) > at(2, 2)) {
    const float s = std::sqrt(1.0F + at(1, 1) - at(0, 0) - at(2, 2)) * 2.0F;
    q = {(at(0, 1) + at(1, 0)) / s, 0.25F * s, (at(1, 2) + at(2, 1)) / s,
         (at(0, 2) - at(2, 0)) / s};
  } else {
    const float s = std::sqrt(1.0F + at(2, 2) - at(0, 0) - at(1, 1)) * 2.0F;
    q = {(at(0, 2) + at(2, 0)) / s, (at(1, 2) + at(2, 1)) / s, 0.25F * s,
         (at(1, 0) - at(0, 1)) / s};
  }
  return bx::normalize(q);
}

}  // namespace engine
