#include "ecs.h"

Entity Ecs::CreateEntity() { return entity_manager_.CreateEntity(); }

void Ecs::DestroyEntity(Entity entity) {
  component_manager_.EntityDestroyed(entity);
  system_manager_.EntityDestroyed(entity);
  entity_manager_.DestroyEntity(entity);
}
