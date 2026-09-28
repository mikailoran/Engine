#include "ecs.h"

Entity Ecs::CreateEntity() { return entity_manager_.CreateEntity(); }

void Ecs::DestroyEntity(Entity entity) {
  if (entity_manager_.IsAlive(entity)) {
    pending_destroy_.push_back(entity);
  }
}

void Ecs::Flush() {
  for (const auto &entity : pending_destroy_) {
    component_manager_.EntityDestroyed(entity);
    system_manager_.EntityDestroyed(entity);
    entity_manager_.DestroyEntity(entity);
  }
  pending_destroy_.clear();
}
