#pragma once

#include "component_array.h"
#include "types.h"

#include <cassert>
#include <cstddef>
#include <span>
#include <tuple>

/**
 * @brief Non-owning view over the entities that have every listed component.
 *
 * Usage: `ecs.View<Transform, Spin>().Each([](Entity e, Transform &t, Spin &s)
 * { ... });`
 *
 * @tparam Components Component types an entity must all have to be visited.
 */
template <class... Components> class View {
  static_assert(sizeof...(Components) > 0, "A view needs at least one type.");

public:
  /** @brief Views @p component_arrays, which must outlive the view. */
  explicit View(ComponentArray<Components> &...component_arrays);

  /**
   * @brief Calls fn(Entity, Components &...) for each matching entity.
   *
   * Visits in the smallest component array's dense order, not entity-id order.
   * @pre @p fn adds or removes none of the viewed components.
   */
  template <class Fn> auto Each(Fn &&fn) const -> void;

private:
  std::tuple<ComponentArray<Components> *...> component_arrays_;
};

// Implementation

template <class... Components>
View<Components...>::View(ComponentArray<Components> &...component_arrays)
    : component_arrays_(&component_arrays...) {}

template <class... Components>
template <class Fn>
auto View<Components...>::Each(Fn &&fn) const -> void {
  // Drive from the smallest component array to minimize Has checks
  std::span<const Entity> driver = std::get<0>(component_arrays_)->Entities();
  std::apply(
      [&driver](const auto *...component_arrays) {
        ((driver = component_arrays->Size() < driver.size()
                       ? component_arrays->Entities()
                       : driver),
         ...);
      },
      component_arrays_);

  // Sum of all viewed array sizes, to detect structural changes
  const auto total_size = [this] {
    return std::apply(
        [](const auto *...component_arrays) {
          return (component_arrays->Size() + ...);
        },
        component_arrays_);
  };

  // Only read by the assert, hence unused in Release
  [[maybe_unused]] const std::size_t total_size_before = total_size();

  for (const Entity entity : driver) {
    assert(total_size() == total_size_before &&
           "Viewed components added or removed during Each.");

    // Skip entities missing any of the other components
    const bool matches = std::apply(
        [entity](const auto *...component_arrays) {
          return (component_arrays->Has(entity) && ...);
        },
        component_arrays_);
    if (!matches) {
      continue;
    }
    // Hand the entity and one reference per component to fn
    std::apply(
        [&fn, entity](auto *...component_arrays) {
          fn(entity, component_arrays->GetData(entity)...);
        },
        component_arrays_);
  }
}
