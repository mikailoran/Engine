#pragma once

#include <optional>

#include "engine/ecs/core/types.h"
#include "engine/platform/screen.h"

class Ecs;
class AssetRegistry;
struct Camera;

/**
 * @brief Finds the entity under a screen position.
 *
 * Casts a ray from @p camera and tests it against the mesh bounds of every
 * entity with a Transform and a Renderable; the nearest hit wins.
 *
 * @param camera Camera whose view and proj are current.
 * @param size Backbuffer size; must have positive area.
 * @return The nearest entity hit, or empty on a miss.
 */
auto Pick(Ecs& ecs, const AssetRegistry& assets, const Camera& camera,
          ScreenPosition position, ScreenSize size) -> std::optional<Entity>;
