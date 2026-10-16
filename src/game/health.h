#pragma once

#include <algorithm>

namespace game {

/**
 * @brief How much damage an entity can still take. Shots lower `current`;
 * at 0 the entity dies (see HandleShots).
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

/** @brief Whether @p health has run out. */
inline auto IsDead(const Health& health) -> bool { return health.current <= 0; }

}  // namespace game
