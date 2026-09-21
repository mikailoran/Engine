#pragma once

#include "component_array.h"
#include "types.h"
#include <memory>
#include <unordered_map>

class ComponentManager {
public:
  template <class ComponentClass> void RegisterComponent();

  template <class ComponentClass> ComponentType GetComponentType() const;

  template <class ComponentClass>
  void AddComponent(Entity entity, ComponentClass component);

  template <class ComponentClass> void RemoveComponentOf(Entity entity);

  template <class ComponentClass> ComponentClass &GetComponentOf(Entity entity);

  void EntityDestroyed(Entity entity);

private:
  template <class ComponentClass>
  std::unique_ptr<ComponentArray<ComponentType>> GetComponentArray();

  ComponentType next_component_type_{};
  std::unordered_map<std::size_t, ComponentType> component_types_;
  std::unordered_map<std::size_t, std::unique_ptr<ComponentArrayInterface>>
      component_arrays_;
};

template <class ComponentClass> void ComponentManager::RegisterComponent() {}
