#pragma once

#include <cstdint>
#include <limits>

// TODO: Same as mesh handle so refactor into one class maybe

/**
 * @brief Non-owning reference to a texture held by AssetRegistry.
 *
 * Mirrors MeshHandle: an index into the registry's storage, not the bgfx
 * handle itself, so holders cannot destroy what they don't own.
 */
struct TextureHandle {
  /// Index into AssetRegistry's texture storage.
  std::uint16_t idx{std::numeric_limits<std::uint16_t>::max()};
};

/// Refers to no texture. The default for any TextureHandle member.
constexpr TextureHandle kInvalidTexture{};

/**
 * @brief Tests whether a handle refers to a texture.
 *
 * @param handle Handle to test.
 * @return True unless the handle is kInvalidTexture.
 */
constexpr bool isValid(TextureHandle handle) {
  return handle.idx != std::numeric_limits<std::uint16_t>::max();
}
