#pragma once

class SceneLoader;

/** @brief Registers the "transform" and "renderable" component loaders. */
void RegisterBuiltinLoaders(SceneLoader &loader);
