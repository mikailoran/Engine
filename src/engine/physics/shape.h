#pragma once

#include <bx/math.h>

#include <cstdint>

namespace engine::physics {

// Shared by Collider (authoring) and PhysicsWorld (simulation), so neither
// keeps its own copy. Jolt-free, like the rest of this module's headers.

/** @brief The kind of a body's shape. */
enum class ShapeKind : std::uint8_t { kBox, kSphere };

/**
 * @brief A shape in body space, before the body's scale. The defaults fit the
 * engine's unit primitive meshes (cube, cylinder, cone, sphere), which all
 * span [-0.5, 0.5].
 */
struct ShapeDesc {
  ShapeKind kind{ShapeKind::kBox};
  /// kBox only: half the box's size on each axis, in m.
  bx::Vec3 half_extents{0.5F};
  /// kSphere only: radius in m; scaled by the largest scale axis.
  float radius{0.5F};
  /// Shape centre relative to the body's origin, in m.
  bx::Vec3 offset{0.0F};
};

/** @brief How a body's surface responds to contact. */
struct Material {
  /// Bounciness from 0 to 1. A contact uses the higher of its two bodies'.
  float restitution{0.6F};
  /// Sliding resistance, 0 or more. A contact uses sqrt(a * b) of its pair's.
  float friction{0.2F};
};

}  // namespace engine::physics
