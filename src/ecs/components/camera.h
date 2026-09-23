#pragma once

#include <bx/math.h>

/**
 * @brief View and projection parameters for a camera entity.
 *
 * The eye position is not stored here — it lives in the same entity's
 * Transform, so that one component owns placement for every kind of entity.
 * RenderSystem builds the view matrix from that position plus @ref target and
 * @ref up, and the projection from the three remaining fields.
 *
 * Every member needs an initialiser: bx::Vec3 has a deleted default
 * constructor, and ComponentArray value-initialises a std::array of these.
 */
struct Camera {
  /// World-space point the camera looks at. Recomputed each frame by
  /// CameraControl. Defaults one unit along +Z so that a camera sitting at the
  /// origin still has a valid, non-degenerate view direction.
  bx::Vec3 target{0.0F, 0.0F, 1.0F};

  /// World-space up axis. Y-up matches the convention used by the floor quad
  /// and by bx::mtxLookAt's own default.
  bx::Vec3 up{0.0F, 1.0F, 0.0F};

  /// Vertical field of view, in degrees.
  float fov_degrees{60.0F};

  /// Near clip distance. Geometry closer than this is not drawn.
  float near_plane{0.1F};

  /// Far clip distance.
  float far_plane{100.0F};
};
