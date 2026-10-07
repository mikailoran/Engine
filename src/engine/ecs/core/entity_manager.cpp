#include "engine/ecs/core/entity_manager.h"

#include <cassert>
#include <stdexcept>

#include "engine/ecs/core/types.h"

namespace engine {

EntityManager::EntityManager() {
  for (EntityType entity_id = 0; entity_id < kMaxEntities; ++entity_id) {
    available_entities_.push(Entity{entity_id});
  }
}

auto EntityManager::CreateEntity() -> Entity {
  assert(living_entity_count_ < kMaxEntities &&
         "Too many entities in existence.");
  // front() on an empty queue is UB once the assert is compiled out
  if (available_entities_.empty()) {
    throw std::length_error("entity limit reached");
  }

  auto id = available_entities_.front();
  available_entities_.pop();
  ++living_entity_count_;
  alive_.at(id) = true;
  return id;
}

auto EntityManager::IsAlive(Entity entity) const -> bool {
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

}  // namespace engine
