#pragma once

#include <bx/math.h>

/**
 * @brief Makes a Collider entity's body dynamic: moved by gravity, forces and
 * collisions. Without a Collider it has no effect.
 */
struct RigidBody {
  /// World-space velocity in m/s. PhysicsSystem writes it back every frame.
  bx::Vec3 velocity{0.0f};

  /// Extra world-space acceleration in m/s^2, applied on top of gravity.
  bx::Vec3 acceleration{0.0f};

  /// Whether world gravity (-9.81 m/s^2 on Y) applies.
  bool has_gravity{true};
};
