#pragma once

#include "engine/util/handle.h"  // IWYU pragma: export

namespace engine::physics {

/**
 * @brief Opaque id of a hitbox in a PhysicsWorld. Hitboxes live until the
 * world is destroyed, so a valid id never goes stale.
 */
using HitboxHandle = Handle<struct HitboxTag>;

}  // namespace engine::physics
