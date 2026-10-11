#include "editor/viewport_window.h"

#include <QtGui/qguiapplication_platform.h>
#include <qpa/qplatformwindow_p.h>

#include <QCoreApplication>
#include <QDebug>
#include <QExposeEvent>
#include <QGuiApplication>
#include <QSurface>
#include <QWindow>
#include <QtLogging>
#include <bit>
#include <cmath>
#include <cstdint>
#include <exception>
#include <format>
#include <stdexcept>
#include <string>

#include "engine/platform/native_surface.h"
#include "engine/platform/screen.h"

using engine::NativeSurface;
using engine::PixelSize;
using engine::SurfaceKind;

namespace editor {

ViewportWindow::ViewportWindow(QWindow* parent) : QWindow(parent) {
  // bgfx renders with Vulkan, so Qt must not back this with buffers of its own
  setSurfaceType(QSurface::VulkanSurface);
}

void ViewportWindow::exposeEvent(QExposeEvent* /*event*/) {
  if (!isExposed()) {
    return;
  }
  // Spike: the engine will be created from these on the first expose
  try {
    const NativeSurface surface = Surface();
    qInfo() << "viewport surface:" << surface.window
            << "display:" << surface.display << "size:" << surface.size.width
            << "x" << surface.size.height << "on"
            << QGuiApplication::platformName();
  } catch (const std::exception& e) {
    // Exceptions must not cross Qt's event loop
    qCritical() << "fatal:" << e.what();
    QCoreApplication::exit(1);
  }
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
