#pragma once

#include <bx/math.h>

#include <array>
#include <cstddef>

/** @brief Position, rotation and scale of an entity, in world space. */
struct Transform {
  bx::Vec3 position{0.0f};
  /// Unit quaternion; see math/rotation.h for the convention.
  bx::Quaternion rotation{bx::InitIdentity};
  bx::Vec3 scale{1.0f};
};

/** @brief Builds the model matrix: scale, then rotate, then translate. */
inline auto ModelMatrix(const Transform& transform) -> std::array<float, 16> {
  std::array<float, 16> mtx{};
  // bx applies matrices to row vectors, which inverts q: build from q*.
  // Not the overload taking a translation: it stores the inverse's
  bx::mtxFromQuaternion(mtx.data(), bx::conjugate(transform.rotation));
  // Scale each basis row, as mtxSRT does
  const std::array<float, 3> scale{transform.scale.x, transform.scale.y,
                                   transform.scale.z};
  for (std::size_t row = 0; row < scale.size(); ++row) {
    for (std::size_t col = 0; col < 3; ++col) {
      mtx.at((row * 4) + col) *= scale.at(row);
    }
  }
  bx::store(&mtx.at(12), transform.position);
  return mtx;
}
