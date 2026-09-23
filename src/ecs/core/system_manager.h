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

  template <class SystemClass> void SetSignature(Signature signature);

  void EntityDestroyed(Entity entity);

  void EntitySignatureChanged(Entity entity, Signature entity_signature);

private:
  std::unordered_map<std::size_t, std::unique_ptr<System>> systems_;
};

template <class SystemClass> SystemClass &SystemManager::RegisterSystem() {
  const auto type_id = SystemTypeId::Get<SystemClass>();

  assert(!systems_.contains(type_id) && "Trying to register more than once.");

  auto system = std::make_unique<SystemClass>();
  auto &ref = *system;
  systems_.emplace(type_id, std::move(system));
  return ref;
}

/**
 * @brief Sets which components an entity must have for @p SystemClass to track
 *        it.
 *
 * Must be called before any entity gains the components in @p signature:
 * EntitySignatureChanged fills System::entities. Currently, a signature set
 * late leaves the system with an empty entity set and no diagnostic.
 *
 * @tparam SystemClass System to configure; must already be registered.
 * @param signature Component mask the system requires.
 */
template <class SystemClass>
void SystemManager::SetSignature(Signature signature) {
  const auto type_id = SystemTypeId::Get<SystemClass>();

  assert(systems_.contains(type_id) &&
         "Setting signature of unregistered system.");

  systems_.at(type_id)->signature = signature;
}
