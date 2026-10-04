#pragma once

#include <array>
#include <cassert>
#include <concepts>
#include <limits>
#include <span>
#include <utility>

#include "ecs/core/types.h"

/** @brief Type-erased base so ComponentManager can hold every component array
 * together. */
class ComponentArrayInterface {
 public:
  virtual ~ComponentArrayInterface() = default;
  ComponentArrayInterface(const ComponentArrayInterface&) = delete;
  auto operator=(const ComponentArrayInterface&)
      -> ComponentArrayInterface& = delete;
  ComponentArrayInterface(ComponentArrayInterface&&) = delete;
  auto operator=(ComponentArrayInterface&&)
      -> ComponentArrayInterface& = delete;

  /** @brief Drops @p entity's component, if it has one. */
  virtual void EntityDestroyed(Entity entity) = 0;

 protected:
  ComponentArrayInterface() = default;
};

/**
 * @brief A type ComponentArray can store.
 *
 * Default-initializable because every slot is constructed up front and reset
 * on removal; movable because components are moved in and compacted.
 */
template <class T>
concept ComponentType = std::default_initializable<T> && std::movable<T>;

/**
 * @brief Sparse-set storage for one component type.
 *
 * Components are packed in [0, Size()); removal moves the last one into the
 * hole, so dense order is not entity-id order. Slots past Size() hold
 * default-constructed components, so removal releases what a component owns.
 */
template <ComponentType Component>
class ComponentArray : public ComponentArrayInterface {
 public:
  /** @brief Creates an empty component array with every entity marked absent.
   */
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

  /** @brief Removes and releases @p entity's component. @pre Has(entity). */
  void RemoveData(Entity entity);

  /** @brief Returns @p entity's component. @pre Has(entity) is true. */
  [[nodiscard]] auto GetData(Entity entity) const -> const Component&;

  /** @brief Returns @p entity's component. @pre Has(entity) is true. */
  auto GetData(Entity entity) -> Component&;

  /**
   * @brief Tests whether @p entity currently has a component in this array.
   *
   * Absence is a normal answer, not a caller error; only an id outside
   * [0, kMaxEntities) is, and throws std::out_of_range.
   */
  [[nodiscard]] auto Has(Entity entity) const -> bool;

  /** @brief Removes @p entity's component if present; a no-op otherwise. */
  void EntityDestroyed(Entity entity) override;

 private:
  std::size_t current_size_{0};
  // Marks an entity with no component in this array
  static constexpr auto kInvalidIndex = std::numeric_limits<std::size_t>::max();
  // TODO: Look up the cost of default initializing the arrays and alternatives
  std::array<std::size_t, kMaxEntities> sparse_{};
  std::array<Entity, kMaxEntities> dense_{};
  std::array<Component, kMaxEntities> components_{};
};

// Implementation

template <ComponentType Component>
ComponentArray<Component>::ComponentArray() {
  sparse_.fill(kInvalidIndex);
}

template <ComponentType Component>
auto ComponentArray<Component>::Size() const -> std::size_t {
  return current_size_;
}

template <ComponentType Component>
auto ComponentArray<Component>::Entities() const -> std::span<const Entity> {
  return std::span(dense_).first(Size());
}

template <ComponentType Component>
void ComponentArray<Component>::InsertData(Entity entity, Component component) {
  assert(!Has(entity) && "Component added to same entity more than once.");

  sparse_.at(entity) = current_size_;
  dense_.at(current_size_) = entity;
  components_.at(current_size_) = std::move(component);

  ++current_size_;
}

template <ComponentType Component>
void ComponentArray<Component>::RemoveData(Entity entity) {
  assert(Has(entity) && "Trying to remove non-existent component.");

  const auto removed_entity_index = sparse_.at(entity);
  const auto last_entity_index = current_size_ - 1;

  // Move the last element into the hole, unless the removed one is last
  if (removed_entity_index != last_entity_index) {
    components_.at(removed_entity_index) =
        std::move(components_.at(last_entity_index));

    const auto entity_of_last_element = dense_.at(last_entity_index);
    sparse_.at(entity_of_last_element) = removed_entity_index;
    dense_.at(removed_entity_index) = entity_of_last_element;
  }

  // Reset the vacated slot so it releases what it owned
  components_.at(last_entity_index) = Component{};

  // Remove the deleted entity from the sparse set
  sparse_.at(entity) = kInvalidIndex;

  --current_size_;
}

template <ComponentType Component>
auto ComponentArray<Component>::GetData(Entity entity) -> Component& {
  assert(Has(entity) &&
         "Trying to retrieve a component the entity does not have.");

  return components_.at(sparse_.at(entity));
}

template <ComponentType Component>
auto ComponentArray<Component>::GetData(Entity entity) const
    -> const Component& {
  assert(Has(entity) &&
         "Trying to retrieve a component the entity does not have.");

  return components_.at(sparse_.at(entity));
}

template <ComponentType Component>
auto ComponentArray<Component>::Has(Entity entity) const -> bool {
  return sparse_.at(entity) != kInvalidIndex;
}

template <ComponentType Component>
void ComponentArray<Component>::EntityDestroyed(Entity entity) {
  if (Has(entity)) {
    RemoveData(entity);
  }
}