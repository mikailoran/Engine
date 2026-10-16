#pragma once

#include <bx/bounds.h>
#include <bx/math.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "engine/physics/body_handle.h"
#include "engine/physics/character_handle.h"
#include "engine/physics/collision_mesh_handle.h"
#include "engine/physics/hitbox_handle.h"
#include "engine/physics/pose.h"  // IWYU pragma: export
#include "engine/physics/ragdoll.h"
#include "engine/physics/shape.h"

namespace engine::physics {

class JoltRuntime;

/** @brief Whether a body moves under forces and collisions. */
enum class Motion : std::uint8_t { kStatic, kDynamic };

/// Caller's tag for a body or character; physics stores it, never reads it.
using UserTag = std::uint64_t;

/** @brief Input to CreateBody: everything needed to create a body. */
struct BodyDesc {
  ShapeDesc shape;
  Pose pose;
  /// Applied to the shape: sizes and offset scale per axis.
  bx::Vec3 scale{1.0F};
  Motion motion{Motion::kStatic};
  Material material;
  /// Initial linear velocity in m/s; dynamic bodies only.
  bx::Vec3 velocity{0.0F};
  /// Whether world gravity applies; dynamic bodies only.
  bool gravity{true};
  /// Caller's tag for the body, returned by Bodies and UserData.
  UserTag user_data{0};
};

/** @brief Output of Bodies: a live body and the tag it was created with. */
struct BodyEntry {
  BodyHandle body;
  UserTag user_data{0};
};

/**
 * @brief Input to CreateCharacter: an upright capsule that moves at the
 * velocity it is given, sliding along what it hits.
 */
struct CharacterDesc {
  /// Where the capsule's bottom touches, in world space.
  bx::Vec3 feet{0.0F};
  /// Capsule height from feet to head, in m; more than twice the radius.
  float height{1.8F};
  /// Capsule radius, in m.
  float radius{0.3F};
  /// Steepest slope it can walk up, in degrees.
  float max_slope_deg{45.0F};
  /// Mass in kg: how hard it pushes and presses on what it stands on.
  float mass{70.0F};
  /// Strongest push it gives a body, in N.
  float push_force{200.0F};
  /// What rays hit instead of the capsule; invalid: the capsule.
  HitboxHandle hitbox{HitboxHandle::kInvalid};
  /// Whether CastRay hits it at all; false when something else stands in
  /// for it, such as its ragdoll's parts.
  bool ray_target{true};
  /// Caller's tag for the character, returned by Characters.
  UserTag user_data{0};
};

/** @brief Output of GetCharacter: a character's state after the last Step. */
struct CharacterState {
  bx::Vec3 feet{0.0F};
  /// The velocity it moves with in m/s, before collisions slow it.
  bx::Vec3 velocity{0.0F};
  /// Whether it stands on walkable ground.
  bool on_ground{false};
};

/** @brief Output of Characters: a live character and its creation tag. */
struct CharacterEntry {
  CharacterHandle character;
  UserTag user_data{0};
};

/** @brief Output of CastRay: where a ray met a body or a character. */
struct RayHit {
  /// Tag of what was hit, as given at creation.
  UserTag user_data{0};
  /// The body hit; invalid when a character or a ragdoll was hit.
  BodyHandle body;
  /// The character hit; invalid when a body or a ragdoll was hit.
  CharacterHandle character;
  /// The ragdoll whose part was hit; invalid otherwise.
  RagdollHandle ragdoll;
  /// The part hit, an index into its RagdollDesc::parts; 0 unless a ragdoll
  /// was hit.
  std::uint32_t part{0};
  /// Distance from the ray's origin, in m; 0 if it starts inside a shape.
  float distance{0.0F};
  /// World-space hit point.
  bx::Vec3 point{0.0F};
};

/**
 * @brief Rigid-body simulation behind an engine-typed API; Jolt stays inside.
 *
 * Edits wake the bodies they affect: destroying, moving or reshaping a body
 * wakes what touched it, so nothing sleeps on in mid-air. Needs a JoltRuntime
 * that outlives it.
 */
class PhysicsWorld {
 public:
  /**
   * @brief Creates an empty world.
   * @param runtime Jolt's process-wide state; must outlive this.
   * @param max_bodies Bodies the world can hold; CreateBody fails past it.
   */
  PhysicsWorld(const JoltRuntime& runtime, std::uint32_t max_bodies);

