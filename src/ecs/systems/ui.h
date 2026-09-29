#pragma once

#include "../core/system.h"
#include "../core/types.h"

class Ecs;
class AssetRegistry;
struct FrameContext;

class UiSystem : public System {
public:
  void Init(AssetRegistry &asset_registry);

  Entity SpawnEntity(Ecs &ecs);

  void Update(Ecs &ecs, const FrameContext &ctx);

  void Shutdown();

private:
  AssetRegistry *asset_registry_{nullptr};
};