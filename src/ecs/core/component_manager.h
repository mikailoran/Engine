#pragma once

#include "component_array.h"
#include "types.h"
#include <cassert>
#include <memory>
#include <unordered_map>

// TODO: Mixup between ComponentType and std::size_t

class ComponentManager {
public:
  template <class Component> void RegisterComponent();

  template <class Component> ComponentType GetComponentType() const;

  template <class Component>
  void AddComponent(Entity entity, Component component);

  template <class Component> void RemoveComponent(Entity entity);

  template <class Component> Component &GetComponent(Entity entity);

  void EntityDestroyed(Entity entity);

private:
  template <class Component> ComponentArray<Component> &GetComponentArray();

  std::unordered_map<ComponentType, std::unique_ptr<ComponentArrayInterface>>
      component_arrays_;
};

template <class Component> void ComponentManager::RegisterComponent() {
  const auto type = TypeId::Get<Component>();
  assert(!component_arrays_.contains(type) &&
         "Registering component type more than once.");

  component_arrays_.emplace(type,
                            std::make_unique<ComponentArray<Component>>());
}

template <class Component>
ComponentType ComponentManager::GetComponentType() const {
  return TypeId::Get<Component>();
}

template <class Component>
void ComponentManager::AddComponent(Entity entity, Component component) {
  GetComponentArray<Component>().InsertComponent(entity, component);
}

template <class Component>
void ComponentManager::RemoveComponent(Entity entity) {
  GetComponentArray<Component>().RemoveComponent(entity);
}

template <class Component>
Component &ComponentManager::GetComponent(Entity entity) {
  GetComponentArray<Component>().GetComponent(entity);
}

void ComponentManager::EntityDestroyed(Entity entity) {}

template <class Component>
ComponentArray<Component> &ComponentManager::GetComponentArray() {
  const auto type = TypeId::Get<Component>();
  assert(component_arrays_.contains(type) &&
         "Component type used before being registered.");

  return *static_cast<ComponentArray<Component> *>(
      component_arrays_.at(type).get());
}
