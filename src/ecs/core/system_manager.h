#pragma once

#include "system.h"
#include "types.h"
#include <cassert>
#include <memory>
#include <unordered_map>

class SystemManager {
public:
  SystemManager() = default;

  template <class SystemClass> SystemClass &RegisterSystem();

  // TODO: Not needed, signatures are stored in the System itself
  template <class SystemClass> void SetSignature(Signature signature);

  void EntityDestoyed(Entity entity);

  void EntitySignatureChanged(Entity entity, Signature entity_signature);

private:
  std::unordered_map<std::size_t, std::unique_ptr<System>> systems_;
};

template <class SystemClass> SystemClass &SystemManager::RegisterSystem() {
  const auto type_id = TypeId::Get<SystemClass>();

  assert(!systems_.contains(type_id) && "Trying to register more than once.");

  auto system = std::make_unique<SystemClass>();
  auto &ref = *system;
  systems_.emplace(type_id, std::move(system));
  return ref;
}

template <class SystemClass>
void SystemManager::SetSignature(Signature signature) {
  // TODO: Do something
}
