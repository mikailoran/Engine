#pragma once

#include <cstdint>

/**
 * @brief Gives an entity a physics body shaped as a box around its mesh.
 *
 * Needs a Transform and a Renderable. The body is static on its own and
 * dynamic alongside a RigidBody.
 */
struct Collider {
  /// No body yet; matches Jolt's invalid BodyID.
  static constexpr std::uint32_t kNoBody = 0xFFFFFFFFU;

  /// Opaque Jolt body id, written by Physics only.
  std::uint32_t body_id{kNoBody};
};
