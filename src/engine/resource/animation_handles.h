#pragma once

#include <cstdint>

#include "engine/util/handle.h"  // IWYU pragma: export

namespace engine {

/**
 * @brief Opaque id of an animation::Rig in an AssetRegistry, one per skinned
 * mesh. Rigs live until the registry is destroyed, so it never goes stale.
 */
using RigHandle = Handle<struct RigTag, std::uint16_t>;

/**
 * @brief Opaque id of an animation::Clip in an AssetRegistry, built for one
 * rig. Never goes stale.
 */
using ClipHandle = Handle<struct ClipTag, std::uint32_t>;

/**
 * @brief Opaque id of a clip file's clips retargeted onto one rig, looked up
 * by name with AssetRegistry::FindClip. Never goes stale.
 */
using ClipSetHandle = Handle<struct ClipSetTag, std::uint16_t>;

}  // namespace engine
