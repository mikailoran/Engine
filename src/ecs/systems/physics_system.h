#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include "physics/jolt_runtime.h"
#include "physics/layers.h"

class Ecs;
struct FrameContext;

/** @brief Fixed-step gravity and floor bounce for rigid bodies. */
class Physics {
 public:
  /** @brief Brings up Jolt and an empty Jolt physics world. */
  Physics();

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

  /// Scratch memory Jolt uses within one step, in bytes.
  static constexpr JPH::uint kTempAllocatorBytes = 10U * 1024U * 1024U;

  // Order matters: Jolt's runtime backs everything below, and the world
  // references the layers, so the runtime comes first and the world last
  JoltRuntime runtime_;
  JPH::TempAllocatorImpl temp_allocator_{kTempAllocatorBytes};
  // Defaults to one worker thread per core but one
  JPH::JobSystemThreadPool job_system_{JPH::cMaxPhysicsJobs,
                                       JPH::cMaxPhysicsBarriers};
  CollisionLayers layers_;
  JPH::PhysicsSystem world_;

  /// Unsimulated seconds carried between frames.
  float accumulator_{0.0F};
};
