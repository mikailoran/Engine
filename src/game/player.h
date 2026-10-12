#pragma once

namespace game {

/**
 * @brief Marks the character the player controls, with its movement tuning.
 * Needs a CharacterBody, which does the moving.
 */
struct Player {
  /// Walking speed, in m/s.
  float walk_speed{4.5F};
  /// Speed factor while sprinting.
  float sprint_multiplier{1.8F};
  /// Upward speed a jump starts with, in m/s; about 1.3 m high at 5.
  float jump_speed{5.0F};
  /// Height of the eyes above the feet, in m.
  float eye_height{1.6F};
};

}  // namespace game
