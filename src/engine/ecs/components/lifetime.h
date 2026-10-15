#pragma once

namespace engine {

/**
 * @brief Destroys its entity once the time runs out (see ExpireLifetimes).
 * Added at runtime (beams) or from a scene's "lifetime" block.
 */
struct Lifetime {
  /// Seconds left; the entity is destroyed at the end of the frame it hits 0.
  float remaining_s{100.0F};
};

}  // namespace engine
