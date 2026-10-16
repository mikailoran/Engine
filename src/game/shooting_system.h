#pragma once

namespace engine {
class Engine;
class Ecs;
}  // namespace engine

namespace game {

/**
 * @brief Fires every ready Weapon whose Aim triggers this frame.
 *
 * Casts along the Aim's ray, ignoring the shooter: the first hit stops the
 * shot and lowers its Health, if any, by the weapon's damage. At 0 it dies:
 * the player respawns at its spawn point with full health, a character with
 * a Ragdoll goes limp and lies for a while, anything else is destroyed. Spawns
 * a beam entity per shot, from the Aim's muzzle, that its Lifetime removes. Run
 * after physics, once aims are placed (PlaceCamera, AimEnemies), and after
 * UpdateCooldowns.
 */
auto HandleShots(engine::Engine& engine) -> void;

/**
 * @brief Counts every Weapon's cooldown down by @p dt seconds, stopping at 0.
 */
auto UpdateCooldowns(engine::Ecs& ecs, float dt) -> void;

}  // namespace game
