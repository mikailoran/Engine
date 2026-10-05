#pragma once

#include <bx/bounds.h>
#include <bx/math.h>

#include <cstdint>

/** @brief The kind of a Collider's shape. */
enum class ColliderShape : std::uint8_t { kBox, kSphere };

/**
 * @brief Gives an entity a physics body: static on its own, dynamic alongside
 * a RigidBody. Needs a Transform, whose scale applies to the shape.
 *
 * The defaults fit the engine's unit primitive meshes (cube, cylinder, cone,
 * sphere), which all span [-0.5, 0.5].
 */
struct Collider {
  ColliderShape shape{ColliderShape::kBox};

  /// kBox only: half the box's size per axis, before scale.
  bx::Vec3 half_extents{0.5F};

  /// kSphere only: radius before scale; scaled by the largest scale axis.
  float radius{0.5F};

  /// Shape centre relative to the entity's origin, before scale.
  bx::Vec3 offset{0.0F};

  /// Bounciness from 0 to 1. A contact uses the higher of its two bodies'.
  float restitution{0.6F};

  /// Sliding resistance, 0 or more. A contact uses sqrt(a * b) of its pair's.
  float friction{0.2F};
};

/** @brief A box Collider fitted around @p bounds, e.g. a mesh's. */
inline auto BoxColliderAround(const bx::Aabb& bounds) -> Collider {
  Collider collider{};
  collider.half_extents = bx::mul(bx::sub(bounds.max, bounds.min), 0.5F);
  collider.offset = bx::mul(bx::add(bounds.min, bounds.max), 0.5F);
  return collider;
}
