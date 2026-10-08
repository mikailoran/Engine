#pragma once

#include <cstdint>

namespace engine::physics {

/**
 * @brief Opaque id of a collision mesh in a PhysicsWorld. Meshes live until
 * the world is destroyed, so a valid id never goes stale.
 */
class CollisionMeshHandle {
 public:
  static constexpr std::uint32_t kInvalidHandle = 0xFFFFFFFFU;

  /** @brief Names no mesh. */
  CollisionMeshHandle() = default;

  /** @brief Wraps a raw id; only PhysicsWorld makes valid ones. */
  explicit CollisionMeshHandle(std::uint32_t value) : value_(value) {}

  /** @brief The raw id, for display and comparison. */
  [[nodiscard]] auto Value() const -> std::uint32_t { return value_; }

  /** @brief Whether this names a mesh. */
  [[nodiscard]] auto IsValid() const -> bool {
    return value_ != kInvalidHandle;
  }

  /** @brief Compares the ids. */
  auto operator==(const CollisionMeshHandle&) const -> bool = default;

 private:
  std::uint32_t value_{kInvalidHandle};
};

}  // namespace engine::physics
