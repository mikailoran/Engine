#pragma once

#include <bx/bounds.h>
#include <bx/math.h>

#include <optional>

#include "engine/ecs/core/types.h"

namespace engine {

class Ecs;
namespace physics {
class PhysicsWorld;
}  // namespace physics

/** @brief Output of CastRay: where a ray met an entity's body or character. */
struct RayHit {
  Entity entity{};
  /// Distance from the ray's origin, in m.
  float distance{0.0F};
  /// World-space hit point.
  bx::Vec3 point{0.0F};
};

/**
 * @brief Finds the nearest entity whose body or character @p ray hits.
 *
 * Sees the poses of the last physics step: an entity created or teleported
 * since is found where physics last put it, or not at all. Bodies whose
 * entity was destroyed but not yet swept are skipped.
 *
 * @param ray Origin and unit-length direction, in world space.
 * @param max_distance How far the ray reaches, in m.
 * @param ignore An entity never hit, usually the one casting; a ray from
 *        inside its capsule would otherwise hit it at 0 m.
 * @return Empty on a miss.
 */
auto CastRay(Ecs& ecs, const physics::PhysicsWorld& world, const bx::Ray& ray,
             float max_distance, std::optional<Entity> ignore = std::nullopt)
    -> std::optional<RayHit>;

}  // namespace engine
