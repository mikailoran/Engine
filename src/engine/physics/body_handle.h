#pragma once

#include "engine/util/handle.h"  // IWYU pragma: export

namespace engine::physics {

/** @brief Opaque id of a body in a PhysicsWorld. Stale ids never alias. */
using BodyHandle = Handle<struct BodyTag>;

}  // namespace engine::physics
