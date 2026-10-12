#pragma once

#include <QWindow>
#include <optional>

#include "engine/platform/input.h"
#include "engine/platform/native_surface.h"
#include "engine/platform/screen.h"

class QEvent;
class QExposeEvent;
class QFocusEvent;
class QKeyEvent;
class QMouseEvent;
class QPointF;

namespace editor {

/**
 * @brief The native window the engine renders the game into, and the keyboard
 * and mouse input it receives.
 *
 * Embedded in the main window through QWidget::createWindowContainer, which
 * takes ownership of it. Qt never paints it: bgfx presents to it directly. The
 * native surface exists from the first Exposed(true) until SurfaceLost.
 */
class ViewportWindow : public QWindow {
  Q_OBJECT

 public:
  /** @brief Creates the window as a Vulkan surface, for bgfx to present to. */
  explicit ViewportWindow(QWindow* parent = nullptr);

  /**
   * @brief This frame's input. Clears presses and mouse motion, so events
   * from here on count toward the next frame.
   */
  [[nodiscard]] auto TakeFrameInput() -> engine::Input;

  /**
   * @brief The native handles and backbuffer size, for the engine. Valid once
   * the window has been exposed.
   * @throws std::runtime_error If Qt isn't running on xcb.
   */
  [[nodiscard]] auto Surface() const -> engine::NativeSurface;

  /** @brief Current size in device pixels: logical size times the scale. */
  [[nodiscard]] auto BackbufferSize() const -> engine::PixelSize;

 signals:
  /**
   * @brief Emitted on every expose event. While @p on_screen, the native
   * surface exists and frames are worth rendering.
   */
  void Exposed(bool on_screen);

  /**
   * @brief The native surface is about to be destroyed. A receiver must let
   * go of it before returning, so connect directly, never queued.
   */
  void SurfaceLost();

 protected:
  /** @brief Reports whether the window is on screen. */
  void exposeEvent(QExposeEvent* event) override;

  /** @brief Emits SurfaceLost before Qt destroys the native surface. */
  auto event(QEvent* e) -> bool override;

  /** @brief Records a key going down; unmapped keys go on to Qt. */
  void keyPressEvent(QKeyEvent* event) override;

  /** @brief Records a key going up; unmapped keys go on to Qt. */
  void keyReleaseEvent(QKeyEvent* event) override;

  /** @brief Releases every key and button, so none stays held unfocused. */
  void focusOutEvent(QFocusEvent* event) override;

  /** @brief Records a button going down; the right one starts looking. */
  void mousePressEvent(QMouseEvent* event) override;

  /** @brief Records a button going up; the right one stops looking. */
  void mouseReleaseEvent(QMouseEvent* event) override;

  /** @brief Records the cursor, and while looking, how far it moved. */
  void mouseMoveEvent(QMouseEvent* event) override;

 private:
  /**
   * @brief Sets @p event's key in the input, as the keyboard layout names it.
   * Auto-repeats are dropped, and keys the engine doesn't read are ignored.
   */
  void RecordKey(QKeyEvent* event, bool down);

  /**
   * @brief Sets @p event's button in the input. Holding the right button looks
   * around: the cursor hides and its movement becomes mouse motion.
   */
  void RecordButton(QMouseEvent* event, bool down);

  /** @brief Releases every key and button and stops looking. */
  void ReleaseInput();

  /** @brief @p position, in Qt's logical pixels, in backbuffer pixels. */
  [[nodiscard]] auto ToBackbuffer(QPointF position) const
      -> engine::ScreenPosition;

  // This frame's keyboard and mouse, filled by Qt's events between ticks
  engine::Input input_;
  // Cursor at the last look step; empty unless the right button is held
  std::optional<engine::ScreenPosition> look_from_;
};

}  // namespace editor
