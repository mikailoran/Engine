#pragma once

#include "component_array.h"
#include "types.h"
#include <cassert>
#include <memory>
#include <unordered_map>

class ComponentManager {
public:
  ComponentManager() = default;

  template <class Component> void RegisterComponent();

  template <class Component> ComponentBit GetComponentBit() const;

  template <class Component>
  void AddComponent(Entity entity, Component component);

  template <class Component> void RemoveComponent(Entity entity);

  template <class Component> Component &GetComponent(Entity entity);

  template <class Component> const Component &GetComponent(Entity entity) const;

  void EntityDestroyed(Entity entity);

private:
  struct ComponentEntry {
    /// Represents the position of the bit in the signature bitset that
    /// represents a component
    ComponentBit bit{0};
    std::unique_ptr<ComponentArrayInterface> array;
  };
  template <class Component> ComponentArray<Component> &GetComponentArray();

  template <class Component>
  const ComponentArray<Component> &GetComponentArray() const;

  std::unordered_map<TypeKey, ComponentEntry> component_arrays_;
  ComponentBit next_bit_{0};
};

// Implementation

template <class Component> void ComponentManager::RegisterComponent() {
  const auto type_key = TypeKeyOf<Component>();
  assert(!component_arrays_.contains(type_key) &&
         "Registering component type more than once.");
  assert(next_bit_ < MAX_COMPONENTS && "Too many component types registered.");

  component_arrays_.try_emplace(type_key, next_bit_,
                                std::make_unique<ComponentArray<Component>>());
  ++next_bit_;
}

template <class Component>
ComponentBit ComponentManager::GetComponentBit() const {
  const auto type_key = TypeKeyOf<Component>();
  assert(component_arrays_.contains(type_key) &&
         "Getting component type of unregistered component.");

  return component_arrays_.at(type_key).bit;
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

inline void ComponentManager::EntityDestroyed(Entity entity) {
  for (const auto &[type_key, entry] : component_arrays_) {
    entry.array->EntityDestroyed(entity);
  }
}

template <class Component>
ComponentArray<Component> &ComponentManager::GetComponentArray() {
  const auto type_key = TypeKeyOf<Component>();
  assert(component_arrays_.contains(type_key) &&
         "Getting component array before component being registered.");

  return *static_cast<ComponentArray<Component> *>(
      component_arrays_.at(type_key).array.get());
}

template <class Component>
const ComponentArray<Component> &ComponentManager::GetComponentArray() const {
  const auto type_key = TypeKeyOf<Component>();
  assert(component_arrays_.contains(type_key) &&
         "Getting component array before component being registered.");

  return *static_cast<const ComponentArray<Component> *>(
      component_arrays_.at(type_key).array.get());
}
