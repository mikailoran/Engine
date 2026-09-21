#pragma once

#include <bx/math.h>

struct RigidBody {
  bx::Vec3 velocity_{0.0f};
  bx::Vec3 acceleration_{0.0f};
};