#pragma once

#include "engine/util/handle.h"  // IWYU pragma: export

namespace engine::physics {

/**
 * @brief Opaque id of a character in a PhysicsWorld. Ids are never reused, so
 * a stale one never aliases a newer character.
 */
using CharacterHandle = Handle<struct CharacterTag>;

}  // namespace engine::physics
