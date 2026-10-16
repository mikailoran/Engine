#pragma once

#include <algorithm>
#include <cstdint>

namespace game {

// Enemy decisions as pure functions: standard headers only, so game_tests
// covers them without the engine.

/** @brief What an enemy does about the player this frame. */
enum class EnemyAction : std::uint8_t {
  /// Far away: run at them.
  kRun,
  /// Closing in: walk at them.
  kWalk,
  /// In range: stand, aim and shoot.
  kEngage,
};

/** @brief Distances at which an enemy changes what it does, in m. */
struct EnemyRanges {
  /// Stops and shoots within this distance.
  float engage_m{8.0F};
  /// Runs beyond this distance, walks between it and engage_m.
  float run_m{15.0F};
};

/** @brief What an enemy @p distance_m from the player does. */
inline auto ChooseEnemyAction(float distance_m, const EnemyRanges& ranges)
    -> EnemyAction {
  if (distance_m <= ranges.engage_m) {
    return EnemyAction::kEngage;
  }
  return distance_m > ranges.run_m ? EnemyAction::kRun : EnemyAction::kWalk;
}

/**
 * @brief How far @p speed lies from walking to running, 0 to 1: the blend
 * between a walk clip authored at @p walk_speed and a run at @p run_speed.
 */
inline auto LocomotionBlend(float speed, float walk_speed, float run_speed)
    -> float {
  if (run_speed <= walk_speed) {
    return speed >= run_speed ? 1.0F : 0.0F;
  }
  return std::clamp((speed - walk_speed) / (run_speed - walk_speed), 0.0F,
                    1.0F);
}

}  // namespace game
