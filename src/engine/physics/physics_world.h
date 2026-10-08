#pragma once

#include <bx/math.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "engine/physics/body_handle.h"
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

/** @brief Everything needed to create a body. */
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
  std::uint64_t user_data{0};
};

/** @brief A body in the world and the tag it was created with. */
struct BodyEntry {
  BodyHandle body;
  std::uint64_t user_data{0};
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
   * @brief Creates a body and adds it to the world.
   * @return The body, or an invalid handle if Jolt is out of bodies.
   */
  auto CreateBody(const BodyDesc& desc) -> BodyHandle;

  /** @brief Removes and destroys @p body, waking what touched it. */
  void DestroyBody(BodyHandle body);

  /** @brief Teleports @p body, waking what touched its old and new place. */
  void SetPose(BodyHandle body, const Pose& pose);

  /**
   * @brief Reshapes @p body in place, keeping its id and velocity.
   * @param scale Applied to @p shape: sizes and offset scale per axis.
   */
  void SetShape(BodyHandle body, const ShapeDesc& shape, const bx::Vec3& scale);

  /** @brief Changes @p body's surface response, waking what touches it. */
  void SetMaterial(BodyHandle body, const Material& material);

  /** @brief Switches @p body between static and dynamic in place. */
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
  [[nodiscard]] auto UserData(BodyHandle body) const
      -> std::optional<std::uint64_t>;

  /** @brief Lists every body; a copy, so destroying while looping is safe. */
  [[nodiscard]] auto Bodies() const -> std::vector<BodyEntry>;

  /** @brief Advances the simulation by exactly @p dt seconds. */
  void Step(float dt);

 private:
  /// Jolt's world and its helpers, defined in the .cpp.
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace engine::physics
