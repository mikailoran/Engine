#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "engine/physics/pose.h"
#include "engine/physics/ragdoll_handle.h"  // IWYU pragma: export

namespace engine::physics {

/**
 * @brief One rigid part of a ragdoll: a capsule that follows one joint of a
 * skeleton and hangs from its parent part by a swing-twist joint.
 */
struct RagdollPart {
  /// The skeleton joint it follows: an index into the joint poses the
  /// world is given.
  std::uint32_t joint{0};
  /// Index of the part it hangs from, earlier in RagdollDesc::parts; empty
  /// for the root part.
  std::optional<std::uint32_t> parent;
  /// The capsule's centre and rotation in the joint's space; the capsule's
  /// axis is the rotated +Y.
  Pose offset;
  /// Half the length of the capsule's straight section, in m.
  float half_height{0.1F};
  /// Capsule radius, in m.
  float radius{0.05F};
  /// Mass in kg.
  float mass{5.0F};
  /// How far it may swing away from its rest direction relative to its
  /// parent, and twist about it either way, in degrees.
  float swing_deg{45.0F};
  float twist_deg{20.0F};
};

/** @brief Input to CreateRagdollSettings: a rig's ragdoll parts. */
struct RagdollDesc {
  /// Parents come before their children.
  std::vector<RagdollPart> parts;
};

/** @brief Output of RagdollJointPoses: where a part puts its joint. */
struct RagdollJoint {
  /// The skeleton joint, as in RagdollPart::joint.
  std::uint32_t joint{0};
  /// Its world pose.
  Pose pose;
};

/** @brief Output of Ragdolls: a live ragdoll and its creation tag. */
struct RagdollEntry {
  RagdollHandle ragdoll;
  std::uint64_t user_data{0};
};

}  // namespace engine::physics
