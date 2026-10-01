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

  [[nodiscard]] bool Has(Entity entity) const;

  void EntityDestroyed(Entity entity) override;

private:
  // TODO: Replace maps i->e and e->i by sparse sets and compare performance
  std::array<Component, MAX_ENTITIES> components_{};
  std::unordered_map<std::size_t, Entity> index_to_entity_;
  std::unordered_map<Entity, std::size_t> entity_to_index_;
  std::size_t current_size_{0};
};

// Implementation

template <class Component>
void ComponentArray<Component>::InsertData(Entity entity, Component component) {
  assert(!entity_to_index_.contains(entity) &&
         "Component added to same entity more than once.");

  components_.at(current_size_) = component;
  // TODO: is this the most optimal way? Answer: No, arrays are
  entity_to_index_[entity] = current_size_;
  index_to_entity_[current_size_] = entity;

  ++current_size_;
}

template <class Component>
void ComponentArray<Component>::RemoveData(Entity entity) {
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

template <class Component>
Component &ComponentArray<Component>::GetData(Entity entity) {
  assert(entity_to_index_.contains(entity) &&
         "Trying to retrieve component data from non-existent entity.");

  const auto entity_index = entity_to_index_.at(entity);
  return components_.at(entity_index);
}

template <class Component>
const Component &ComponentArray<Component>::GetData(Entity entity) const {
  assert(entity_to_index_.contains(entity) &&
         "Trying to retrieve component data from non-existent entity.");

  const auto entity_index = entity_to_index_.at(entity);
  return components_.at(entity_index);
}

/**
 * @brief Tests whether @p entity currently has a component in this array.
 *
 * Unlike GetData this has no precondition: absence is a normal answer, not a
 * caller error.
 *
 * @param entity Entity to test.
 * @return True if the entity has a component of this type.
 */
template <class Component> bool ComponentArray<Component>::Has(Entity entity) const {
  return entity_to_index_.contains(entity);
}

template <class Component>
void ComponentArray<Component>::EntityDestroyed(Entity entity) {
  if (entity_to_index_.contains(entity)) {
    RemoveData(entity);
  }
}