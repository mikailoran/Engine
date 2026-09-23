#pragma once

#include <bx/math.h>

struct Transform {
  bx::Vec3 position{0.0f};
  bx::Vec3 rotation{0.0f};
  bx::Vec3 scale{1.0f};
};