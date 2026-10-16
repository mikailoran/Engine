#include "engine/resource/retarget.h"

#include <bx/math.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "engine/resource/animation_clip.h"
#include "engine/resource/skeleton.h"

namespace engine {

namespace {

/** @brief Shortest bone below which no length ratio is taken, in m. */
constexpr float kMinBoneLength = 1e-4F;

/** @brief Whether @p a and @p b have the same joints and rest translations. */
auto SameRig(const Skeleton& a, const Skeleton& b) -> bool {
  return std::ranges::equal(
      a.joints, b.joints, [](const Joint& x, const Joint& y) -> bool {
        return x.name == y.name && x.parent == y.parent &&
               bx::isEqual(x.translation, y.translation, 1e-6F);
      });
}

/** @brief Whether @p joint is the child of a root, like the hips. */
auto IsRootChild(const Skeleton& skeleton, const Joint& joint) -> bool {
  return joint.parent && !skeleton.joints.at(*joint.parent).parent;
}

/**
 * @brief The target's version of @p track: rotations kept, translations per
 * the rules in RetargetClip, scale left to the rest pose.
 */
auto RetargetTrack(const JointTrack& track, const Joint& from, const Joint& to,
                   const Skeleton& target, std::uint32_t target_index)
    -> JointTrack {
  JointTrack out{.joint = target_index,
                 .translations = {},
                 .rotations = track.rotations,
                 .scales = {}};
  if (!to.parent) {
    out.translations = track.translations;
  } else if (IsRootChild(target, to)) {
    const float from_length = bx::length(from.translation);
    const float ratio = from_length > kMinBoneLength
                            ? bx::length(to.translation) / from_length
                            : 1.0F;
    out.translations = track.translations;
    for (VectorKey& key : out.translations) {
      key.value = bx::mul(key.value, ratio);
    }
  }
  return out;
}

}  // namespace

auto RetargetClip(const AnimationClip& clip, const Skeleton& source,
                  const Skeleton& target) -> AnimationClip {
  if (SameRig(source, target)) {
    return clip;
  }

  std::unordered_map<std::string, std::uint32_t> target_of;
  for (std::size_t i = 0; i < target.joints.size(); ++i) {
    target_of.emplace(target.joints.at(i).name, static_cast<std::uint32_t>(i));
  }

  AnimationClip out{.name = clip.name, .duration = clip.duration, .tracks = {}};
  for (const JointTrack& track : clip.tracks) {
    const Joint& from = source.joints.at(track.joint);
    const auto it = target_of.find(from.name);
    if (it == target_of.end()) {
      continue;
    }
    out.tracks.push_back(RetargetTrack(
        track, from, target.joints.at(it->second), target, it->second));
  }
  if (out.tracks.empty()) {
    throw std::runtime_error("clip '" + clip.name +
                             "' shares no joint name with the skeleton");
  }
  // Target order may differ from the source's
  std::ranges::sort(out.tracks, {}, &JointTrack::joint);
  return out;
}

}  // namespace engine