  /** @brief Destroys the world and every body still in it. */
  ~PhysicsWorld();

  // Jolt keeps internal references into the world: it stays put
  PhysicsWorld(const PhysicsWorld&) = delete;
  auto operator=(const PhysicsWorld&) -> PhysicsWorld& = delete;
  PhysicsWorld(PhysicsWorld&&) = delete;
  auto operator=(PhysicsWorld&&) -> PhysicsWorld& = delete;

  /**
   * @brief Creates a body and adds it to the world. A mesh body is static
   * whatever @p desc's motion, and can never turn dynamic.
   * @return The body, or an invalid handle if Jolt is out of bodies.
   */
  auto CreateBody(const BodyDesc& desc) -> BodyHandle;

  /**
   * @brief Builds a collision mesh for kMesh shapes to share. It lives until
   * the world is destroyed.
   * @param mesh Copied; free to discard afterwards.
   * @return The mesh, or an invalid handle if Jolt rejected the triangles.
   */
  auto CreateCollisionMesh(const TriangleMesh& mesh) -> CollisionMeshHandle;

  /**
   * @brief Builds a hitbox for characters to share: one convex hull per
   * part, hit only by CastRay. It lives until the world is destroyed.
   *
   * Parts that make no hull are skipped.
   * @param parts In the hitbox's space, whose origin goes at the feet.
   * @return The hitbox, or an invalid handle if no part made a hull.
   */
  auto CreateHitbox(const std::vector<ConvexPart>& parts) -> HitboxHandle;

  /** @brief Removes and destroys @p body, waking what touched it. */
  void DestroyBody(BodyHandle body);

  /** @brief Teleports @p body, waking what touched its old and new place. */
  void SetPose(BodyHandle body, const Pose& pose);

  /**
   * @brief Reshapes @p body in place, keeping its id and velocity. A mesh
   * only fits a body created with a mesh.
   * @param scale Applied to @p shape: sizes and offset scale per axis.
   */
  void SetShape(BodyHandle body, const ShapeDesc& shape, const bx::Vec3& scale);

  /** @brief Changes @p body's surface response, waking what touches it. */
  void SetMaterial(BodyHandle body, const Material& material);

  /**
   * @brief Switches @p body between static and dynamic in place. Mesh bodies
   * stay static.
   */
  void SetMotion(BodyHandle body, Motion motion);

  /** @brief Sets @p body's linear velocity in m/s. */
  void SetVelocity(BodyHandle body, const bx::Vec3& velocity);

  /** @brief Turns world gravity on or off for @p body, waking it. */
  void SetGravityEnabled(BodyHandle body, bool enabled);

  /**
   * @brief Sets an extra acceleration in m/s^2, applied to @p body on every
   * Step while it is dynamic, on top of gravity.
   */
  void SetAcceleration(BodyHandle body, const bx::Vec3& acceleration);

  /** @brief Returns @p body's current pose. */
  [[nodiscard]] auto GetPose(BodyHandle body) const -> Pose;

  /** @brief Returns @p body's linear velocity in m/s. */
  [[nodiscard]] auto GetVelocity(BodyHandle body) const -> bx::Vec3;

  /**
   * @brief Returns @p body's tag, or nothing if it no longer exists. Stale
   * handles are safe to pass.
   */
  [[nodiscard]] auto UserData(BodyHandle body) const -> std::optional<UserTag>;

  /**
   * @brief Lists every body but ragdoll parts; a copy, so destroying while
   * looping is safe.
   */
  [[nodiscard]] auto Bodies() const -> std::vector<BodyEntry>;

  /**
   * @brief Adds a character. It collides with every body and pushes dynamic
   * ones, but bodies don't collide with it.
   * @return The character, never invalid.
   */
  auto CreateCharacter(const CharacterDesc& desc) -> CharacterHandle;

  /**
   * @brief Returns @p character's tag, or nothing if it no longer exists.
   * Stale handles are safe to pass.
   */
  [[nodiscard]] auto UserData(CharacterHandle character) const
      -> std::optional<UserTag>;

  /** @brief Removes @p character. Stale handles are safe to pass. */
  void DestroyCharacter(CharacterHandle character);

  /** @brief Teleports @p character's feet to @p feet. */
  void SetCharacterFeet(CharacterHandle character, const bx::Vec3& feet);

  /**
   * @brief Sets the velocity @p character moves with, in m/s. Each Step adds
   * gravity to it, and drops the downward part while on the ground.
   */
  void SetCharacterVelocity(CharacterHandle character,
                            const bx::Vec3& velocity);

