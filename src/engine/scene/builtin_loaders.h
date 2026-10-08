#pragma once

namespace engine {

class SceneLoader;

/**
 * @brief Registers the "transform", "renderable", "directional_light",
 * "collider" and "rigid_body" component loaders.
 */
void RegisterBuiltinLoaders(SceneLoader& loader);

}  // namespace engine
