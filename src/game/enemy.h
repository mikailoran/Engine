#pragma once

#include <cstddef>

#include "engine/resource/animation_handles.h"
#include "game/enemy_rules.h"

namespace game {

/// Animator layers an enemy uses: legs and body, the upper body's stance,
/// and the upper-body shot on top.
inline constexpr std::size_t kBaseLayer = 0;
inline constexpr std::size_t kUpperLayer = 1;
inline constexpr std::size_t kShotLayer = 2;

/** @brief The clips an enemy plays, found by name when it loads. */
struct EnemyClips {
  /// Base layer, standing.
  engine::ClipHandle idle{engine::ClipHandle::kInvalid};
  /// Base layer, blended by speed between the two.
  engine::ClipHandle walk{engine::ClipHandle::kInvalid};
  engine::ClipHandle run{engine::ClipHandle::kInvalid};
  /// Upper body while moving: the weapon held low.
  engine::ClipHandle hold{engine::ClipHandle::kInvalid};
  /// Upper body while engaging: the weapon raised.
  engine::ClipHandle aim{engine::ClipHandle::kInvalid};
  /// Upper body, once per shot.
  engine::ClipHandle fire{engine::ClipHandle::kInvalid};
};

/**
 * @brief Makes a character chase the player in a straight line and shoot
 * once in range. Needs a Transform, a CharacterBody and an Animator, whose
 * upper layers it masks; loaded from a scene's "enemy" block.
 */
struct Enemy {
  EnemyRanges ranges;
  /// Ground speeds the walk and run clips were authored at, in m/s; the
  /// enemy moves at these.
  float walk_speed{1.5F};
  float run_speed{3.5F};
  /// Seconds between shots while engaging.
  float fire_interval_s{1.0F};
  EnemyClips clips;
  /// Runtime: s until the next shot while engaging.
  float fire_cooldown_s{0.0F};
};

}  // namespace game
