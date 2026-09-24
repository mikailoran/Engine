#pragma once

#include "../core/system.h"
#include "../core/types.h"

#include <vector>

class Ecs;
struct FrameContext;
struct AssetRegistry;

class UiSystem : public System {
public:
  void Init(AssetRegistry &asset_registry);

  Entity SpawnEntity(Ecs &ecs);

  void DestroyAllSpawnedEntities(Ecs &ecs);

  void Update(Ecs &ecs, const FrameContext &ctx);

  void Shutdown();

private:
  AssetRegistry *asset_registry_{nullptr};
  std::vector<Entity> entities_;
};