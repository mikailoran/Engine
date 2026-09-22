#pragma once

#include "component_array.h"
#include "types.h"
#include <cassert>
#include <memory>
#include <unordered_map>

// TODO: Mixup between ComponentType and std::size_t

class ComponentManager {
public:
  ComponentManager() = default;

  template <class Component> void RegisterComponent();

  template <class Component> ComponentType GetComponentType() const;

  template <class Component>
  void AddComponent(Entity entity, Component component);

  template <class Component> void RemoveComponent(Entity entity);

  template <class Component> Component &GetComponent(Entity entity);

  template <class Component> const Component &GetComponent(Entity entity) const;

  // void EntityDestroyed(Entity entity);

private:
  template <class Component> ComponentArray<Component> &GetComponentArray();

  template <class Component>
  const ComponentArray<Component> &GetComponentArray() const;

  std::unordered_map<ComponentType, std::unique_ptr<ComponentArrayInterface>>
      component_arrays_;
};

// Implementation

template <class Component> void ComponentManager::RegisterComponent() {
  const auto type = TypeId::Get<Component>();
  assert(!component_arrays_.contains(type) &&
         "Registering component type more than once.");

  component_arrays_.emplace(type,
                            std::make_unique<ComponentArray<Component>>());
}

template <class Component>
ComponentType ComponentManager::GetComponentType() const {
  // TODO: Possible bug if too many components or removing components
  // The component's position in the signature bitset could be incorrect
  return TypeId::Get<Component>();
}

template <class Component>
void ComponentManager::AddComponent(Entity entity, Component component) {
  GetComponentArray<Component>().InsertData(entity, component);
}

template <class Component>
void ComponentManager::RemoveComponent(Entity entity) {
  GetComponentArray<Component>().RemoveData(entity);
}

template <class Component>
Component &ComponentManager::GetComponent(Entity entity) {
  return GetComponentArray<Component>().GetData(entity);
}

template <class Component>
const Component &ComponentManager::GetComponent(Entity entity) const {
  return GetComponentArray<Component>().GetData(entity);
}

// void ComponentManager::EntityDestroyed(Entity entity) {}

template <class Component>
ComponentArray<Component> &ComponentManager::GetComponentArray() {
  const auto type = TypeId::Get<Component>();
  assert(component_arrays_.contains(type) &&
         "Component type used before being registered.");

  return *static_cast<ComponentArray<Component> *>(
      component_arrays_.at(type).get());
}

template <class Component>
const ComponentArray<Component> &ComponentManager::GetComponentArray() const {
  const auto type = TypeId::Get<Component>();
  assert(component_arrays_.contains(type) &&
         "Component type used before being registered.");

  return *static_cast<const ComponentArray<Component> *>(
      component_arrays_.at(type).get());
}
