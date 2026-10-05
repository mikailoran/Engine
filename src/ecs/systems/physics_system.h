#pragma once

class Ecs;
class PhysicsWorld;
struct FrameContext;

/**
 * @brief Keeps a PhysicsWorld body for every entity with a Collider and a
 * Transform, and steps the world at a fixed rate.
 *
 * A Collider alone makes a static body; with a RigidBody it is dynamic. Each
 * body's entity gets a PhysicsBody that only this system writes. Bodies are
 * created and destroyed as components come and go, edits made outside physics
 * are pushed in, and dynamic bodies' motion is read back.
 */
class Physics {
 public:
  /**
   * @brief Syncs bodies with the ECS, then advances the world at a fixed rate.
   *
   * Accumulates @p ctx.dt and drains it in whole @ref kFixedDt steps, so this
   * may run zero or many steps per frame. The remainder carries over.
   *
   * @param ecs World to read and write components through.
   * @param world Physics world holding the bodies.
   * @param ctx Per-frame inputs; only its variable @c dt is consumed.
   */
  void Update(Ecs& ecs, PhysicsWorld& world, const FrameContext& ctx);

 private:
  /// Simulation step length, in seconds (60 Hz).
  static constexpr float kFixedDt = 1.0F / 60.0F;

  /// Upper bound on a frame's delta, in seconds. Caps how many steps one hitch
  /// can queue, trading dropped simulated time for a bounded frame cost.
  static constexpr float kMaxFrameDt = 0.25F;

  /// Unsimulated seconds carried between frames.
  float accumulator_{0.0F};
};
