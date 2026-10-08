#include "engine/ecs/core/ecs.h"

#include "engine/ecs/core/types.h"

namespace engine {

auto Ecs::CreateEntity() -> Entity { return entity_manager_.CreateEntity(); }

void Ecs::DestroyEntity(Entity entity) {
  // Filtering dead ids here keeps the queue short; it does not make the queue
  // duplicate-free, since the same live entity can be requested twice before
  // either request is applied. Flush handles that case.
  if (entity_manager_.IsAlive(entity)) {
    pending_destroy_.push_back(entity);
  }
}

void Ecs::Flush() {
  for (const auto& entity : pending_destroy_) {
    component_manager_.EntityDestroyed(entity);
    entity_manager_.DestroyEntity(entity);
  }
  pending_destroy_.clear();
}

}  // namespace engine
