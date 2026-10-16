#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "engine/physics/pose.h"
#include "engine/physics/ragdoll.h"
#include "engine/resource/skeleton.h"
#include "engine/util/mat4.h"

namespace engine {

/** @brief One ragdoll part as a ragdoll file describes it, by joint names. */
struct RagdollPartSpec {
  /// The joint the part follows; its capsule starts there.
  std::string joint;
  /// The joint its capsule ends at, in rest pose; empty: `length` along the
  /// joint's own +Y (a bone's direction in Blender rigs).
  std::string end;
  /// Capsule length from the joint when `end` is empty, in m.
  float length{0.2F};
  /// The joint of the part it hangs from; empty for the root part.
  std::string parent;
  /// Capsule radius, in m.
  float radius{0.1F};
  /// Mass in kg.
  float mass{5.0F};
  /// Swing cone half angle and twist range either way, in degrees.
  float swing_deg{45.0F};
  float twist_deg{20.0F};
};

/**
 * @brief Reads a ragdoll file: {"version": 1, "parts": [...]}, each part
 * {"joint", "end" or "length", "parent", "radius", "mass_kg", "swing_deg",
 * "twist_deg"}; only "joint" is required.
 * @param path A file on disk, e.g. under assets/ragdolls/.
 * @throws std::runtime_error Naming @p path, if it can't be read, isn't
 *         valid JSON, has the wrong version or an unknown key.
 */
auto ReadRagdollFile(const std::filesystem::path& path)
    -> std::vector<RagdollPartSpec>;

/**
 * @brief Fits each part's capsule to @p skeleton's rest pose: from its joint
 * to its end, the radius covering the end caps.
 *
 * @param rest_model Each joint's rest model matrix, in skeleton order.
 * @return Parts in @p specs' order, with joint and parent indices resolved.
 * @throws std::runtime_error On a name the skeleton doesn't have, a parent
 *         that isn't an earlier part, or a capsule of zero length.
 */
auto FitRagdoll(const std::vector<RagdollPartSpec>& specs,
                const Skeleton& skeleton, const std::vector<Mat4>& rest_model)
    -> physics::RagdollDesc;

/** @brief Each model matrix as a pose: position and rotation, scale dropped. */
auto PosesOf(const std::vector<Mat4>& model) -> std::vector<physics::Pose>;

}  // namespace engine
