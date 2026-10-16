#pragma once

#include "engine/util/handle.h"  // IWYU pragma: export

namespace engine::physics {

/**
 * @brief Opaque id of shared ragdoll settings in a PhysicsWorld, one per
 * rig. Never goes stale.
 */
using RagdollSettingsHandle = Handle<struct RagdollSettingsTag>;

/** @brief Opaque id of a ragdoll in a PhysicsWorld. Stale ids never alias. */
using RagdollHandle = Handle<struct RagdollTag>;

}  // namespace engine::physics
