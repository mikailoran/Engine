#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <functional>
#include <ranges>
#include <span>
#include <tuple>

#include "engine/ecs/core/component_array.h"
#include "engine/ecs/core/types.h"

// TODO: Make View become a range. Currently, we can only pass functions to
// views (internal iteration). Making View returning ranges would allow external
// iteration and early exits in systems where iterating over all entities is not
// necessary (LightSystem).

// TODO: Add const overload to views. Useful for const Ecs & and read only
// systems. Warning: TypeKeyOf<Transform>() != TypeKeyOf<const Transform>()

/**
 * @brief Non-owning view over the entities that have every listed component.
 *
 * Usage: `ecs.View<Transform, RigidBody>().ForEach(
 * [](Entity e, Transform& t, RigidBody& b) -> void { ... });`
 *
 * @tparam Components Distinct component types an entity must all have.
 */
template <class... Components>
class View {
  static_assert(sizeof...(Components) > 0, "A view needs at least one type.");

 public:
  /** @brief Views @p component_arrays, which must outlive the view. */
  explicit View(ComponentArray<Components>&... component_arrays);

  /**
   * @brief Calls fn(Entity, Components &...) for each matching entity.
   *
   * Visits in the smallest component array's dense order, not entity-id order.
   * @pre @p fn adds or removes none of the viewed components.
   */
  template <std::invocable<Entity, Components&...> Fn>
  auto ForEach(Fn fn) const -> void;

 private:
  /** @brief Returns the viewed component array for @p Component. */
  template <class Component>
  [[nodiscard]] auto ComponentArrayOf() const -> ComponentArray<Component>&;

  std::tuple<ComponentArray<Components>*...> component_arrays_;
};

// Implementation

template <class... Components>
View<Components...>::View(ComponentArray<Components>&... component_arrays)
    : component_arrays_(&component_arrays...) {}

template <class... Components>
template <std::invocable<Entity, Components&...> Fn>
auto View<Components...>::ForEach(Fn fn) const -> void {
  // Drive from the smallest component array to minimize Has checks
  const std::array candidates{ComponentArrayOf<Components>().Entities()...};
  const std::span<const Entity> driver =
      *std::ranges::min_element(candidates, {}, std::ranges::size);

  // Sum of all viewed array sizes, to detect structural changes
  const auto total_size = [this]() -> std::size_t {
    return (ComponentArrayOf<Components>().Size() + ...);
  };

  // Only read by the assert, hence unused in Release
  [[maybe_unused]] const std::size_t total_size_before = total_size();

  for (const Entity entity : driver) {
    assert(total_size() == total_size_before &&
           "Viewed components added or removed during ForEach.");

    // Skip entities missing any of the other components
    if (!(ComponentArrayOf<Components>().Has(entity) && ...)) {
      continue;
    }
    // Hand the entity and one reference per component to fn
    std::invoke(fn, entity, ComponentArrayOf<Components>().GetData(entity)...);
  }
}

template <class... Components>
template <class Component>
auto View<Components...>::ComponentArrayOf() const
    -> ComponentArray<Component>& {
  return *std::get<ComponentArray<Component>*>(component_arrays_);
}
