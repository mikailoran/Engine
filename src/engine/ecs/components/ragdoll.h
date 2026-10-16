#pragma once

#include "engine/physics/ragdoll_handle.h"

namespace engine {

/**
 * @brief Gives a skinned, posed entity a ragdoll: one capsule per major
 * bone. Needs a Transform and a SkinPose (an Animator gives one).
 *
 * While not simulated its parts follow the pose, so rays hit the limbs
 * (and a CharacterBody's capsule is no ray target); simulated, physics
 * moves them and the pose follows. Assumes the Transform's scale is 1.
 */
struct Ragdoll {
  /// The rig's parts, from AssetRegistry::LoadRagdoll.
  physics::RagdollSettingsHandle settings{
      physics::RagdollSettingsHandle::kInvalid};
  /// Hand the parts to the simulation (death); gameplay sets it.
  bool simulate{false};
};

}  // namespace engine
