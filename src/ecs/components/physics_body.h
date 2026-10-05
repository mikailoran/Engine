#pragma once

#include <optional>

#include "ecs/components/collider.h"
#include "ecs/components/rigid_body.h"
#include "ecs/components/transform.h"
#include "physics/body_handle.h"

/**
 * @brief Links an entity to its PhysicsWorld body. Added, written and removed
 * by Physics only.
 *
 * Holds copies of the components as last synced with the body, so a component
 * that differs from its copy was edited outside physics.
 */
struct PhysicsBody {
  BodyHandle body;
  Transform transform;
  Collider collider;
  /// Engaged while the body is dynamic.
  std::optional<RigidBody> rigid_body;
};
