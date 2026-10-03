#pragma once

class SceneLoader;

/**
 * @brief Registers the "transform", "renderable" and "directional_light"
 * component loaders.
 */
void RegisterBuiltinLoaders(SceneLoader& loader);
