#pragma once

#include "engine/util/handle.h"  // IWYU pragma: export

namespace engine::physics {

/**
 * @brief Opaque id of a collision mesh in a PhysicsWorld. Meshes live until
 * the world is destroyed, so a valid id never goes stale.
 */
using CollisionMeshHandle = Handle<struct CollisionMeshTag>;

}  // namespace engine::physics
