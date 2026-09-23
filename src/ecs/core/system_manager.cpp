#include "system_manager.h"

void SystemManager::EntityDestoyed(Entity entity) {
  for (const auto &[id, system] : systems_) {
    system->entities.erase(entity);
  }
}

void SystemManager::EntitySignatureChanged(Entity entity,
                                           Signature entity_signature) {
  for (const auto &[type, system] : systems_) {
    const auto system_signature = system->signature;

    // Systems with an empty signature are not handled
    if (system_signature.none()) {
      continue;
    }

    // Check if the entity has AT LEAST the same bits enabled as the system
    if ((entity_signature & system_signature) == system_signature) {
      system->entities.insert(entity);
    } else {
      system->entities.erase(entity);
    }
  }
}
