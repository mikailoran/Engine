#pragma once

#include "component_manager.h"
#include "entity_manager.h"
#include "system_manager.h"
#include "types.h"

class Ecs {
public:
  Ecs() = default;

  Entity CreateEntity();

  /**
   * @brief Removes an entity: its components, its entry in every system, and
   *        its id.
   *
   * @warning Unsafe to call from inside a system's Update on the entity
   *          currently being iterated. SystemManager::EntityDestroyed erases
   *          from System::entities, which invalidates the iterator a range-for
   *          is holding, and the next ++ is undefined. Destroying a *different*
   *          entity is fine: std::set only invalidates iterators to the erased
   *          element. Until this is addressed, destroy only between system
   *          Updates.
   *
   * @param entity Entity to destroy. Must be alive; destroying twice silently
   *               corrupts the id pool, see EntityManager::DestroyEntity.
   */
  // TODO: deferred destruction (queue here, flush between Updates) would make
  // this callable from anywhere, including from within a system's Update.
  void DestroyEntity(Entity entity);

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