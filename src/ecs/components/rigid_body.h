#pragma once

#include <bx/math.h>

struct RigidBody {
  bx::Vec3 velocity{0.0f};
  bx::Vec3 acceleration{0.0f};
  bool has_gravity{true};
};