#pragma once

#include "system.h"
#include "types.h"
#include <cassert>
#include <memory>
#include <unordered_map>

/** @brief Owns every registered system, one instance per type. */
class SystemManager {
public:
  SystemManager() = default;

  /** @brief Creates @p SystemClass. @pre Not registered yet. */
  template <class SystemClass> SystemClass &RegisterSystem();

private:
  std::unordered_map<TypeKey, std::unique_ptr<System>> systems_;
};

template <class SystemClass> SystemClass &SystemManager::RegisterSystem() {
  const auto type_key = TypeKeyOf<SystemClass>();

  assert(!systems_.contains(type_key) && "Trying to register more than once.");

  auto system = std::make_unique<SystemClass>();
  auto &ref = *system;
  systems_.emplace(type_key, std::move(system));
  return ref;
}
