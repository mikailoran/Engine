#include "engine/bgfx_context.h"

#include <bgfx/bgfx.h>

#include <stdexcept>

#include "engine/platform/native_surface.h"

BgfxContext::BgfxContext(const NativeSurface& surface) {
  bgfx::Init init;
  init.type = bgfx::RendererType::Count;  // auto-select backend
  init.platformData.nwh = surface.window;
  init.platformData.ndt = surface.display;
  init.platformData.type = surface.kind == SurfaceKind::kWayland
                               ? bgfx::NativeWindowHandleType::Wayland
                               : bgfx::NativeWindowHandleType::Default;
  init.resolution.width = surface.width;
  init.resolution.height = surface.height;
  init.resolution.reset = kResetFlags;
  if (!bgfx::init(init)) {
    throw std::runtime_error("bgfx::init failed");
  }

  bgfx::setDebug(kDebugFlags);
}

BgfxContext::~BgfxContext() { bgfx::shutdown(); }
