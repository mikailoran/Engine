#include "editor/viewport_window.h"

#include <QtCore/qnamespace.h>
#include <QtCore/qtmetamacros.h>
#include <QtGui/qguiapplication_platform.h>
#include <qpa/qplatformwindow_p.h>

#include <QCoreApplication>
#include <QDebug>
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
#include <QTimer>
#include <QWheelEvent>
#include <QWindow>
#include <QtLogging>
#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <format>
#include <optional>
#include <stdexcept>
#include <string>

#include "engine/engine.h"
#include "engine/platform/frame_context.h"
#include "engine/platform/input.h"
#include "engine/platform/native_surface.h"
#include "engine/platform/screen.h"
#include "game/game_logic.h"

using engine::Engine;
using engine::FrameContext;
using engine::Input;
using engine::Key;
using engine::MouseButton;
using engine::NativeSurface;
using engine::PixelSize;
using engine::ScreenPosition;
using engine::SurfaceKind;

namespace editor {

namespace {

/** @brief An engine key and the Qt key that drives it. */
struct KeyBinding {
  Key key;
  Qt::Key qt_key;
};

// Keys as the layout names them. Qt has one key per modifier, not a left and a
// right one, so it drives the left one and the right ones stay unbound
constexpr auto kKeyBindings = std::to_array<KeyBinding>({
    // clang-format off
    {.key = Key::kW,         .qt_key = Qt::Key_W},
    {.key = Key::kA,         .qt_key = Qt::Key_A},
    {.key = Key::kS,         .qt_key = Qt::Key_S},
    {.key = Key::kD,         .qt_key = Qt::Key_D},
    {.key = Key::kQ,         .qt_key = Qt::Key_Q},
    {.key = Key::kE,         .qt_key = Qt::Key_E},
    {.key = Key::kC,         .qt_key = Qt::Key_C},
    {.key = Key::kV,         .qt_key = Qt::Key_V},
    {.key = Key::kX,         .qt_key = Qt::Key_X},
    {.key = Key::kY,         .qt_key = Qt::Key_Y},
    {.key = Key::kZ,         .qt_key = Qt::Key_Z},
    {.key = Key::kTab,       .qt_key = Qt::Key_Tab},
    {.key = Key::kLeft,      .qt_key = Qt::Key_Left},
    {.key = Key::kRight,     .qt_key = Qt::Key_Right},
    {.key = Key::kUp,        .qt_key = Qt::Key_Up},
    {.key = Key::kDown,      .qt_key = Qt::Key_Down},
    {.key = Key::kPageUp,    .qt_key = Qt::Key_PageUp},
    {.key = Key::kPageDown,  .qt_key = Qt::Key_PageDown},
    {.key = Key::kHome,      .qt_key = Qt::Key_Home},
    {.key = Key::kEnd,       .qt_key = Qt::Key_End},
    {.key = Key::kDelete,    .qt_key = Qt::Key_Delete},
    {.key = Key::kBackspace, .qt_key = Qt::Key_Backspace},
    {.key = Key::kEnter,     .qt_key = Qt::Key_Return},
    {.key = Key::kEscape,    .qt_key = Qt::Key_Escape},
    {.key = Key::kLeftCtrl,  .qt_key = Qt::Key_Control},
    {.key = Key::kLeftShift, .qt_key = Qt::Key_Shift},
    {.key = Key::kLeftAlt,   .qt_key = Qt::Key_Alt},
    {.key = Key::kSpace,     .qt_key = Qt::Key_Space},
    {.key = Key::kF1,        .qt_key = Qt::Key_F1},
    // clang-format on
});

/** @brief Whether Qt can report @p key: every key but the right modifiers. */
constexpr auto QtReports(Key key) -> bool {
  return key != Key::kRightCtrl && key != Key::kRightShift &&
         key != Key::kRightAlt;
}

/** @brief Whether every Key Qt reports appears in exactly one binding. */
constexpr auto BindsEveryKeyOnce() -> bool {
  std::array<int, static_cast<std::size_t>(Key::kCount)> uses{};
  for (const KeyBinding& binding : kKeyBindings) {
    ++uses.at(static_cast<std::size_t>(binding.key));
  }
  for (std::size_t index = 0; index < uses.size(); ++index) {
    const int expected = QtReports(static_cast<Key>(index)) ? 1 : 0;
    if (uses.at(index) != expected) {
      return false;
    }
  }
  return true;
}

static_assert(BindsEveryKeyOnce(), "every Key Qt reports needs one binding");

/** @brief The engine key for a QKeyEvent::key(), if the engine has one. */
auto KeyFromQt(int qt_key) -> std::optional<Key> {
  const auto* const binding = std::ranges::find_if(
      kKeyBindings, [qt_key](const KeyBinding& candidate) -> bool {
        return candidate.qt_key == qt_key;
      });
  if (binding == kKeyBindings.end()) {
    return std::nullopt;
  }
  return binding->key;
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

// Qt reports the wheel in eighths of a degree; one notch is 15 degrees
constexpr float kWheelUnitsPerNotch = 120.0F;

}  // namespace

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

auto ViewportWindow::eventFilter(QObject* watched, QEvent* event) -> bool {
  switch (event->type()) {
    case QEvent::KeyPress:
    case QEvent::KeyRelease:
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
      RecordKey(static_cast<QKeyEvent*>(event),
                event->type() == QEvent::KeyPress);
      // RecordKey ignores the keys the engine doesn't read
      return event->isAccepted();
    case QEvent::FocusOut:
      ReleaseInput();
      break;
    default:
      break;
  }
  return QWindow::eventFilter(watched, event);
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

void ViewportWindow::wheelEvent(QWheelEvent* event) {
  const float notches =
      static_cast<float>(event->angleDelta().y()) / kWheelUnitsPerNotch;
  // Natural scrolling flips the sign; undo it so positive is away
  input_.AddWheel(event->inverted() ? -notches : notches);
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
      .input = input_,
  };

  // The launcher's play-mode frame, without devtools
  session_->PrePhysics(*engine_, ctx, /*has_control=*/true);
  engine_->Update(ctx);
  session_->PostPhysics(*engine_);
  engine_->Render(ctx, {.debug_draw = false});
  engine_->EndFrame();
  // Events until the next tick belong to the next frame
  input_.BeginFrame();
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

auto ViewportWindow::ToBackbuffer(const QPointF& position) const
    -> ScreenPosition {
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
