#pragma once

#include "types.h"
#include <array>
#include <cassert>
#include <limits>
#include <span>
#include <utility>

/** @brief Type-erased base so ComponentManager can hold every pool together. */
class ComponentArrayInterface {
public:
  virtual ~ComponentArrayInterface() = default;
  /** @brief Drops @p entity's component, if it has one. */
  virtual void EntityDestroyed(Entity entity) = 0;
};

/**
 * @brief Sparse-set storage for one component type.
 *
 * Components are packed in [0, Size()); removal swaps the last one into the
 * hole, so dense order is not entity-id order.
 */
template <class Component>
class ComponentArray : public ComponentArrayInterface {
public:
  /** @brief Creates an empty pool with every entity marked absent. */
  ComponentArray();

  /** @brief Number of entities that have this component. */
  [[nodiscard]] auto Size() const -> std::size_t;

  /**
   * @brief Entities that have this component, in dense order.
   * @return View invalidated by InsertData and RemoveData.
   */
  [[nodiscard]] auto Entities() const -> std::span<const Entity>;

  /** @brief Adds @p entity's component. @pre Has(entity) is false. */
  void InsertData(Entity entity, Component component);

  /** @brief Removes @p entity's component. @pre Has(entity) is true. */
  void RemoveData(Entity entity);

  /** @brief Returns @p entity's component. @pre Has(entity) is true. */
  const Component &GetData(Entity entity) const;

  /** @brief Returns @p entity's component. @pre Has(entity) is true. */
  Component &GetData(Entity entity);

  /**
   * @brief Tests whether @p entity currently has a component in this array.
   *
   * Absence is a normal answer, not a caller error; only an id outside
   * [0, kMaxEntities) is, and throws std::out_of_range.
   */
  [[nodiscard]] bool Has(Entity entity) const;

  /** @brief Removes @p entity's component if present; a no-op otherwise. */
  void EntityDestroyed(Entity entity) override;

private:
  std::size_t current_size_{0};
  // Marks an entity with no component in this pool
  static constexpr auto kInvalidIndex = std::numeric_limits<std::size_t>::max();
  std::array<std::size_t, kMaxEntities> sparse_;
  std::array<Entity, kMaxEntities> dense_;
  std::array<Component, kMaxEntities> components_;
};

// Implementation

template <class Component> ComponentArray<Component>::ComponentArray() {
  sparse_.fill(kInvalidIndex);
}

template <class Component>
auto ComponentArray<Component>::Size() const -> std::size_t {
  return current_size_;
}

template <class Component>
auto ComponentArray<Component>::Entities() const -> std::span<const Entity> {
  return {dense_.cbegin(), dense_.cbegin() + Size()};
}

template <class Component>
void ComponentArray<Component>::InsertData(Entity entity, Component component) {
  assert(!Has(entity) && "Component added to same entity more than once.");

  sparse_.at(entity) = current_size_;
  dense_.at(current_size_) = entity;
  components_.at(current_size_) = std::move(component);

  ++current_size_;
}

template <class Component>
void ComponentArray<Component>::RemoveData(Entity entity) {
  assert(Has(entity) && "Trying to remove non-existent component.");

  // Swap deleted element with last element of component array
  const auto removed_entity_index = sparse_.at(entity);
  const auto last_entity_index = current_size_ - 1;
  // TODO: self swap would corrupt entities with non-trivial move semantics
  std::swap(components_.at(removed_entity_index),
            components_.at(last_entity_index));

  // Update sparse set arrays to reflect change to moved entity
  const auto entity_of_last_element = dense_.at(last_entity_index);
  sparse_.at(entity_of_last_element) = removed_entity_index;
  dense_.at(removed_entity_index) = entity_of_last_element;

  // Remove the deleted entity from the sparse set
  sparse_.at(entity) = kInvalidIndex;

  --current_size_;
}

template <class Component>
Component &ComponentArray<Component>::GetData(Entity entity) {
  assert(Has(entity) &&
         "Trying to retrieve a component the entity does not have.");

  return components_.at(sparse_.at(entity));
}

template <class Component>
const Component &ComponentArray<Component>::GetData(Entity entity) const {
  assert(Has(entity) &&
         "Trying to retrieve a component the entity does not have.");

  return components_.at(sparse_.at(entity));
}

template <class Component>
bool ComponentArray<Component>::Has(Entity entity) const {
  return sparse_.at(entity) != kInvalidIndex;
}

template <class Component>
void ComponentArray<Component>::EntityDestroyed(Entity entity) {
  if (Has(entity)) {
    RemoveData(entity);
  }
}