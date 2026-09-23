#include "entity_manager.h"
#include <cassert>

EntityManager::EntityManager() {
  for (EntityType entity_id = 0; entity_id < MAX_ENTITIES; ++entity_id) {
    available_entities_.push(Entity{entity_id});
  }
}

Entity EntityManager::CreateEntity() {
  assert(living_entity_count_ < MAX_ENTITIES &&
         "Too many entities in existence.");

  auto id = available_entities_.front();
  available_entities_.pop();
  ++living_entity_count_;
  return id;
}

Signature EntityManager::signature(Entity entity) {
  return signatures_.at(entity);
}

void EntityManager::SetSignature(Entity entity, Signature signature) {
  assert(entity < MAX_ENTITIES && "Entity out of range.");

  signatures_.at(entity) = signature;
}

void EntityManager::DestroyEntity(Entity entity) {
  assert(entity < MAX_ENTITIES && "Entity out of range.");

  available_entities_.push(entity);
  signatures_.at(entity).reset();
  --living_entity_count_;
}