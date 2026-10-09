#pragma once

namespace engine {
class Engine;
}  // namespace engine

namespace game {

/**
 * @brief Registers the game's components and their scene loaders with
 * @p engine. Call before loading a scene that uses them.
 */
void RegisterGameLogic(engine::Engine& engine);

}  // namespace game
