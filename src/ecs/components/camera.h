#pragma once

#include <bx/math.h>

#include <array>

/**
 * @brief View and projection parameters for a camera entity.
 */
struct Camera {
  // World-space point the camera looks at.
  bx::Vec3 target{0.0F, 0.0F, 1.0F};

  // World-space up axis. Y-up matches the convention used by bx::mtxLookAt
  bx::Vec3 up{0.0F, 1.0F, 0.0F};

  // Vertical field of view
  float fov_degrees{60.0F};

  // Near clip distance. Geometry closer than this is not drawn.
  float near_plane{0.1F};

  // Far clip distance.
  float far_plane{100.0F};

  // Derived, not configuration: written each frame by CameraControl.
  std::array<float, 16> view{};
  std::array<float, 16> proj{};
};
