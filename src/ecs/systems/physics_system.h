#pragma once

#include "../core/system.h"

class Ecs;
struct FrameContext;

class Physics : public System {
public:
  void Init();

  /**
   * @brief Handles tracked entities' physics.
   *
   * @param ecs World to read and write components through.
   * @param ctx Per-frame inputs.
   */
  // TODO: Switch from passing ECS directly, to Views (apparently a lot of work)
  void Update(Ecs &ecs, const FrameContext &ctx);

private:
};