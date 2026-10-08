#pragma once

#include <bx/math.h>

#include "engine/ecs/components/character_body.h"
#include "engine/physics/character_handle.h"

namespace engine {

class PhysicsSystem;

/**
 * @brief Links an entity to its PhysicsWorld character. Only PhysicsSystem
 * can create or change one; everyone else can read the handle.
 *
 * Holds copies of the feet and CharacterBody as last synced, so a component
 * that differs from its copy was edited outside physics. A copy on another
 * entity is dropped by PhysicsSystem.
 */
class CharacterLink {
 public:
  /** @brief Links to no character; only so the ECS can preallocate slots. */
  CharacterLink() = default;

  /** @brief The entity's character; it stores the entity as its user data. */
  [[nodiscard]] auto Character() const -> physics::CharacterHandle {
    return character_;
  }

  /** @brief Whether it stood on walkable ground after the last step. */
  [[nodiscard]] auto OnGround() const -> bool { return on_ground_; }

 private:
  friend class PhysicsSystem;

  /** @brief Links to @p character with the state it was created from. */
  CharacterLink(physics::CharacterHandle character, const bx::Vec3& feet,
                const CharacterBody& body)
      : character_(character), feet_(feet), body_(body) {}

  physics::CharacterHandle character_;
  /// Transform::position as last synced with the character.
  bx::Vec3 feet_{0.0F};
  /// CharacterBody as last synced with the character.
  CharacterBody body_;
  /// Written by PhysicsSystem every frame.
  bool on_ground_{false};
};

}  // namespace engine
