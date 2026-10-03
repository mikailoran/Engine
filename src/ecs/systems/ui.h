#pragma once

#include "ecs/core/types.h"

class Ecs;
class AssetRegistry;
struct FrameContext;

class UiSystem {
 public:
  UiSystem() = default;
  UiSystem(const UiSystem&) = delete;
  UiSystem& operator=(const UiSystem&) = delete;

  void Init(AssetRegistry& asset_registry);

  Entity SpawnEntity(Ecs& ecs);

  void Update(Ecs& ecs, const FrameContext& ctx);

  void Shutdown();

 private:
  AssetRegistry* asset_registry_{nullptr};
};