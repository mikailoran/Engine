#include "ecs.h"

Entity Ecs::CreateEntity() { return entity_manager_.CreateEntity(); }

void Ecs::DestroyEntity(Entity entity) {
  entity_manager_.DestroyEntity(entity);
}

template <class Component> void Ecs::RegisterComponent() {
  component_manager_.RegisterComponent<Component>();
}

template <class Component>
void Ecs::AddComponent(Entity entity, Component component) {
  component_manager_.AddComponent(entity, component);

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

template <class Component> Component &Ecs::GetComponent(Entity entity) const {
  return component_manager_.GetComponent<Component>(entity);
}

template <class SystemClass>
std::unique_ptr<SystemClass> Ecs::RegisterSystem() {
  return system_manager_.RegisterSystem<SystemClass>();
}