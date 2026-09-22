#pragma once

#include "../core/system.h"

class Ecs;
struct FrameContext;

/// Applies each entity's Spin to its Transform. Matches {Transform, Spin}.
class Physics : public System {
public:
  void Init();

  /**
   * @brief Advances every tracked entity's rotation by one frame.
   *
   * @param ecs World to read and write components through.
   * @param ctx Per-frame inputs; only dt is used.
   */
  // TODO: Switch from passing ECS directly, to Views (apparently a lot of work)
  void Update(Ecs &ecs, const FrameContext &ctx);

private:
};