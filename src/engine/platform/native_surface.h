#pragma once

// Standard headers only: hosts fill this without seeing bgfx

#include <cstdint>

#include "engine/platform/screen.h"

/** @brief Windowing protocol a NativeSurface's handles belong to. */
enum class SurfaceKind : std::uint8_t { kX11, kWayland };

/** @brief A host window for the engine to render into. */
struct NativeSurface {
  // X11 Window id, or wl_surface*.
  void* window{nullptr};

  // X11 Display*, or wl_display*.
  void* display{nullptr};

  SurfaceKind kind{SurfaceKind::kX11};

  // Backbuffer size when the engine starts.
  PixelSize size{};
};