  /**
   * @brief Turns and scales @p character's hitbox about its feet; the capsule
   * stays upright and unscaled. Identity rotation and unit scale by default.
   * @param scale Per axis; every axis non-zero.
   */
  void SetHitboxPose(CharacterHandle character, const bx::Quaternion& rotation,
                     const bx::Vec3& scale);

  /** @brief Returns @p character's state after the last Step. */
  [[nodiscard]] auto GetCharacter(CharacterHandle character) const
      -> CharacterState;

  /** @brief Lists every character; a copy, safe to destroy while looping. */
  [[nodiscard]] auto Characters() const -> std::vector<CharacterEntry>;

  /**
   * @brief Builds the parts and joints of a rig's ragdolls for every one of
   * them to share. They live until the world is destroyed.
   * @param rest_joints Every skeleton joint's rest pose, in model space:
   *        the directions parts swing and twist away from.
   * @return Invalid if Jolt rejects the parts.
   * @pre Parents precede children; every part's joint indexes rest_joints.
   */
  auto CreateRagdollSettings(const RagdollDesc& desc,
                             const std::vector<Pose>& rest_joints)
      -> RagdollSettingsHandle;

  /**
   * @brief Adds a kinematic ragdoll posed at @p joints: rays hit its parts
   * and it pushes dynamic bodies while it follows PoseRagdoll, until
   * SetRagdollSimulated hands it to the simulation.
   *
   * Characters never collide with ragdolls, and a ragdoll's parts never
   * collide with each other.
   * @param joints Every skeleton joint's pose, in world space.
   * @return Invalid if Jolt is out of bodies.
   */
  auto CreateRagdoll(RagdollSettingsHandle settings,
                     const std::vector<Pose>& joints, UserTag user_data)
      -> RagdollHandle;

  /** @brief Removes @p ragdoll. Stale handles are safe to pass. */
  void DestroyRagdoll(RagdollHandle ragdoll);

  /**
   * @brief Teleports a kinematic ragdoll's parts to follow @p joints, and
   * records how fast they moved over @p dt seconds for SetRagdollSimulated.
   * Ignored while the ragdoll is simulated.
   * @param joints Every skeleton joint's pose, in world space.
   */
  void PoseRagdoll(RagdollHandle ragdoll, const std::vector<Pose>& joints,
                   float dt);

  /**
   * @brief Hands @p ragdoll to the simulation, each part keeping the
   * velocity it last moved at, or takes it back to follow PoseRagdoll.
   */
  void SetRagdollSimulated(RagdollHandle ragdoll, bool simulated);

  /**
   * @brief The world pose of every joint a part follows, from where the
   * parts are now, in part order.
   */
  [[nodiscard]] auto RagdollJointPoses(RagdollHandle ragdoll) const
      -> std::vector<RagdollJoint>;

  /** @brief Each of @p ragdoll's part capsules in world space, in part order.
   */
  [[nodiscard]] auto RagdollCapsules(RagdollHandle ragdoll) const
      -> std::vector<bx::Capsule>;

  /**
   * @brief Returns @p ragdoll's tag, or nothing if it no longer exists.
   * Stale handles are safe to pass.
   */
  [[nodiscard]] auto UserData(RagdollHandle ragdoll) const
      -> std::optional<UserTag>;

  /** @brief Lists every ragdoll; a copy, safe to destroy while looping. */
  [[nodiscard]] auto Ragdolls() const -> std::vector<RagdollEntry>;

  /**
   * @brief Casts @p ray against every body, ragdoll and character, as of the
   * last Step (and PoseRagdoll). A character with a hitbox is hit only on
   * its hitbox, and one that isn't a ray target never.
   *
   * A ray starting inside a convex shape hits it at distance 0; meshes are
   * one-sided, so their back faces are never hit.
   * @param ray Origin and unit-length direction, in world space.
   * @param max_distance How far the ray reaches, in m.
   * @return Every hit, nearest first; at most one per body or character.
   */
  [[nodiscard]] auto CastRay(const bx::Ray& ray, float max_distance) const
      -> std::vector<RayHit>;

  /**
   * @brief Advances the simulation by exactly @p dt seconds: moves every
   * character, then steps the bodies.
   */
  void Step(float dt);

 private:
  /// Jolt's world and its helpers, defined in the .cpp.
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace engine::physics
