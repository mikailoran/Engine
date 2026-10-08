#pragma once

#include <bx/bounds.h>
#include <bx/math.h>

#include <array>
#include <cassert>

#include "engine/ecs/components/transform.h"
#include "engine/platform/screen.h"

namespace engine {

/**
 * @brief Lens of a camera entity. Its Transform is the pose: the camera looks
 * along local +Z with local +Y up, and scale is ignored.
 */
struct Camera {
  // Vertical field of view
  float fov_degrees{60.0F};

  // Near clip distance. Geometry closer than this is not drawn.
  float near_plane{0.1F};

  // Far clip distance.
  float far_plane{100.0F};
};

/** @brief World-to-view matrix for a camera posed by @p transform. */
inline auto ViewMatrix(const Transform& transform) -> std::array<float, 16> {
  const bx::Vec3 forward =
      bx::mul(bx::Vec3{0.0F, 0.0F, 1.0F}, transform.rotation);
  const bx::Vec3 up = bx::mul(bx::Vec3{0.0F, 1.0F, 0.0F}, transform.rotation);

  // Rotated +Y as up, so it is never parallel to forward
  std::array<float, 16> view{};
  bx::mtxLookAt(view.data(), transform.position,
                bx::add(transform.position, forward), up);
  return view;
}

/**
 * @brief Perspective projection for @p camera.
 * @param aspect Viewport width over height.
 * @param homogeneous_depth True for NDC depth -1..1, false for 0..1; pass
 *        bgfx::getCaps()->homogeneousDepth when rendering.
 */
inline auto ProjectionMatrix(const Camera& camera, float aspect,
                             bool homogeneous_depth) -> std::array<float, 16> {
  std::array<float, 16> proj{};
  bx::mtxProj(proj.data(), camera.fov_degrees, aspect, camera.near_plane,
              camera.far_plane, homogeneous_depth);
  return proj;
}

/**
 * @brief Casts a world-space ray from a camera through a window pixel.
 * @param transform The camera's pose.
 * @pre @p size has positive area.
 */
inline auto ScreenPositionToRay(const Camera& camera,
                                const Transform& transform,
                                ScreenPosition point, ScreenSize size)
    -> bx::Ray {
  assert(size.width > 0.0F && size.height > 0.0F && "window has no area");

  // Pixel to NDC; screen y grows down, NDC y up
  const float x_ndc = (2.0F * point.x / size.width) - 1.0F;
  const float y_ndc = 1.0F - (2.0F * point.y / size.height);

  // makeRay unprojects NDC depth 0 and 1, so use 0..1 whatever the backend
  const auto view = ViewMatrix(transform);
  const auto proj = ProjectionMatrix(camera, size.width / size.height, false);
  std::array<float, 16> view_proj{};
  std::array<float, 16> inv_view_proj{};
  bx::mtxMul(view_proj.data(), view.data(), proj.data());
  bx::mtxInverse(inv_view_proj.data(), view_proj.data());
  return bx::makeRay(x_ndc, y_ndc, inv_view_proj.data());
}

}  // namespace engine
