#pragma once

class Ecs;
struct FrameContext;

class Physics {
 public:
  Physics() = default;
  Physics(const Physics&) = delete;
  auto operator=(const Physics&) -> Physics& = delete;

  void Init();

  /**
   * @brief Advances tracked entities' physics at a fixed rate.
   *
   * Accumulates @p ctx.dt and drains it in whole @ref kFixedDt steps, so this
   * may run Step() zero or many times per frame. The remainder carries over.
   *
   * @param ecs World to read and write components through.
   * @param ctx Per-frame inputs; only its variable @c dt is consumed.
   */
  // TODO: Switch from passing ECS directly, to Views (apparently a lot of work)
  void Update(Ecs& ecs, const FrameContext& ctx);

 private:
  /**
   * @brief Simulates exactly one step.
   *
   * @param ecs World to read and write components through.
   * @param fixed_dt Seconds to advance by; always @ref kFixedDt.
   */
  void Step(Ecs& ecs, float fixed_dt);

  /// Simulation step length, in seconds (60 Hz).
  static constexpr float kFixedDt = 1.0F / 60.0F;

  /// Upper bound on a frame's delta, in seconds. Caps how many steps one hitch
  /// can queue, trading dropped simulated time for a bounded frame cost.
  static constexpr float kMaxFrameDt = 0.25F;

  /// Unsimulated seconds carried between frames.
  float accumulator_{0.0F};

  static constexpr float kGravity = -9.81F;
  /// Restitution factor when bouncing on the floor.
  static constexpr float kRestitutionFactor = 0.6F;
  /// Rest threshold to avoid jitters.
  static constexpr float kRestThreshold = 0.2F;
};
