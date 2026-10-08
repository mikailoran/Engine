#pragma once

#include <cstdint>
#include <limits>

namespace engine {

/**
 * @brief Non-owning reference to a mesh held by AssetRegistry.
 *
 * An index rather than a pointer: it survives the registry reallocating its
 * storage, and it cannot be mistaken for something the holder owns.
 */
struct MeshHandle {
  /// Index into AssetRegistry's mesh storage.
  std::uint16_t idx{std::numeric_limits<std::uint16_t>::max()};
};

/// Refers to no mesh. The default for any MeshHandle member.
constexpr MeshHandle kInvalidMesh{};

/**
 * @brief Tests whether a handle refers to a mesh.
 *
 * @param handle Handle to test.
 * @return True unless the handle is kInvalidMesh.
 */
constexpr auto IsValid(MeshHandle handle) -> bool {
  return handle.idx != std::numeric_limits<std::uint16_t>::max();
}

}  // namespace engine
