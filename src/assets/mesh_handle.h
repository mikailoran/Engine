#pragma once

#include <cstdint>
#include <limits>

struct MeshHandle {
  std::uint16_t idx{std::numeric_limits<std::uint16_t>::max()};
};

constexpr MeshHandle kInvalidMesh{};

constexpr bool isValid(MeshHandle handle) {
  return handle.idx != std::numeric_limits<std::uint16_t>::max();
}
