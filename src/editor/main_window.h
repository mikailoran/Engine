#pragma once

#include <QMainWindow>
#include <filesystem>

class QAction;

namespace editor {

class ViewportWindow;

/**
 * @brief The editor's top-level window.
 *
 * Opens the launcher's start scene once the viewport's engine is Ready. Scenes
 * are opened from the source tree's `assets/scenes/`.
 */
class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  /**
   * @brief Builds the menus and embeds a new viewport as the central widget.
   * Requires a QApplication. The engine waits for the viewport's first expose.
   */
  explicit MainWindow(QWidget* parent = nullptr);

 private:
  /**
   * @brief Loads @p path into the viewport, or shows why it couldn't. A failed
   * load leaves the editor running, and Reload tries the same file again.
   */
  void OpenScene(const std::filesystem::path& path);

  /** @brief Asks for a scene file in the source tree, then opens it. */
  void ChooseScene();

  // Owned by the central widget's window container.
  ViewportWindow* viewport_{nullptr};

  // Owned by the File menu; disabled until a scene has been opened.
  QAction* reload_{nullptr};

  // The scene last opened, which Reload loads again.
  std::filesystem::path scene_;
};

}  // namespace editor
