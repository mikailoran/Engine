#pragma once

#include "system.h"
#include "types.h"
#include <memory>
#include <unordered_map>

class SystemManager {
public:
  SystemManager() = default;

  template <class SystemClass> void RegisterSystem();

  void EntityDestoyed(Entity entity);

  void EntitySignatureChanged(Entity entity, Signature entity_signature);

private:
  std::unordered_map<std::size_t, std::unique_ptr<System>> systems_;
};

template <class SystemClass> void SystemManager::RegisterSystem() {
  const auto type_id = TypeId::Get<SystemClass>();

  assert(!systems_.contains(type_id) && "Trying to register more than once.");

  systems_.emplace(type_id, std::make_unique<SystemClass>());
}