#include "editor/viewport_window.h"

#include <QtCore/qtmetamacros.h>
#include <QtGui/qguiapplication_platform.h>
#include <qpa/qplatformwindow_p.h>

#include <QCoreApplication>
#include <QDebug>
#include <QEvent>
#include <QExposeEvent>
#include <QGuiApplication>
#include <QPlatformSurfaceEvent>
#include <QSurface>
#include <QTimer>
#include <QWindow>
#include <QtLogging>
#include <bit>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <format>
#include <stdexcept>
#include <string>

#include "engine/engine.h"
#include "engine/platform/frame_context.h"
#include "engine/platform/native_surface.h"
#include "engine/platform/screen.h"
#include "game/game_logic.h"

using engine::Engine;
using engine::FrameContext;
using engine::NativeSurface;
using engine::PixelSize;
using engine::SurfaceKind;

namespace editor {

ViewportWindow::ViewportWindow(QWindow* parent) : QWindow(parent) {
  // bgfx renders with Vulkan, so Qt must not back this with buffers of its own
  setSurfaceType(QSurface::VulkanSurface);
  connect(&frame_timer_, &QTimer::timeout, this, &ViewportWindow::Tick);
}

void ViewportWindow::LoadScene(const std::filesystem::path& path) {
  assert(engine_ && "LoadScene before Ready");
  Engine& engine = engine_.value();
  engine.ClearWorld();
  session_.emplace(engine);
  engine.SetActiveCamera(session_->ViewCamera());
  engine.LoadScene(path);
  // Loading time is not game time
  last_time_ = Clock::now();
}

void ViewportWindow::exposeEvent(QExposeEvent* /*event*/) {
  if (!isExposed()) {
    frame_timer_.stop();
    return;
  }
  if (!engine_) {
    try {
      engine_.emplace(Surface());
      game::RegisterGameLogic(*engine_);
    } catch (const std::exception& e) {
      // Exceptions must not cross Qt's event loop
      qCritical() << "fatal:" << e.what();
      engine_.reset();
      QCoreApplication::exit(1);
      return;
    }
    start_time_ = Clock::now();
    last_time_ = start_time_;
    emit Ready();
  }
  frame_timer_.start();
}

auto ViewportWindow::event(QEvent* e) -> bool {
  if (e->type() == QEvent::PlatformSurface) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
    const auto* surface_event = static_cast<QPlatformSurfaceEvent*>(e);
    if (surface_event->surfaceEventType() ==
        QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed) {
      // bgfx must let go of the surface while it still exists
      frame_timer_.stop();
      session_.reset();
      engine_.reset();
    }
  }
  return QWindow::event(e);
}

void ViewportWindow::Tick() {
  // Nothing to show until a scene has started a session
  if (!engine_ || !session_) {
    return;
  }
  const PixelSize size = BackbufferSize();
  if (size.width == 0 || size.height == 0) {
    return;
  }

  using Seconds = std::chrono::duration<float>;
  const auto now = Clock::now();
  const float dt = Seconds(now - last_time_).count();
  last_time_ = now;

  engine_->Resize(size);
  const FrameContext ctx{
      .width = size.width,
      .height = size.height,
      .dt = dt,
      .time = Seconds(now - start_time_).count(),
  };

  // The launcher's play-mode frame, without devtools
  session_->PrePhysics(*engine_, ctx, /*has_control=*/true);
  engine_->Update(ctx);
  session_->PostPhysics(*engine_);
  engine_->Render(ctx, {.debug_draw = false});
  engine_->EndFrame();
}

auto ViewportWindow::Surface() const -> NativeSurface {
  NativeSurface surface{.size = BackbufferSize()};

  const std::string platform = QGuiApplication::platformName().toStdString();
  if (platform == "wayland") {
    const auto* app =
        qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
    // The wl_surface is only reachable through Qt's private API
    const auto* window =
        nativeInterface<QNativeInterface::Private::QWaylandWindow>();
    surface.kind = SurfaceKind::kWayland;
    surface.display = app != nullptr ? app->display() : nullptr;
    surface.window = window != nullptr ? window->surface() : nullptr;
  } else if (platform == "xcb") {
    const auto* app =
        qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
    surface.kind = SurfaceKind::kX11;
    surface.display = app != nullptr ? app->display() : nullptr;
    // An X11 window is an integer id; bgfx takes it in a pointer
    surface.window = std::bit_cast<void*>(static_cast<std::uintptr_t>(winId()));
  }

  if (surface.window == nullptr || surface.display == nullptr) {
    throw std::runtime_error(
        std::format("no native window handles for Qt platform '{}'", platform));
  }
  return surface;
}

auto ViewportWindow::BackbufferSize() const -> PixelSize {
  const double scale = devicePixelRatio();
  return {.width = static_cast<std::uint32_t>(std::lround(width() * scale)),
          .height = static_cast<std::uint32_t>(std::lround(height() * scale))};
}

}  // namespace editor
