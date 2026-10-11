#pragma once

#include <QMainWindow>
#include <filesystem>

class QAction;

namespace editor {

class ViewportWindow;

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
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
