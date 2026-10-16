#pragma once

#include <cgltf/cgltf.h>

#include <vector>

#include "engine/resource/animation_clip.h"

namespace engine::gltf {

// Internal to engine_assets: no public header includes this one.

/**
 * @brief Reads every animation in @p data as a clip on @p skin's joints.
 *
 * Channels on nodes that aren't joints of @p skin, and morph-target weight
 * channels, are skipped. STEP samplers become linear ones that hold each
 * value until just before the next key.
 * @throws std::runtime_error On CUBICSPLINE interpolation, two channels
 *         animating the same joint property, or malformed keys.
 */
auto ReadClips(const cgltf_data& data, const cgltf_skin& skin)
    -> std::vector<AnimationClip>;

}  // namespace engine::gltf
