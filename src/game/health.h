#pragma once

#include <algorithm>

namespace game {

/**
 * @brief How much damage an entity can still take. Shots lower `current`;
 * nothing happens yet when it reaches 0.
 */
struct Health {
  /// Full health; positive.
  int max{100};
  /// Remaining health, within 0..max.
  int current{100};
};

/**
 * @brief Lowers @p health by @p damage, never below 0.
 * @return The new current health.
 */
inline auto ApplyDamage(Health& health, int damage) -> int {
  health.current = std::max(0, health.current - damage);
  return health.current;
}

}  // namespace game
