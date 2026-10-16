#pragma once

#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/base/memory/unique_ptr.h>

#include <string>
#include <vector>

#include "engine/animation/rig.h"
#include "engine/resource/skeleton.h"

namespace engine::animation {

// Internal to engine_animation: only its .cpp files include this, so ozz
// never reaches another target.

/** @brief What a Rig owns: the runtime skeleton and the joint order maps. */
struct Rig::State {
  /// The skeleton the rig was built from.
  Skeleton source;
  /// Never null.
  ozz::unique_ptr<ozz::animation::Skeleton> skeleton;
  /// The runtime's joint index for each Skeleton joint.
  std::vector<int> runtime_of_joint;
  /// The Skeleton joint for each runtime joint index (depth-first order).
  std::vector<int> joint_of_runtime;
};

/** @brief What a Clip owns: the runtime animation. */
struct Clip::State {
  std::string name;
  /// Never null.
  ozz::unique_ptr<ozz::animation::Animation> animation;
};

}  // namespace engine::animation
