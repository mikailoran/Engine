#include "ecs/core/entity_manager.h"

#include <cassert>

#include "ecs/core/types.h"

EntityManager::EntityManager() {
  for (EntityType entity_id = 0; entity_id < kMaxEntities; ++entity_id) {
    available_entities_.push(Entity{entity_id});
  }
}

Entity EntityManager::CreateEntity() {
  assert(living_entity_count_ < kMaxEntities &&
         "Too many entities in existence.");

  auto id = available_entities_.front();
  available_entities_.pop();
  ++living_entity_count_;
  alive_.at(id) = true;
  return id;
}

bool EntityManager::IsAlive(Entity entity) const {
  return entity < kMaxEntities && alive_.at(entity);
}

void EntityManager::DestroyEntity(Entity entity) {
  assert(entity < kMaxEntities && "Trying to destroy entity out of range.");
  // A guard rather than an assert to allow repeat requests
  if (!IsAlive(entity)) {
    return;
  }

  available_entities_.push(entity);
  --living_entity_count_;
  alive_.at(entity) = false;
}