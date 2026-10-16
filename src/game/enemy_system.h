#pragma once

namespace engine {
class Ecs;
}  // namespace engine

namespace game {

/**
 * @brief Steers every Enemy at the player and picks its clips: runs or
 * walks when far, stands, aims and fires when in range. Run before physics.
 * @param dt Frame time, in s.
 * @param has_control False stops enemies where they stand.
 */
auto DriveEnemies(engine::Ecs& ecs, float dt, bool has_control) -> void;

}  // namespace game
