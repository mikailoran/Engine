#pragma once

/**
 * @brief Constant angular velocity about the Y axis.
 */
struct Spin {
  // Positive is counter-clockwise looking down towards -Y.
  float radians_per_second{2.0F};
};
