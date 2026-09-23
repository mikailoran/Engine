#pragma once

#include "../core/system.h"

class Ecs;
struct FrameContext;

class UiSystem : public System {
public:
  void Init();

  void Update(Ecs &ecs, const FrameContext &ctx);

  void Shutdown();

private:
};