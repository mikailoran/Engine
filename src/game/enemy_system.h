#pragma once

namespace engine {
class Ecs;
class Engine;
}  // namespace engine

namespace game {

/**
 * @brief Steers every Enemy at the player and picks its clips: runs or
 * walks when far, stands, aims and fires when in range, pulling its Aim's
 * trigger on the frame a shot starts. Run before physics.
 * @param dt Frame time, in s.
 * @param has_control False stops enemies where they stand.
 */
auto DriveEnemies(engine::Ecs& ecs, float dt, bool has_control) -> void;

/**
 * @brief Points each armed enemy's Aim from its gun's muzzle at the
 * player's chest. Run after the engine's update, once the hands are posed,
 * and before HandleShots.
 */
auto AimEnemies(engine::Engine& engine) -> void;

}  // namespace game
