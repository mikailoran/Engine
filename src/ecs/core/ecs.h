#pragma once

#include "component_manager.h"
#include "entity_manager.h"
#include "system_manager.h"
#include "types.h"
#include "view.h"

#include <vector>

/** @brief Facade over the entity, component and system managers. */
class Ecs {
public:
  Ecs() = default;

  /** @brief Creates an entity with no components. */
  Entity CreateEntity();

  /**
   * @brief Requests removal of an entity: its components, its entry in every
   *        system, and its id.
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
  template <class Component> void RegisterComponent();

  /** @brief Creates and registers a system. @return The Ecs-owned system. */
  template <class SystemClass> SystemClass &RegisterSystem();

  /** @brief Gives @p entity a @p Component. @pre It does not have one yet. */
  template <class Component>
  void AddComponent(Entity entity, Component component);

  /** @brief Removes @p entity's @p Component. @pre It has one. */
  template <class Component> void RemoveComponent(Entity entity);

  /** @brief Returns @p entity's @p Component. @pre It has one. */
  template <class Component> Component &GetComponent(Entity entity);

  /** @brief Returns @p entity's @p Component. @pre It has one. */
  template <class Component> const Component &GetComponent(Entity entity) const;

  /** @brief Tests whether @p entity has a @p Component. @pre Registered. */
  template <class Component>
  [[nodiscard]] bool HasComponent(Entity entity) const;

  /** @brief Returns @p Component's bit in Signature. @pre Registered. */
  template <class Component> ComponentBit GetComponentBit() const;

  /**
   * @brief Views the entities that have every one of @p Components.
   * @pre Every type in @p Components is registered.
   */
  template <class... Components> auto View() -> ::View<Components...>;

  /** @brief Sets the components @p SystemClass requires to track an entity. */
  template <class SystemClass> void SetSystemSignature(Signature signature);

private:
  EntityManager entity_manager_;
  ComponentManager component_manager_;
  SystemManager system_manager_;
  std::vector<Entity> pending_destroy_;
};

// Implementation

template <class Component> void Ecs::RegisterComponent() {
  component_manager_.RegisterComponent<Component>();
}

template <class SystemClass> SystemClass &Ecs::RegisterSystem() {
  return system_manager_.RegisterSystem<SystemClass>();
}

template <class Component>
void Ecs::AddComponent(Entity entity, Component component) {
  component_manager_.AddComponent(entity, component);

  // Mark the entity's signature with the component's ID
  auto entity_signature = entity_manager_.signature(entity);
  const auto signature_component_bit =
      component_manager_.GetComponentBit<Component>();
  entity_signature.set(signature_component_bit, true);
  entity_manager_.SetSignature(entity, entity_signature);

  system_manager_.EntitySignatureChanged(entity, entity_signature);
}

template <class Component> void Ecs::RemoveComponent(Entity entity) {
  component_manager_.RemoveComponent<Component>(entity);

  auto entity_signature = entity_manager_.signature(entity);
  const auto signature_component_bit =
      component_manager_.GetComponentBit<Component>();
  entity_signature.set(signature_component_bit, false);
  entity_manager_.SetSignature(entity, entity_signature);

  system_manager_.EntitySignatureChanged(entity, entity_signature);
}

template <class Component> Component &Ecs::GetComponent(Entity entity) {
  return component_manager_.GetComponent<Component>(entity);
}

template <class Component>
const Component &Ecs::GetComponent(Entity entity) const {
  return component_manager_.GetComponent<Component>(entity);
}

template <class Component> bool Ecs::HasComponent(Entity entity) const {
  return component_manager_.HasComponent<Component>(entity);
}

template <class Component> ComponentBit Ecs::GetComponentBit() const {
  return component_manager_.GetComponentBit<Component>();
}

template <class... Components> auto Ecs::View() -> ::View<Components...> {
  return component_manager_.View<Components...>();
}

template <class SystemClass> void Ecs::SetSystemSignature(Signature signature) {
  system_manager_.SetSignature<SystemClass>(signature);
}