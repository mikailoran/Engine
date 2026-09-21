#pragma once

#include "types.h"
#include <array>
#include <cassert>
#include <unordered_map>

// TODO: Can any of the member functions be separated into a cpp file?

class ComponentArrayInterface {
public:
  virtual ~ComponentArrayInterface() = default;
  virtual void EntityDestroyed(Entity entity) = 0;
};

template <class Component>
class ComponentArray : public ComponentArrayInterface {
public:
  void InsertComponent(Entity entity, Component component);

  void RemoveComponent(Entity entity);

  Component &GetComponent(Entity entity);

  void EntityDestroyed(Entity entity) override;

private:
  std::array<Component, MAX_ENTITIES> components_{};
  std::unordered_map<std::size_t, Entity> index_to_entity_;
  std::unordered_map<Entity, std::size_t> entity_to_index_;
  std::size_t current_size_{0};
};

template <class Type>
void ComponentArray<Type>::InsertComponent(Entity entity, Type component) {
  assert(!entity_to_index_.contains(entity) &&
         "Component added to same entity more than once.");

  components_.at(current_size_) = component;
  entity_to_index_.at(entity) = current_size_;
  index_to_entity_.at(current_size_) = entity;

  ++current_size_;
}

template <class Type>
void ComponentArray<Type>::RemoveComponent(Entity entity) {
  assert(entity_to_index_.contains(entity) &&
         "Trying to remove non-existant component.");

  // Swap deleted element with last element of component array
  const auto removed_entity_index = entity_to_index_.at(entity);
  const auto last_entity_index = current_size_ - 1;
  // TODO: What happens if we swap an element with itself?
  std::swap(components_.at(removed_entity_index),
            components_.at(last_entity_index));

  // Update maps to reflect change to moved entity
  const auto entity_of_last_element = index_to_entity_.at(last_entity_index);
  entity_to_index_.at(entity_of_last_element) = removed_entity_index;
  index_to_entity_.at(removed_entity_index) = entity_of_last_element;

  // Remove the deleted entity from the maps
  entity_to_index_.erase(entity);
  index_to_entity_.erase(last_entity_index);

  --current_size_;
}

template <class Type> Type &ComponentArray<Type>::GetComponent(Entity entity) {
  assert(entity_to_index_.contains(entity) &&
         "Trying to retrieve non-existent component.");

  const auto entity_index = entity_to_index_.at(entity);
  return components_.at(entity_index);
}