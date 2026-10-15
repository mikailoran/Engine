#pragma once

namespace engine {
class Engine;
class Ecs;
}  // namespace engine

namespace game {

/**
 * @brief Fires every ready Weapon whose Aim triggers this frame.
 *
 * Casts from the Aim's eyes, ignoring the shooter: the first hit stops the
 * shot and lowers its Health, if any, by the weapon's damage. Spawns a beam
 * entity per shot that its Lifetime removes. Run after physics, once aims
 * are placed (PlaceCamera), and after UpdateCooldowns.
 */
auto HandleShots(engine::Engine& engine) -> void;

/**
 * @brief Counts every Weapon's cooldown down by @p dt seconds, stopping at 0.
 */
auto UpdateCooldowns(engine::Ecs& ecs, float dt) -> void;

}  // namespace game
