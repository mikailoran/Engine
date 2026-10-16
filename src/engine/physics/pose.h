#pragma once

#include <bx/bx.h>
#include <bx/math.h>

namespace engine::physics {

/** @brief A body's position and rotation in world space. */
struct Pose {
  bx::Vec3 position{0.0F};
  bx::Quaternion rotation{bx::InitIdentity};
};

}  // namespace engine::physics
