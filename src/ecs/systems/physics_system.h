#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <bx/math.h>

#include <unordered_map>

#include "ecs/core/types.h"
#include "physics/jolt_runtime.h"
#include "physics/layers.h"

class AssetRegistry;
class Ecs;
struct Collider;
struct FrameContext;
struct Mesh;
struct RigidBody;
struct Transform;

/**
 * @brief Simulates every Collider entity as a Jolt body, at a fixed rate.
 *
 * A Collider alone makes a static body; with a RigidBody it is dynamic. Bodies
 * follow the ECS: they are created, rebuilt and destroyed as components come
 * and go, and Transform or RigidBody edits made outside physics are pushed in.
 */
class Physics {
 public:
  /**
   * @brief Brings up an empty Jolt physics world.
   * @param runtime Jolt's process-wide state; must outlive this. Only taken to
   * prove Jolt is set up.
   */
  explicit Physics(const JoltRuntime& runtime);

  /**
   * @brief Syncs bodies with the ECS, then advances them at a fixed rate.
   *
   * Accumulates @p ctx.dt and drains it in whole @ref kFixedDt steps, so this
   * may run zero or many steps per frame. The remainder carries over.
   *
   * @param ecs World to read and write components through.
   * @param assets Registry the colliders' meshes are sized from.
   * @param ctx Per-frame inputs; only its variable @c dt is consumed.
   */
  void Update(Ecs& ecs, const AssetRegistry& assets, const FrameContext& ctx);

 private:
  /** @brief What Physics last wrote to, or read from, one entity's body. */
  struct BodyRecord {
    JPH::BodyID id;
    bool dynamic{false};
    bool has_gravity{true};
    /// Scale the shape was built for; a change reshapes the body.
    bx::Vec3 scale{1.0F};
    bx::Vec3 position{0.0F};
    bx::Quaternion rotation{bx::InitIdentity};
    bx::Vec3 velocity{0.0F};
    float restitution{0.0F};
    float friction{0.0F};
  };

  /**
   * @brief Destroys bodies whose entity or required components are gone, or
   * whose Collider was replaced.
   */
  void RemoveStaleBodies(Ecs& ecs);

  /** @brief Creates a body for each Collider that has none. */
  void CreateBodies(Ecs& ecs, const AssetRegistry& assets);

  /**
   * @brief Pushes edits made outside physics into the existing bodies: pose,
   * scale, material, and a RigidBody being added or removed.
   */
  void PushEdits(Ecs& ecs, const AssetRegistry& assets);

  /** @brief Pushes pose and scale edits, waking what the body touched. */
  void PushPoseAndShape(BodyRecord& record, const Transform& transform,
                        const Mesh& mesh);

  /** @brief Pushes restitution and friction edits. */
  void PushMaterial(BodyRecord& record, const Collider& collider);

  /**
   * @brief Pushes RigidBody edits; adding or removing one switches the body
   * between static and dynamic. @p rigid_body is null when absent.
   */
  void PushMotion(BodyRecord& record, const RigidBody* rigid_body);

  /** @brief Applies each RigidBody's acceleration over one step. */
  void ApplyAccelerations(Ecs& ecs);

  /** @brief Copies dynamic bodies' poses and velocities back to the ECS. */
  void ReadBack(Ecs& ecs);

  /// Simulation step length, in seconds (60 Hz).
  static constexpr float kFixedDt = 1.0F / 60.0F;

  /// Upper bound on a frame's delta, in seconds. Caps how many steps one hitch
  /// can queue, trading dropped simulated time for a bounded frame cost.
  static constexpr float kMaxFrameDt = 0.25F;

  /// Scratch memory Jolt uses within one step, in bytes.
  static constexpr JPH::uint kTempAllocatorBytes = 10U * 1024U * 1024U;

  // The world references the layers, so it must follow them
  CollisionLayers layers_;
  JPH::PhysicsSystem world_;
  JPH::TempAllocatorImpl temp_allocator_{kTempAllocatorBytes};

  /// One record per entity that has a body.
  std::unordered_map<Entity, BodyRecord> bodies_;

  /// Unsimulated seconds carried between frames.
  float accumulator_{0.0F};

  // Last, where its 64-byte alignment adds no padding. One thread per core
  // but one by default
  // TODO: learn about job systems and how to integrate ours to Jolt's
  JPH::JobSystemThreadPool job_system_{JPH::cMaxPhysicsJobs,
                                       JPH::cMaxPhysicsBarriers};
};
