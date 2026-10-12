#include "editor/viewport_window.h"

#include <QtCore/qnamespace.h>
#include <QtCore/qtmetamacros.h>
#include <QtGui/qguiapplication_platform.h>

#include <QEvent>
#include <QExposeEvent>
#include <QFocusEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QObject>
#include <QPlatformSurfaceEvent>
#include <QPointF>
#include <QSurface>
#include <QWindow>
#include <bit>
#include <cmath>
#include <cstdint>
#include <format>
#include <optional>
#include <stdexcept>

#include "engine/platform/input.h"
#include "engine/platform/native_surface.h"
#include "engine/platform/screen.h"

using engine::Input;
using engine::Key;
using engine::MouseButton;
using engine::NativeSurface;
using engine::PixelSize;
using engine::ScreenPosition;
using engine::SurfaceKind;

namespace editor {

namespace {

/**
 * @brief The engine key for a QKeyEvent::key(), if the game reads it. Qt has
 * one key per modifier, so Shift drives the left one.
 */
auto KeyFromQt(int qt_key) -> std::optional<Key> {
  switch (qt_key) {
    case Qt::Key_W:
      return Key::kW;
    case Qt::Key_A:
      return Key::kA;
    case Qt::Key_S:
      return Key::kS;
    case Qt::Key_D:
      return Key::kD;
    case Qt::Key_Space:
      return Key::kSpace;
    case Qt::Key_Shift:
      return Key::kLeftShift;
    default:
      return std::nullopt;
  }
}

/** @brief The engine's button for a Qt button, if the engine reads it. */
auto ButtonFromQt(Qt::MouseButton button) -> std::optional<MouseButton> {
  switch (button) {
    case Qt::LeftButton:
      return MouseButton::kLeft;
    case Qt::RightButton:
      return MouseButton::kRight;
    case Qt::MiddleButton:
      return MouseButton::kMiddle;
    default:
      return std::nullopt;
  }
}

}  // namespace

ViewportWindow::ViewportWindow(QWindow* parent) : QWindow(parent) {
  // bgfx renders with Vulkan, so Qt must not back this with buffers of its own
  setSurfaceType(QSurface::VulkanSurface);
}

auto ViewportWindow::TakeFrameInput() -> Input {
  Input frame = input_;
  input_.BeginFrame();
  return frame;
}

void ViewportWindow::exposeEvent(QExposeEvent* /*event*/) {
  emit Exposed(isExposed());
}

auto ViewportWindow::event(QEvent* e) -> bool {
  if (e->type() == QEvent::PlatformSurface) {
    const auto* surface_event = static_cast<QPlatformSurfaceEvent*>(e);
    if (surface_event->surfaceEventType() ==
        QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed) {
      emit SurfaceLost();
    }
  }
  return QWindow::event(e);
}

void ViewportWindow::keyPressEvent(QKeyEvent* event) { RecordKey(event, true); }

void ViewportWindow::keyReleaseEvent(QKeyEvent* event) {
  RecordKey(event, false);
}

void ViewportWindow::focusOutEvent(QFocusEvent* event) {
  ReleaseInput();
  QWindow::focusOutEvent(event);
}

void ViewportWindow::mousePressEvent(QMouseEvent* event) {
  RecordButton(event, true);
}

void ViewportWindow::mouseReleaseEvent(QMouseEvent* event) {
  RecordButton(event, false);
}

void ViewportWindow::mouseMoveEvent(QMouseEvent* event) {
  const ScreenPosition position = ToBackbuffer(event->position());
  input_.SetMouse(position);
  if (look_from_) {
    input_.AddMouseMotion(
        {.x = position.x - look_from_->x, .y = position.y - look_from_->y});
    look_from_ = position;
  }
}

void ViewportWindow::RecordButton(QMouseEvent* event, bool down) {
  const std::optional<MouseButton> button = ButtonFromQt(event->button());
  if (!button) {
    event->ignore();
    return;
  }
  input_.SetButton(*button, down);
  if (*button != MouseButton::kRight) {
    return;
  }
  if (down) {
    look_from_ = ToBackbuffer(event->position());
    setCursor(Qt::BlankCursor);
  } else {
    look_from_.reset();
    unsetCursor();
  }
}

void ViewportWindow::ReleaseInput() {
  // Releases that happen while unfocused never arrive; drop everything held
  input_ = Input{};
  look_from_.reset();
  unsetCursor();
}

auto ViewportWindow::ToBackbuffer(QPointF position) const -> ScreenPosition {
  const double scale = devicePixelRatio();
  return {.x = static_cast<float>(position.x() * scale),
          .y = static_cast<float>(position.y() * scale)};
}

void ViewportWindow::RecordKey(QKeyEvent* event, bool down) {
  const std::optional<Key> key = KeyFromQt(event->key());
  if (!key) {
    // Not the engine's: let Qt pass it on, e.g. to shortcuts
    event->ignore();
    return;
  }
  // A held key repeats; only the first press and the release matter
  if (!event->isAutoRepeat()) {
    input_.SetKey(*key, down);
  }
}

auto ViewportWindow::Surface() const -> NativeSurface {
  const auto* app =
      qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
  if (app == nullptr || app->display() == nullptr) {
    throw std::runtime_error(
        std::format("no X11 display for Qt platform '{}'",
                    QGuiApplication::platformName().toStdString()));
  }
  return {
      // An X11 window is an integer id; bgfx takes it in a pointer
      .window = std::bit_cast<void*>(static_cast<std::uintptr_t>(winId())),
      .display = app->display(),
      .kind = SurfaceKind::kX11,
      .size = BackbufferSize(),
  };
}

auto ViewportWindow::BackbufferSize() const -> PixelSize {
  const double scale = devicePixelRatio();
  return {.width = static_cast<std::uint32_t>(std::lround(width() * scale)),
          .height = static_cast<std::uint32_t>(std::lround(height() * scale))};
}

}  // namespace editor
