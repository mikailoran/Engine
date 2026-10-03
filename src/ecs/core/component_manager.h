#pragma once

#include <cassert>
#include <memory>
#include <unordered_map>

#include "component_array.h"
#include "types.h"
#include "view.h"

class ComponentManager {
 public:
  ComponentManager() = default;

  template <class Component>
  void RegisterComponent();

  template <class Component>
  void AddComponent(Entity entity, Component component);

  template <class Component>
  void RemoveComponent(Entity entity);

  template <class Component>
  Component& GetComponent(Entity entity);

  template <class Component>
  const Component& GetComponent(Entity entity) const;

  template <class Component>
  [[nodiscard]] bool HasComponent(Entity entity) const;

  /** @brief Views the entities having all @p Components. @pre Registered. */
  // ::View: inside this class, plain View names this member
  template <class... Components>
  auto View() -> ::View<Components...>;

  void EntityDestroyed(Entity entity);

 private:
  template <class Component>
  ComponentArray<Component>& GetComponentArray();

  template <class Component>
  const ComponentArray<Component>& GetComponentArray() const;

  std::unordered_map<TypeKey, std::unique_ptr<ComponentArrayInterface>>
      component_arrays_;
};

// Implementation

template <class Component>
void ComponentManager::RegisterComponent() {
  const auto type_key = TypeKeyOf<Component>();
  assert(!component_arrays_.contains(type_key) &&
         "Registering component type more than once.");

  component_arrays_.try_emplace(type_key,
                                std::make_unique<ComponentArray<Component>>());
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
Component& ComponentManager::GetComponent(Entity entity) {
  return GetComponentArray<Component>().GetData(entity);
}

template <class Component>
const Component& ComponentManager::GetComponent(Entity entity) const {
  return GetComponentArray<Component>().GetData(entity);
}

/**
 * @brief Tests whether @p entity has a component of type @p Component.
 *
 * @p Component must already be registered: this answers "does this entity have
 * one", not "is this type known". An unregistered type trips
 * GetComponentArray's assert.
 *
 * @tparam Component Registered component type to look for.
 * @param entity Entity to test.
 * @return True if the entity has that component.
 */
template <class Component>
bool ComponentManager::HasComponent(Entity entity) const {
  return GetComponentArray<Component>().Has(entity);
}

template <class... Components>
auto ComponentManager::View() -> ::View<Components...> {
  return ::View<Components...>(GetComponentArray<Components>()...);
}

inline void ComponentManager::EntityDestroyed(Entity entity) {
  for (const auto& [type_key, array] : component_arrays_) {
    array->EntityDestroyed(entity);
  }
}

template <class Component>
ComponentArray<Component>& ComponentManager::GetComponentArray() {
  const auto type_key = TypeKeyOf<Component>();
  assert(component_arrays_.contains(type_key) &&
         "Getting component array before component being registered.");

  return *static_cast<ComponentArray<Component>*>(
      component_arrays_.at(type_key).get());
}

template <class Component>
const ComponentArray<Component>& ComponentManager::GetComponentArray() const {
  const auto type_key = TypeKeyOf<Component>();
  assert(component_arrays_.contains(type_key) &&
         "Getting component array before component being registered.");

  return *static_cast<const ComponentArray<Component>*>(
      component_arrays_.at(type_key).get());
}
