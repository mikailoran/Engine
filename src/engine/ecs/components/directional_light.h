#pragma once

#include <bx/math.h>

/**
 * @brief A sun-like light at infinity, plus the scene's hemisphere ambient.
 */
struct DirectionalLight {
  // World-space direction toward the light. Need not be normalized.
  bx::Vec3 direction{-0.4F, 1.0F, -0.3F};

  // Linear RGB, scaled by intensity.
  bx::Vec3 color{1.0F, 0.95F, 0.85F};
  float intensity{1.0F};

  // Linear RGB ambient for up- and down-facing surfaces.
  bx::Vec3 sky_color{0.22F, 0.25F, 0.3F};
  bx::Vec3 ground_color{0.08F, 0.07F, 0.06F};
};
