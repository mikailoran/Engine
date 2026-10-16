#include "engine/resource/gltf_animation.h"

#include <bx/math.h>
#include <cgltf/cgltf.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <span>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "engine/resource/animation_clip.h"
#include "engine/resource/gltf_document.h"

namespace engine::gltf {

namespace {

/** @brief A sampler's key times and flat values, interpolated linearly. */
struct LinearKeys {
  /// Seconds, strictly increasing.
  std::vector<float> times;
  /// `components` floats per time.
  std::vector<float> values;
};

/**
 * @brief Reads @p sampler's keys, @p components floats per value; a STEP
 * sampler gains a key just before each change, so linear interpolation
 * holds each value.
 * @throws std::runtime_error On CUBICSPLINE, no keys, mismatched counts or
 *         times that don't increase.
 */
auto ReadLinearKeys(const cgltf_animation_sampler& sampler,
                    std::size_t components) -> LinearKeys {
  if (sampler.interpolation == cgltf_interpolation_type_cubic_spline) {
    throw std::runtime_error("CUBICSPLINE animation is unsupported");
  }
  const auto times = UnpackFloats(*sampler.input, 1, "animation input");
  const auto values =
      UnpackFloats(*sampler.output, components, "animation output");
  if (times.empty()) {
    throw std::runtime_error("animation sampler without keys");
  }
  if (values.size() != times.size() * components) {
    throw std::runtime_error("animation key counts differ");
  }
  for (std::size_t i = 1; i < times.size(); ++i) {
    if (times.at(i) <= times.at(i - 1)) {
      throw std::runtime_error("animation key times don't increase");
    }
  }
  if (sampler.interpolation != cgltf_interpolation_type_step) {
    return {.times = times, .values = values};
  }

  // Hold each value until a hair before the next key
  LinearKeys keys;
  for (std::size_t i = 0; i < times.size(); ++i) {
    const auto value = std::span(values).subspan(i * components, components);
    keys.times.push_back(times.at(i));
    keys.values.insert(keys.values.end(), value.begin(), value.end());
    if (i + 1 < times.size()) {
      const float gap = times.at(i + 1) - times.at(i);
      keys.times.push_back(times.at(i + 1) - (gap * 1e-3F));
      keys.values.insert(keys.values.end(), value.begin(), value.end());
    }
  }
  return keys;
}

/** @brief @p keys as translation or scale keys. */
auto ToVectorKeys(const LinearKeys& keys) -> std::vector<VectorKey> {
  std::vector<VectorKey> out;
  out.reserve(keys.times.size());
  for (std::size_t i = 0; i < keys.times.size(); ++i) {
    out.push_back({.time = keys.times.at(i),
                   .value = bx::load<bx::Vec3>(&keys.values.at(i * 3))});
  }
  return out;
}

/** @brief @p keys as rotation keys, renormalized. */
auto ToRotationKeys(const LinearKeys& keys) -> std::vector<RotationKey> {
  std::vector<RotationKey> out;
  out.reserve(keys.times.size());
  for (std::size_t i = 0; i < keys.times.size(); ++i) {
    const auto q = bx::load<bx::Quaternion>(&keys.values.at(i * 4));
    out.push_back({.time = keys.times.at(i), .value = bx::normalize(q)});
  }
  return out;
}

/**
 * @brief Fills @p track's channel for @p channel's property from its keys.
 * @return The time of its last key.
 * @throws std::runtime_error If the channel is already filled.
 */
auto FillChannel(const cgltf_animation_channel& channel, JointTrack& track)
    -> float {
  const auto refuse_twice = [](bool filled) -> void {
    if (filled) {
      throw std::runtime_error("two channels animate one joint property");
    }
  };
  switch (channel.target_path) {
    case cgltf_animation_path_type_translation: {
      refuse_twice(!track.translations.empty());
      const auto keys = ReadLinearKeys(*channel.sampler, 3);
      track.translations = ToVectorKeys(keys);
      return keys.times.back();
    }
    case cgltf_animation_path_type_rotation: {
      refuse_twice(!track.rotations.empty());
      const auto keys = ReadLinearKeys(*channel.sampler, 4);
      track.rotations = ToRotationKeys(keys);
      return keys.times.back();
    }
    case cgltf_animation_path_type_scale: {
      refuse_twice(!track.scales.empty());
      const auto keys = ReadLinearKeys(*channel.sampler, 3);
      track.scales = ToVectorKeys(keys);
      return keys.times.back();
    }
    default:
      return 0.0F;
  }
}

}  // namespace

auto ReadClips(const cgltf_data& data, const cgltf_skin& skin)
    -> std::vector<AnimationClip> {
  // Joint index of each node that is one of the skin's joints
  std::unordered_map<const cgltf_node*, std::uint32_t> joint_of;
  for (const cgltf_node* node : std::span(skin.joints, skin.joints_count)) {
    joint_of.emplace(node, static_cast<std::uint32_t>(joint_of.size()));
  }

  std::vector<AnimationClip> clips;
  for (const cgltf_animation& animation :
       std::span(data.animations, data.animations_count)) {
    AnimationClip clip{.name = animation.name != nullptr
                                   ? animation.name
                                   : "clip" + std::to_string(clips.size()),
                       .duration = 0.0F,
                       .tracks = {}};
    // Sorted by joint, as AnimationClip::tracks must be
    std::map<std::uint32_t, JointTrack> tracks;
    for (const cgltf_animation_channel& channel :
         std::span(animation.channels, animation.channels_count)) {
      // Only joints' transforms; morph weights and other nodes are skipped
      const auto joint = joint_of.find(channel.target_node);
      if (joint == joint_of.end() ||
          channel.target_path == cgltf_animation_path_type_weights) {
        continue;
      }
      JointTrack& track = tracks[joint->second];
      track.joint = joint->second;
      try {
        clip.duration = std::max(clip.duration, FillChannel(channel, track));
      } catch (const std::runtime_error& e) {
        throw std::runtime_error("animation '" + clip.name + "': " + e.what());
      }
    }
    for (auto& [joint, track] : tracks) {
      clip.tracks.push_back(std::move(track));
    }
    clips.push_back(std::move(clip));
  }
  return clips;
}

}  // namespace engine::gltf
