#pragma once

#include <bx/bx.h>
#include <bx/math.h>

#include <cstdint>
#include <string>
#include <vector>

#include "engine/resource/skeleton.h"

namespace engine {

/** @brief A translation or scale at one moment of a clip. */
struct VectorKey {
  /// Seconds from the clip's start.
  float time{0.0F};
  bx::Vec3 value{0.0F};
};

/** @brief A rotation at one moment of a clip. */
struct RotationKey {
  /// Seconds from the clip's start.
  float time{0.0F};
  /// Unit quaternion, xyzw like Joint::rotation.
  bx::Quaternion value{bx::InitIdentity};
};

/**
 * @brief How one joint moves through a clip, relative to its parent.
 *
 * Keys are sorted by time, strictly increasing, and interpolate linearly
 * (rotations along the shorter arc). An empty channel holds the joint's rest
 * value.
 */
struct JointTrack {
  /// Index into the Skeleton the clip was read or retargeted for.
  std::uint32_t joint{0};
  std::vector<VectorKey> translations;
  std::vector<RotationKey> rotations;
  std::vector<VectorKey> scales;
};

/**
 * @brief One named animation: a track for each joint it moves.
 *
 * Read with its file's skeleton by ReadGltfClips; RetargetClip maps it onto
 * another skeleton with the same joint names.
 */
struct AnimationClip {
  std::string name;
  /// Seconds; the time of its last key.
  float duration{0.0F};
  /// At most one per joint, sorted by joint.
  std::vector<JointTrack> tracks;
};

/** @brief Every clip in a glTF file, with the skeleton they animate. */
struct ClipFile {
  /// The file's skin; the clips' JointTrack::joint indexes it.
  Skeleton skeleton;
  std::vector<AnimationClip> clips;
};

}  // namespace engine
