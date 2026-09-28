#pragma once

#include "component_manager.h"
#include "entity_manager.h"
#include "system_manager.h"
#include "types.h"

#include <vector>

class Ecs {
public:
  Ecs() = default;

  Entity CreateEntity();

  /**
   * @brief Requests removal of an entity: its components, its entry in every
   *        system, and its id.
   *
   * Uses deffered destruction. Flush() is run after system updates to run the
   * destruction process on all entities marked for deletion.
   *
   * Idempotent: queueing an entity twice, or naming one that is already dead,
   * destroys it once and is otherwise a no-op.
   *
   * @param entity Entity to destroy. Must be < MAX_ENTITIES; the id need not
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

  template <class Component> void RegisterComponent();

  template <class Component>
  void AddComponent(Entity entity, Component component);

  template <class Component> void RemoveComponent(Entity entity);

  template <class Component> Component &GetComponent(Entity entity);

  template <class Component> const Component &GetComponent(Entity entity) const;

  /**
   * @brief Tests whether @p entity has a component of type @p Component.
   *
   * The way to check for absence: GetComponent asserts when the component is
   * missing, so it cannot answer this question.
   *
   * @tparam Component Registered component type to look for.
   * @param entity Entity to test.
   * @return True if the entity has that component.
   */
  template <class Component>
  [[nodiscard]] bool HasComponent(Entity entity) const;

  template <class Component> ComponentBit GetComponentBit() const;

  template <class SystemClass> SystemClass &RegisterSystem();

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

template <class SystemClass> SystemClass &Ecs::RegisterSystem() {
  return system_manager_.RegisterSystem<SystemClass>();
}

template <class SystemClass> void Ecs::SetSystemSignature(Signature signature) {
  system_manager_.SetSignature<SystemClass>(signature);
}