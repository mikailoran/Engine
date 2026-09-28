#pragma once

#include "types.h"

#include <array>
#include <queue>

class EntityManager {
public:
  EntityManager();

  Entity CreateEntity();

  [[nodiscard]] Signature signature(Entity entity) const;

  void SetSignature(Entity entity, Signature signature);

  [[nodiscard]] bool IsAlive(Entity entity) const;

  void DestroyEntity(Entity entity);

private:
  std::queue<Entity> available_entities_;
  std::array<Signature, MAX_ENTITIES> signatures_{};
  EntityType living_entity_count_{0};
  std::array<bool, MAX_ENTITIES> alive_{};
};