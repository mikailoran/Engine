#pragma once

#include "component_manager.h"
#include "entity_manager.h"
#include "system_manager.h"
#include "types.h"

class Ecs {
public:
  Ecs() = default;

  Entity CreateEntity();

  void DestroyEntity(Entity entity);

  template <class Component> void RegisterComponent();

  template <class Component>
  void AddComponent(Entity entity, Component component);

  template <class Component> void RemoveComponent(Entity entity);

  template <class Component> Component &GetComponent(Entity entity);

  template <class Component> const Component &GetComponent(Entity entity) const;

  template <class Component> ComponentType GetComponentType() const;

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
  auto signature_component_bit =
      component_manager_.GetComponentType<Component>();
  entity_signature.set(signature_component_bit, true);
  entity_manager_.SetSignature(entity, entity_signature);

  system_manager_.EntitySignatureChanged(entity, entity_signature);
}

template <class Component> void Ecs::RemoveComponent(Entity entity) {
  component_manager_.RemoveComponent<Component>(entity);

  auto entity_signature = entity_manager_.signature(entity);
  auto signature_component_bit =
      component_manager_.GetComponentType<Component>();
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

template <class Component> ComponentType Ecs::GetComponentType() const {
  return component_manager_.GetComponentType<Component>();
}

template <class SystemClass> SystemClass &Ecs::RegisterSystem() {
  return system_manager_.RegisterSystem<SystemClass>();
}

template <class SystemClass> void Ecs::SetSystemSignature(Signature signature) {
  system_manager_.SetSignature<SystemClass>(signature);
}