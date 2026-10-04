#pragma once

#include <bx/math.h>

#include <array>

struct Transform {
  bx::Vec3 position{0.0f};
  bx::Vec3 rotation{0.0f};
  bx::Vec3 scale{1.0f};
};

/** @brief Builds the model matrix: scale, then rotate, then translate. */
inline auto ModelMatrix(const Transform& transform) -> std::array<float, 16> {
  std::array<float, 16> mtx{};
  bx::mtxSRT(mtx.data(), transform.scale.x, transform.scale.y,
             transform.scale.z, transform.rotation.x, transform.rotation.y,
             transform.rotation.z, transform.position.x, transform.position.y,
             transform.position.z);
  return mtx;
}
