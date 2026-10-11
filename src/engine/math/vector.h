#pragma once

#include <bx/math.h>

#include <array>

namespace engine {

/**
 * @brief Extends @p v to four floats, e.g. for a vec4 shader uniform.
 * @param w 0 for directions and colors, 1 for positions.
 */
inline auto ToVec4(const bx::Vec3& v, float w = 0.0F) -> std::array<float, 4> {
  return {v.x, v.y, v.z, w};
}

}  // namespace engine
