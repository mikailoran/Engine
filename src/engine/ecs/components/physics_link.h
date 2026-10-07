#pragma once

#include <optional>

#include "engine/ecs/components/collider.h"
#include "engine/ecs/components/rigid_body.h"
#include "engine/ecs/components/transform.h"
#include "engine/physics/body_handle.h"

class PhysicsSystem;

/**
 * @brief Links an entity to its PhysicsWorld body. Only PhysicsSystem can
 * create or change one; everyone else can read the body handle.
 *
 * Holds copies of the components as last synced with the body, so a component
 * that differs from its copy was edited outside physics. Copyable, as every
 * component is, but a copy on another entity is dropped by PhysicsSystem.
 */
class PhysicsLink {
 public:
  /** @brief Links to no body; only so the ECS can preallocate slots. */
  PhysicsLink() = default;

  /** @brief The entity's body; it stores the entity back as its user data. */
  [[nodiscard]] auto Body() const -> BodyHandle { return body_; }

 private:
  friend class PhysicsSystem;

  /** @brief Links to @p body with the components it was created from. */
  PhysicsLink(BodyHandle body, const Transform& transform,
              const Collider& collider,
              const std::optional<RigidBody>& rigid_body)
      : body_(body),
        transform_(transform),
        collider_(collider),
        rigid_body_(rigid_body) {}

  BodyHandle body_;
  /// Transform as last synced with the body.
  Transform transform_;
  /// Collider as last synced with the body.
  Collider collider_;
  /// RigidBody as last synced; engaged while the body is dynamic.
  std::optional<RigidBody> rigid_body_;
};
