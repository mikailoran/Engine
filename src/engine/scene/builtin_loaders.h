#pragma once

namespace engine {

class SceneLoader;

/**
 * @brief Registers the "transform", "renderable", "directional_light" and
 * "collider" component loaders.
 */
void RegisterBuiltinLoaders(SceneLoader& loader);

}  // namespace engine
