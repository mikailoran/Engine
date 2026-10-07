#pragma once

// Standard headers only: hosts fill this without seeing bgfx

#include <cstdint>

/** @brief Windowing protocol a NativeSurface's handles belong to. */
enum class SurfaceKind : std::uint8_t { kX11, kWayland };

/** @brief A host window for the engine to render into. */
struct NativeSurface {
  // X11 Window id, or wl_surface*.
  void* window{nullptr};

  // X11 Display*, or wl_display*.
  void* display{nullptr};

  SurfaceKind kind{SurfaceKind::kX11};

  // Backbuffer size in pixels when the engine starts.
  std::uint32_t width{0};
  std::uint32_t height{0};
};
