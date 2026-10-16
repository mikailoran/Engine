#pragma once

#include <cstdint>
#include <vector>

#include "engine/physics/ragdoll_handle.h"
#include "engine/util/mat4.h"

namespace engine {

class PhysicsSystem;

/** @brief A joint a simulated ragdoll part moves, and where it put it. */
struct DrivenJoint {
  /// The skeleton joint.
  std::uint32_t joint{0};
  /// Its world matrix after the last step.
  Mat4 world{};
};

/**
 * @brief Links an entity to its PhysicsWorld ragdoll. Only PhysicsSystem
 * can create or change one; everyone else can read it.
 *
 * Holds the settings and simulate flag as last synced, so a Ragdoll that
 * differs was edited outside physics. A copy on another entity is dropped.
 */
class RagdollLink {
 public:
  /** @brief Links to no ragdoll; only so the ECS can preallocate slots. */
  RagdollLink() = default;

  /** @brief The entity's ragdoll; it stores the entity as its user data. */
  [[nodiscard]] auto Ragdoll() const -> physics::RagdollHandle {
    return ragdoll_;
  }

  /** @brief Whether physics moves its parts. */
  [[nodiscard]] auto Simulated() const -> bool { return simulated_; }

  /**
   * @brief While simulated, every joint a part moves and where it was after
   * the last step; empty otherwise. AnimationSystem poses the rest of the
   * skeleton around them.
   */
  [[nodiscard]] auto Driven() const -> const std::vector<DrivenJoint>& {
    return driven_;
  }

 private:
  friend class PhysicsSystem;

  /** @brief Links to @p ragdoll, built from @p settings, kinematic. */
  RagdollLink(physics::RagdollHandle ragdoll,
              physics::RagdollSettingsHandle settings)
      : ragdoll_(ragdoll), settings_(settings) {}

  physics::RagdollHandle ragdoll_;
  /// Ragdoll::settings as last synced.
  physics::RagdollSettingsHandle settings_;
  /// Ragdoll::simulate as last synced.
  bool simulated_{false};
  /// Written by PhysicsSystem after each step while simulated.
  std::vector<DrivenJoint> driven_;
};

}  // namespace engine
