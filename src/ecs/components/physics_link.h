#pragma once

#include <optional>

#include "ecs/components/collider.h"
#include "ecs/components/rigid_body.h"
#include "ecs/components/transform.h"
#include "physics/body_handle.h"

/**
 * @brief Links an entity to its PhysicsWorld body. Added, written and removed
 * by PhysicsSystem only.
 *
 * Holds copies of the components as last synced with the body, so a component
 * that differs from its copy was edited outside physics.
 */
struct PhysicsLink {
  /// The entity's body; it stores the entity back as its user data.
  BodyHandle body;
  /// Transform as last synced with the body.
  Transform transform;
  /// Collider as last synced with the body.
  Collider collider;
  /// RigidBody as last synced; engaged while the body is dynamic.
  std::optional<RigidBody> rigid_body;
};
