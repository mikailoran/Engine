#pragma once

#include <bx/bounds.h>
#include <bx/math.h>

namespace engine {

/**
 * @brief Makes an entity a character: an upright capsule that physics moves
 * at the velocity gameplay gives it, sliding along what it hits and pushing
 * dynamic bodies. Needs a Transform, whose position is the capsule's feet.
 *
 * Physics owns the Transform's position, but never its rotation. A Collider
 * on the same entity is ignored.
 */
struct CharacterBody {
  /// Capsule height from feet to head, in m; more than twice the radius.
  float height{1.8F};
  /// Capsule radius, in m.
  float radius{0.3F};
  /// Steepest slope it can walk up, in degrees.
  float max_slope_deg{45.0F};
  /// Mass in kg: how hard it pushes and presses on what it stands on.
  float mass{70.0F};
  /// Strongest push it gives a body, in N.
  float push_force{200.0F};

  /// World-space velocity in m/s. Gameplay sets it; physics adds gravity and
  /// writes it back every frame.
  bx::Vec3 velocity{0.0F};
};

/** @brief The world-space capsule of @p body standing at @p feet. */
inline auto CharacterCapsule(const CharacterBody& body, const bx::Vec3& feet)
    -> bx::Capsule {
  // A capsule's ends are the centres of its rounded caps
  return {.pos = bx::add(feet, {0.0F, body.radius, 0.0F}),
          .end = bx::add(feet, {0.0F, body.height - body.radius, 0.0F}),
          .radius = body.radius};
}

}  // namespace engine
