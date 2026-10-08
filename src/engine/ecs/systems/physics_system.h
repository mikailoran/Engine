#pragma once

namespace engine {

class CharacterLink;
class Ecs;
class PhysicsLink;
namespace physics {
class PhysicsWorld;
}  // namespace physics
struct CharacterBody;
struct FrameContext;
struct RigidBody;

/**
 * @brief Keeps a PhysicsWorld body for every entity with a Collider and a
 * Transform, a character for every CharacterBody and Transform, and steps the
 * world at a fixed rate.
 *
 * A Collider alone makes a static body; with a RigidBody it is dynamic. Each
 * body's entity gets a PhysicsLink, and each character's a CharacterLink, that
 * only this system writes. Bodies and characters are created and destroyed as
 * components come and go, edits made outside physics are pushed in, and
 * motion is read back.
 */
class PhysicsSystem {
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
  void Update(Ecs& ecs, physics::PhysicsWorld& world, const FrameContext& ctx);

 private:
  // Static: steps reach PhysicsLink's privates through this class's friendship

  /**
   * @brief Destroys bodies that no PhysicsLink points to: their entity was
   * destroyed, or its id was reused by an entity not yet given a body.
   */
  static void SweepOrphans(Ecs& ecs, physics::PhysicsWorld& world);

  /**
   * @brief Unlinks entities that lost their Collider or Transform, destroying
   * their bodies. Also drops a PhysicsLink that does not own its body, such as
   * a copy, so the entity gets its own.
   */
  static void DetachBodies(Ecs& ecs, physics::PhysicsWorld& world);

  /** @brief Creates a body for each Collider and Transform entity without one.
   */
  static void AttachBodies(Ecs& ecs, physics::PhysicsWorld& world);

  /**
   * @brief Pushes edits made outside physics into each body, then refreshes the
   * link's copies to match.
   */
  static void PushEdits(Ecs& ecs, physics::PhysicsWorld& world);

  /**
   * @brief Pushes RigidBody edits; adding or removing one switches the body
   * between static and dynamic. @p rigid_body is null when absent.
   */
  static void PushMotion(physics::PhysicsWorld& world, PhysicsLink& link,
                         const RigidBody* rigid_body);

  /** @brief Copies dynamic bodies' poses and velocities back to the ECS. */
  static void PullResults(Ecs& ecs, const physics::PhysicsWorld& world);

  /** @brief Destroys characters that no CharacterLink points to. */
  static void SweepOrphanCharacters(Ecs& ecs, physics::PhysicsWorld& world);

  /**
   * @brief Unlinks entities that lost their CharacterBody or Transform,
   * destroying their characters, and drops links that don't own theirs.
   */
  static void DetachCharacters(Ecs& ecs, physics::PhysicsWorld& world);

  /** @brief Creates a character for each CharacterBody entity without one. */
  static void AttachCharacters(Ecs& ecs, physics::PhysicsWorld& world);

  /**
   * @brief Pushes position, velocity and size edits into each character; a
   * size edit rebuilds it, keeping its velocity.
   */
  static void PushCharacterEdits(Ecs& ecs, physics::PhysicsWorld& world);

  /** @brief Copies characters' feet, velocities and ground state back. */
  static void PullCharacters(Ecs& ecs, const physics::PhysicsWorld& world);

  /// Simulation step length, in seconds (60 Hz).
  static constexpr float kFixedDt = 1.0F / 60.0F;

  /// Upper bound on a frame's delta, in seconds. Caps how many steps one hitch
  /// can queue, trading dropped simulated time for a bounded frame cost.
  static constexpr float kMaxFrameDt = 0.25F;

  /// Unsimulated seconds carried between frames.
  float accumulator_{0.0F};
};

}  // namespace engine
