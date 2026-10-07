#include "engine/bgfx_context.h"

#include <bgfx/bgfx.h>

#include <stdexcept>

#include "engine/platform/native_surface.h"
#include "engine/platform/screen.h"

namespace engine {

BgfxContext::BgfxContext(const NativeSurface& surface) : size_(surface.size) {
  // Called before init, this keeps bgfx from starting its own render thread
  bgfx::renderFrame();

  bgfx::Init init;
  init.type = bgfx::RendererType::Count;  // auto-select backend
  init.platformData.nwh = surface.window;
  init.platformData.ndt = surface.display;
  init.platformData.type = surface.kind == SurfaceKind::kWayland
                               ? bgfx::NativeWindowHandleType::Wayland
                               : bgfx::NativeWindowHandleType::Default;
  init.resolution.width = surface.size.width;
  init.resolution.height = surface.size.height;
  init.resolution.reset = kResetFlags;
  if (!bgfx::init(init)) {
    throw std::runtime_error("bgfx::init failed");
  }

  bgfx::setDebug(kDebugFlags);
}

BgfxContext::~BgfxContext() { bgfx::shutdown(); }

void BgfxContext::Resize(PixelSize size) {
  if (size == size_) {
    return;
  }
  size_ = size;
  bgfx::reset(size.width, size.height, kResetFlags);
}

}  // namespace engine
