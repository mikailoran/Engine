#pragma once

#include <bx/bounds.h>
#include <bx/math.h>

#include <array>
#include <cassert>

#include "platform/screen.h"

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

/**
 * @brief Casts a world-space ray from the camera through a window pixel.
 * @pre @p size has positive area; view and proj are current.
 */
inline auto ScreenPositionToRay(const Camera& camera, ScreenPosition point,
                                ScreenSize size) -> bx::Ray {
  assert(size.width > 0.0F && size.height > 0.0F && "window has no area");

  // Pixel to NDC; screen y grows down, NDC y up
  const float x_ndc = (2.0F * point.x / size.width) - 1.0F;
  const float y_ndc = 1.0F - (2.0F * point.y / size.height);

  std::array<float, 16> view_proj{};
  std::array<float, 16> inv_view_proj{};
  bx::mtxMul(view_proj.data(), camera.view.data(), camera.proj.data());
  bx::mtxInverse(inv_view_proj.data(), view_proj.data());
  return bx::makeRay(x_ndc, y_ndc, inv_view_proj.data());
}
