#pragma once

#include <optional>

#include "engine/ecs/core/types.h"
#include "engine/platform/screen.h"

namespace engine {

class Ecs;
class AssetRegistry;

/**
 * @brief Finds the entity under a screen position.
 *
 * Casts a ray from @p camera and tests it against the mesh bounds of every
 * entity with a Transform and a Renderable; the nearest hit wins.
 *
 * @param camera Entity carrying a Camera and a Transform.
 * @param size Backbuffer size; must have positive area.
 * @return The nearest entity hit, or empty on a miss.
 */
auto Pick(Ecs& ecs, const AssetRegistry& assets, Entity camera,
          ScreenPosition position, ScreenSize size) -> std::optional<Entity>;

}  // namespace engine
