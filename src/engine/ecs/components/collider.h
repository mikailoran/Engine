#pragma once

#include <bx/bounds.h>
#include <bx/math.h>

#include "engine/physics/shape.h"

/**
 * @brief Gives an entity a physics body: static on its own, dynamic alongside
 * a RigidBody. Needs a Transform, whose scale applies to the shape.
 *
 * The default shape fits the engine's unit primitive meshes.
 */
struct Collider {
  /// The body's shape before Transform::scale.
  ShapeDesc shape;
  /// The body's surface response.
  Material material;
};

/** @brief A box Collider fitted around @p bounds, e.g. a mesh's. */
inline auto BoxColliderAround(const bx::Aabb& bounds) -> Collider {
  Collider collider{};
  collider.shape.half_extents = bx::mul(bx::sub(bounds.max, bounds.min), 0.5F);
  collider.shape.offset = bx::mul(bx::add(bounds.min, bounds.max), 0.5F);
  return collider;
}
