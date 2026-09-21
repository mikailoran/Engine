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

  template <class Component> Component &GetComponent(Entity entity) const;

  template <class SystemClass> std::unique_ptr<SystemClass> RegisterSystem();

private:
  EntityManager entity_manager_;
  ComponentManager component_manager_;
  SystemManager system_manager_;
};