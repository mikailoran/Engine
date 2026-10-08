#pragma once

namespace engine {

class SceneLoader;

/**
 * @brief Registers the "transform", "renderable", "directional_light",
 * "collider", "rigid_body" and "character_body" component loaders.
 */
void RegisterBuiltinLoaders(SceneLoader& loader);

}  // namespace engine
