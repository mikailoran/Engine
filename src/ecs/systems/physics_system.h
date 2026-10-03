#pragma once

class Ecs;
struct FrameContext;

/** @brief Fixed-step gravity, floor bounce and spin for rigid bodies. */
class Physics {
 public:
  /**
   * @brief Advances tracked entities' physics at a fixed rate.
   *
   * Accumulates @p ctx.dt and drains it in whole @ref kFixedDt steps, so this
   * may run zero or many steps per frame. The remainder carries over.
   *
   * @param ecs World to read and write components through.
   * @param ctx Per-frame inputs; only its variable @c dt is consumed.
   */
  void Update(Ecs& ecs, const FrameContext& ctx);

 private:
  /// Simulation step length, in seconds (60 Hz).
  static constexpr float kFixedDt = 1.0F / 60.0F;

  /// Upper bound on a frame's delta, in seconds. Caps how many steps one hitch
  /// can queue, trading dropped simulated time for a bounded frame cost.
  static constexpr float kMaxFrameDt = 0.25F;

  /// Unsimulated seconds carried between frames.
  float accumulator_{0.0F};
};
