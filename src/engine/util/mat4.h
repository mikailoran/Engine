#pragma once

#include <array>

namespace engine {

/// A column-major 4x4 matrix: glTF's layout, and bx's and ozz's in memory
/// (translation in elements 12 to 14).
using Mat4 = std::array<float, 16>;

}  // namespace engine
