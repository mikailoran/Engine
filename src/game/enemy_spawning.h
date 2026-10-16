#pragma once

namespace engine {
class Engine;
}  // namespace engine

namespace game {

/**
 * @brief Spawns an enemy where each Aim whose spawn trigger fired this frame
 * hits, feet at the hit point; a miss spawns nothing.
 *
 * Run after physics, once aims are placed (PlaceCamera). Spawned enemies
 * join physics on the next Update.
 */
auto HandleEnemySpawns(engine::Engine& engine) -> void;

}  // namespace game
