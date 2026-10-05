#pragma once

#include <bx/math.h>

#include <unordered_map>

#include "ecs/core/types.h"
#include "physics/physics_world.h"

class AssetRegistry;
class Ecs;
struct Collider;
struct FrameContext;
struct Mesh;
struct RigidBody;
struct Transform;

/**
 * @brief Keeps a PhysicsWorld body per Collider entity and steps the world at
 * a fixed rate.
 *
 * A Collider alone makes a static body; with a RigidBody it is dynamic. Bodies
 * follow the ECS: they are created, reshaped and destroyed as components come
 * and go, and Transform or RigidBody edits made outside physics are pushed in.
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
   * @param assets Registry the colliders' meshes are sized from.
   * @param ctx Per-frame inputs; only its variable @c dt is consumed.
   */
  void Update(Ecs& ecs, PhysicsWorld& world, const AssetRegistry& assets,
              const FrameContext& ctx);

 private:
  /** @brief What Physics last wrote to, or read from, one entity's body. */
  struct BodyRecord {
    BodyHandle body;
    bool dynamic{false};
    bool has_gravity{true};
    /// Scale the shape was built for; a change reshapes the body.
    bx::Vec3 scale{1.0F};
    bx::Vec3 position{0.0F};
    bx::Quaternion rotation{bx::InitIdentity};
    bx::Vec3 velocity{0.0F};
    bx::Vec3 acceleration{0.0F};
    float restitution{0.0F};
    float friction{0.0F};
  };

  /**
   * @brief Destroys bodies whose entity or required components are gone, or
   * whose Collider was replaced.
   */
  void RemoveStaleBodies(Ecs& ecs, PhysicsWorld& world);

  /** @brief Creates a body for each Collider that has none. */
  void CreateBodies(Ecs& ecs, PhysicsWorld& world, const AssetRegistry& assets);

  /**
   * @brief Pushes edits made outside physics into the existing bodies: pose,
   * scale, material, and RigidBody state.
   */
  void PushEdits(Ecs& ecs, PhysicsWorld& world, const AssetRegistry& assets);

  /** @brief Pushes pose and scale edits. */
  static void PushPoseAndShape(PhysicsWorld& world, BodyRecord& record,
                               const Transform& transform, const Mesh& mesh);

  /** @brief Pushes restitution and friction edits. */
  static void PushMaterial(PhysicsWorld& world, BodyRecord& record,
                           const Collider& collider);

  /**
   * @brief Pushes RigidBody edits; adding or removing one switches the body
   * between static and dynamic. @p rigid_body is null when absent.
   */
  static void PushMotion(PhysicsWorld& world, BodyRecord& record,
                         const RigidBody* rigid_body);

  /** @brief Copies dynamic bodies' poses and velocities back to the ECS. */
  void ReadBack(Ecs& ecs, const PhysicsWorld& world);

  /// Simulation step length, in seconds (60 Hz).
  static constexpr float kFixedDt = 1.0F / 60.0F;

  /// Upper bound on a frame's delta, in seconds. Caps how many steps one hitch
  /// can queue, trading dropped simulated time for a bounded frame cost.
  static constexpr float kMaxFrameDt = 0.25F;

  /// One record per entity that has a body.
  std::unordered_map<Entity, BodyRecord> bodies_;

  /// Unsimulated seconds carried between frames.
  float accumulator_{0.0F};
};
