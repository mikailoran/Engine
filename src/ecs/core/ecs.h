#pragma once

#include <utility>
#include <vector>

#include "ecs/core/component_manager.h"
#include "ecs/core/entity_manager.h"
#include "ecs/core/types.h"
#include "ecs/core/view.h"

/** @brief Facade over the entity, component and system managers. */
class Ecs {
 public:
  Ecs() = default;

  /**
   * @brief Creates an entity with no components.
   * @throws std::length_error If kMaxEntities entities are alive.
   */
  auto CreateEntity() -> Entity;

  /**
   * @brief Requests removal of an entity: its components and its id.
   *
   * Uses deferred destruction. Flush() is run after system updates to run the
   * destruction process on all entities marked for deletion.
   *
   * Idempotent: queueing an entity twice, or naming one that is already dead,
   * destroys it once and is otherwise a no-op.
   *
   * @param entity Entity to destroy. Must be < kMaxEntities; the id need not
   *               be alive.
   */
  // TODO: rename to RequestDestroyEntity?
  void DestroyEntity(Entity entity);

  /**
   * @brief Applies every queued destruction, then empties the queue.
   *
   * The single sync point for structural change. Call it between system
   * Updates, never during one, so that no system observes the world
   * mid-teardown and every system sees the same entity set for a whole frame.
   */
  void Flush();

  /** @brief Registers @p Component. @pre Not registered yet. */
  template <class Component>
  void RegisterComponent();

  /** @brief Gives @p entity a @p Component. @pre It does not have one yet. */
  template <class Component>
  void AddComponent(Entity entity, Component component);

  /** @brief Removes @p entity's @p Component. @pre It has one. */
  template <class Component>
  void RemoveComponent(Entity entity);

  /** @brief Returns @p entity's @p Component. @pre It has one. */
  template <class Component>
  auto GetComponent(Entity entity) -> Component&;

  /** @brief Returns @p entity's @p Component. @pre It has one. */
  template <class Component>
  auto GetComponent(Entity entity) const -> const Component&;

  /** @brief Tests whether @p entity has a @p Component. @pre Registered. */
  template <class Component>
  [[nodiscard]] auto HasComponent(Entity entity) const -> bool;

  /**
   * @brief Views the entities that have every one of @p Components.
   * @pre Every type in @p Components is registered.
   */
  template <class... Components>
  auto View() -> ::View<Components...>;

 private:
  EntityManager entity_manager_;
  ComponentManager component_manager_;
  std::vector<Entity> pending_destroy_;
};

// Implementation

template <class Component>
void Ecs::RegisterComponent() {
  component_manager_.RegisterComponent<Component>();
}

template <class Component>
void Ecs::AddComponent(Entity entity, Component component) {
  component_manager_.AddComponent(entity, std::move(component));
}

template <class Component>
void Ecs::RemoveComponent(Entity entity) {
  component_manager_.RemoveComponent<Component>(entity);
}

template <class Component>
auto Ecs::GetComponent(Entity entity) -> Component& {
  return component_manager_.GetComponent<Component>(entity);
}

template <class Component>
auto Ecs::GetComponent(Entity entity) const -> const Component& {
  return component_manager_.GetComponent<Component>(entity);
}

template <class Component>
auto Ecs::HasComponent(Entity entity) const -> bool {
  return component_manager_.HasComponent<Component>(entity);
}

template <class... Components>
auto Ecs::View() -> ::View<Components...> {
  return component_manager_.View<Components...>();
}
