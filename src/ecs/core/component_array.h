#pragma once

#include "types.h"
#include <array>
#include <cassert>
#include <unordered_map>

class ComponentArrayInterface {
public:
  virtual ~ComponentArrayInterface() = default;
  virtual void EntityDestroyed(Entity entity) = 0;
};

template <class Component>
class ComponentArray : public ComponentArrayInterface {
public:
  ComponentArray() = default;

  void InsertData(Entity entity, Component component);

  void RemoveData(Entity entity);

  const Component &GetData(Entity entity) const;

  Component &GetData(Entity entity);

  void EntityDestroyed(Entity entity) override;

private:
  std::array<Component, MAX_ENTITIES> components_{};
  std::unordered_map<std::size_t, Entity> index_to_entity_;
  std::unordered_map<Entity, std::size_t> entity_to_index_;
  std::size_t current_size_{0};
};

template <class Type>
void ComponentArray<Type>::InsertData(Entity entity, Type component) {
  assert(!entity_to_index_.contains(entity) &&
         "Component added to same entity more than once.");

  components_.at(current_size_) = component;
  // TODO: is this the most optimal way?
  entity_to_index_[entity] = current_size_;
  index_to_entity_[current_size_] = entity;

  ++current_size_;
}

template <class Type> void ComponentArray<Type>::RemoveData(Entity entity) {
  assert(entity_to_index_.contains(entity) &&
         "Trying to remove non-existant component.");

  // Swap deleted element with last element of component array
  const auto removed_entity_index = entity_to_index_.at(entity);
  const auto last_entity_index = current_size_ - 1;
  std::swap(components_.at(removed_entity_index),
            components_.at(last_entity_index));

  // Update maps to reflect change to moved entity
  const auto entity_of_last_element = index_to_entity_.at(last_entity_index);
  entity_to_index_[entity_of_last_element] = removed_entity_index;
  index_to_entity_[removed_entity_index] = entity_of_last_element;

  // Remove the deleted entity from the maps
  entity_to_index_.erase(entity);
  index_to_entity_.erase(last_entity_index);

  --current_size_;
}

template <class Type> Type &ComponentArray<Type>::GetData(Entity entity) {
  assert(entity_to_index_.contains(entity) &&
         "Trying to retrieve component data from non-existent entity.");

  const auto entity_index = entity_to_index_.at(entity);
  return components_.at(entity_index);
}

template <class Type>
const Type &ComponentArray<Type>::GetData(Entity entity) const {
  assert(entity_to_index_.contains(entity) &&
         "Trying to retrieve component data from non-existent entity.");

  const auto entity_index = entity_to_index_.at(entity);
  return components_.at(entity_index);
}

template <class Type>
void ComponentArray<Type>::EntityDestroyed(Entity entity) {
  if (entity_to_index_.contains(entity)) {
    RemoveData(entity);
  }
}