#pragma once

#include <bx/math.h>

#include <cstdint>
#include <memory>

class JoltRuntime;

/** @brief Opaque id of a body in a PhysicsWorld. Stale ids never alias. */
class BodyHandle {
 public:
  static constexpr std::uint32_t kInvalidHandle = 0xFFFFFFFFU;

  /** @brief Names no body. */
  BodyHandle() = default;

  /** @brief Wraps a raw id; only PhysicsWorld makes valid ones. */
  explicit BodyHandle(std::uint32_t value) : value_(value) {}

  /** @brief The raw id, for display and comparison. */
  [[nodiscard]] auto Value() const -> std::uint32_t { return value_; }

  /** @brief Whether this names a body (it may since have been destroyed). */
  [[nodiscard]] auto IsValid() const -> bool { return value_ != kInvalidHandle; }

  /** @brief Compares the ids. */
  auto operator==(const BodyHandle&) const -> bool = default;

 private:
  std::uint32_t value_{kInvalidHandle};
};

/** @brief A body's position and rotation in world space. */
struct Pose {
  bx::Vec3 position{0.0F};
  bx::Quaternion rotation{bx::InitIdentity};
};

/** @brief A box in body space, with any scale already applied. */
struct ShapeDesc {
  /// Half the box's size on each axis, in m.
  bx::Vec3 half_extents{0.5F};
  /// Box centre relative to the body's origin, in m.
  bx::Vec3 offset{0.0F};
};

/** @brief How a body's surface responds to contact. */
struct Material {
  /// Bounciness from 0 to 1. A contact uses the higher of its two bodies'.
  float restitution{0.0F};
  /// Sliding resistance, 0 or more. A contact uses sqrt(a * b) of its pair's.
  float friction{0.2F};
};

/** @brief Whether a body moves under forces and collisions. */
enum class Motion : std::uint8_t { kStatic, kDynamic };

/** @brief Everything needed to create a body. */
struct BodyDesc {
  ShapeDesc shape;
  Pose pose;
  Motion motion{Motion::kStatic};
  Material material;
  /// Initial linear velocity in m/s; dynamic bodies only.
  bx::Vec3 velocity{0.0F};
  /// Whether world gravity applies; dynamic bodies only.
  bool gravity{true};
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

  /** @brief Reshapes @p body in place, keeping its id and velocity. */
  void SetShape(BodyHandle body, const ShapeDesc& shape);

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

  /** @brief Advances the simulation by exactly @p dt seconds. */
  void Step(float dt);

 private:
  /// Jolt's world and its helpers, defined in the .cpp.
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
