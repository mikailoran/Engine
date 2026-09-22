#pragma once

/**
 * @brief Constant angular velocity about the Y axis.
 *
 * Exists to narrow the Physics system's signature. Transform alone is carried
 * by nearly every entity, so a system matching on it would sweep up the camera
 * and anything else that merely has a position.
 */
struct Spin {
  /// Radians per second. Positive is counter-clockwise looking down -Y.
  float radians_per_second{2.0F};
};
