#include "engine/animation/rig.h"

#include <bx/math.h>
#include <ozz/animation/offline/animation_builder.h>
#include <ozz/animation/offline/raw_animation.h>
#include <ozz/animation/offline/raw_skeleton.h>
#include <ozz/animation/offline/skeleton_builder.h>
#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/base/containers/vector.h>
#include <ozz/base/maths/quaternion.h>
#include <ozz/base/maths/transform.h>
#include <ozz/base/maths/vec_float.h>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "engine/animation/ozz_state.h"
#include "engine/resource/animation_clip.h"
#include "engine/resource/skeleton.h"

namespace engine::animation {

namespace {

/// Shortest clip the runtime accepts, in s; single-pose clips get this.
constexpr float kMinDuration = 1.0F / 30.0F;

/** @brief @p v as the runtime's vector. */
auto ToOzz(const bx::Vec3& v) -> ozz::math::Float3 { return {v.x, v.y, v.z}; }

/** @brief @p q as the runtime's quaternion. */
auto ToOzz(const bx::Quaternion& q) -> ozz::math::Quaternion {
  return {q.x, q.y, q.z, q.w};
}

/**
 * @brief The runtime's raw hierarchy for @p skeleton, and the Skeleton joint
 * of each runtime joint in the depth-first order the builder numbers them.
 */
auto BuildRawSkeleton(const Skeleton& skeleton, std::vector<int>& joint_order)
    -> ozz::animation::offline::RawSkeleton {
  using RawJoint = ozz::animation::offline::RawSkeleton::Joint;
  const std::size_t count = skeleton.joints.size();
  std::vector<std::vector<std::size_t>> children(count);
  std::vector<std::size_t> roots;
  for (std::size_t i = 0; i < count; ++i) {
    const auto& parent = skeleton.joints.at(i).parent;
    (parent ? children.at(*parent) : roots).push_back(i);
  }

  // Pre-order, children in Skeleton order: the order SkeletonBuilder numbers
  std::vector<std::size_t> pending(roots.rbegin(), roots.rend());
  while (!pending.empty()) {
    const std::size_t index = pending.back();
    pending.pop_back();
    joint_order.push_back(static_cast<int>(index));
    const auto& kids = children.at(index);
    pending.insert(pending.end(), kids.rbegin(), kids.rend());
  }

  // Children follow their parent in pre-order, so build in reverse
  std::vector<RawJoint> built(count);
  for (const int index : std::views::reverse(joint_order)) {
    const Joint& joint = skeleton.joints.at(index);
    RawJoint& raw = built.at(index);
    // ozz strings use their own allocator
    raw.name.assign(joint.name.begin(), joint.name.end());
    raw.transform.translation = ToOzz(joint.translation);
    raw.transform.rotation = ToOzz(joint.rotation);
    raw.transform.scale = ToOzz(joint.scale);
    for (const std::size_t child : children.at(index)) {
      raw.children.push_back(std::move(built.at(child)));
    }
  }
  ozz::animation::offline::RawSkeleton raw;
  for (const std::size_t root : roots) {
    raw.roots.push_back(std::move(built.at(root)));
  }
  return raw;
}

/** @brief A one-key channel holding @p value, for joints a clip doesn't key. */
template <class Key, class Value>
auto RestChannel(const Value& value) -> ozz::vector<Key> {
  Key key{};
  key.time = 0.0F;
  key.value = ToOzz(value);
  return {key};
}

/** @brief @p keys in the runtime's form, or @p rest if there are none. */
template <class Key, class Source, class Value>
auto Channel(const std::vector<Source>& keys, const Value& rest)
    -> ozz::vector<Key> {
  if (keys.empty()) {
    return RestChannel<Key>(rest);
  }
  ozz::vector<Key> out;
  out.reserve(keys.size());
  for (const Source& source : keys) {
    Key key{};
    key.time = source.time;
    key.value = ToOzz(source.value);
    out.push_back(key);
  }
  return out;
}

}  // namespace

Rig::Rig(const Skeleton& skeleton) : state_(std::make_unique<State>()) {
  state_->source = skeleton;
  const auto raw = BuildRawSkeleton(skeleton, state_->joint_of_runtime);
  if (!raw.Validate()) {
    throw std::runtime_error("invalid skeleton hierarchy");
  }
  state_->skeleton = ozz::animation::offline::SkeletonBuilder()(raw);
  if (!state_->skeleton) {
    throw std::runtime_error("cannot build skeleton");
  }

  state_->runtime_of_joint.resize(skeleton.joints.size());
  for (std::size_t i = 0; i < state_->joint_of_runtime.size(); ++i) {
    state_->runtime_of_joint.at(state_->joint_of_runtime.at(i)) =
        static_cast<int>(i);
  }
}

Rig::~Rig() = default;
Rig::Rig(Rig&&) noexcept = default;
auto Rig::operator=(Rig&&) noexcept -> Rig& = default;

auto Rig::Source() const -> const Skeleton& { return state_->source; }

auto Rig::JointCount() const -> std::size_t {
  return state_->source.joints.size();
}

Clip::Clip(const AnimationClip& clip, const Rig& rig)
    : state_(std::make_unique<State>()) {
  using Raw = ozz::animation::offline::RawAnimation;
  const Rig::State& rig_state = *rig.state_;
  const Skeleton& skeleton = rig_state.source;

  // Each Skeleton joint's track, if the clip keys it
  std::vector<const JointTrack*> track_of(skeleton.joints.size(), nullptr);
  for (const JointTrack& track : clip.tracks) {
    track_of.at(track.joint) = &track;
  }

  Raw raw;
  raw.name.assign(clip.name.begin(), clip.name.end());
  raw.duration = std::max(clip.duration, kMinDuration);
  // The runtime wants one track per joint, in its own order
  for (const int joint_index : rig_state.joint_of_runtime) {
    const Joint& joint = skeleton.joints.at(joint_index);
    const JointTrack* track = track_of.at(joint_index);
    Raw::JointTrack& raw_track = raw.tracks.emplace_back();
    raw_track.translations = Channel<Raw::TranslationKey>(
        track ? track->translations : std::vector<VectorKey>{},
        joint.translation);
    raw_track.rotations = Channel<Raw::RotationKey>(
        track ? track->rotations : std::vector<RotationKey>{}, joint.rotation);
    raw_track.scales = Channel<Raw::ScaleKey>(
        track ? track->scales : std::vector<VectorKey>{}, joint.scale);
  }
  if (!raw.Validate()) {
    throw std::runtime_error("invalid keys in clip '" + clip.name + "'");
  }
  state_->name = clip.name;
  state_->animation = ozz::animation::offline::AnimationBuilder()(raw);
  if (!state_->animation) {
    throw std::runtime_error("cannot build clip '" + clip.name + "'");
  }
}

Clip::~Clip() = default;
Clip::Clip(Clip&&) noexcept = default;
auto Clip::operator=(Clip&&) noexcept -> Clip& = default;

auto Clip::Name() const -> const std::string& { return state_->name; }

auto Clip::Duration() const -> float { return state_->animation->duration(); }

}  // namespace engine::animation
