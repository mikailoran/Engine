#include "engine/ecs/systems/physics_query.h"

#include <bx/bounds.h>

#include <optional>

#include "engine/ecs/components/character_link.h"
#include "engine/ecs/components/physics_link.h"
#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"
#include "engine/physics/physics_world.h"

namespace engine {

namespace {

/** @brief Tests whether @p hit's body or character still belongs to @p entity.
 */
auto LinkedTo(Ecs& ecs, Entity entity, const physics::RayHit& hit) -> bool {
  if (hit.body.IsValid()) {
    const auto* link = ecs.TryGetComponent<PhysicsLink>(entity);
    return link != nullptr && link->Body() == hit.body;
  }
  const auto* link = ecs.TryGetComponent<CharacterLink>(entity);
  return link != nullptr && link->Character() == hit.character;
}

}  // namespace

auto CastRay(Ecs& ecs, const physics::PhysicsWorld& world, const bx::Ray& ray,
             float max_distance, std::optional<Entity> ignore)
    -> std::optional<RayHit> {
  // Hits come nearest first; skip the ignored entity and orphaned bodies
  for (const physics::RayHit& hit : world.CastRay(ray, max_distance)) {
    const auto entity = static_cast<Entity>(hit.user_data);
    if (entity == ignore || !LinkedTo(ecs, entity, hit)) {
      continue;
    }
    return RayHit{
        .entity = entity, .distance = hit.distance, .point = hit.point};
  }
  return std::nullopt;
}

}  // namespace engine
