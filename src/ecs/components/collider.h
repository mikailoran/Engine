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

  /// Bounciness from 0 to 1. A contact uses the higher of its two bodies'.
  float restitution{0.6F};

  /// Sliding resistance, 0 or more. A contact uses sqrt(a * b) of its pair's.
  float friction{0.2F};

  /// Opaque Jolt body id, written by Physics only.
  std::uint32_t body_id{kNoBody};
};
