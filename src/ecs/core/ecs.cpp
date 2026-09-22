#include "ecs.h"

Entity Ecs::CreateEntity() { return entity_manager_.CreateEntity(); }

void Ecs::DestroyEntity(Entity entity) {
  entity_manager_.DestroyEntity(entity);
}
