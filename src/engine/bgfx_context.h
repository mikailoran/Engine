#pragma once

#include <bgfx/defines.h>

#include <cstdint>

#include "engine/platform/native_surface.h"

/**
 * @brief Owns bgfx's lifetime: bgfx::init on construction, bgfx::shutdown on
 * destruction.
 *
 * Every GPU resource must be released before this is destroyed.
 */
class BgfxContext {
 public:
  /** @brief Reset flags the backbuffer is created and resized with. */
  static constexpr std::uint32_t kResetFlags = BGFX_RESET_VSYNC;

  /** @brief Debug flags bgfx starts with. */
  static constexpr std::uint32_t kDebugFlags = BGFX_DEBUG_TEXT;

  /**
   * @brief Brings up bgfx against @p surface.
   * @throws std::runtime_error If bgfx::init fails.
   */
  explicit BgfxContext(const NativeSurface& surface);

  /** @brief Shuts bgfx down. */
  ~BgfxContext();

  // bgfx is a process-wide singleton: exactly one owner
  BgfxContext(const BgfxContext&) = delete;
  auto operator=(const BgfxContext&) -> BgfxContext& = delete;
  BgfxContext(BgfxContext&&) = delete;
  auto operator=(BgfxContext&&) -> BgfxContext& = delete;
};
