#pragma once

#include <bx/math.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "engine/physics/body_handle.h"
#include "engine/physics/character_handle.h"
#include "engine/physics/collision_mesh_handle.h"
#include "engine/physics/shape.h"

namespace engine::physics {

class JoltRuntime;

/** @brief A body's position and rotation in world space. */
struct Pose {
  bx::Vec3 position{0.0F};
  bx::Quaternion rotation{bx::InitIdentity};
};

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
  /// Caller's tag for the character, returned by Characters.
  UserTag user_data{0};
};

/** @brief A character's state after the last Step. */
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

  /** @brief Lists every body; a copy, so destroying while looping is safe. */
  [[nodiscard]] auto Bodies() const -> std::vector<BodyEntry>;

  /**
   * @brief Adds a character. It collides with every body and pushes dynamic
   * ones, but bodies don't collide with it.
   * @return The character, never invalid.
   */
  auto CreateCharacter(const CharacterDesc& desc) -> CharacterHandle;

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

  /** @brief Returns @p character's state after the last Step. */
  [[nodiscard]] auto GetCharacter(CharacterHandle character) const
      -> CharacterState;

  /** @brief Lists every character; a copy, safe to destroy while looping. */
  [[nodiscard]] auto Characters() const -> std::vector<CharacterEntry>;

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
