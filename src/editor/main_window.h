#pragma once

#include <QMainWindow>
#include <filesystem>

namespace editor {

class ViewportWindow;

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget* parent = nullptr);

 private:
  /**
   * @brief Loads @p path into the viewport, or shows why it couldn't. A failed
   * load leaves the editor running.
   */
  void OpenScene(const std::filesystem::path& path);

  // Owned by the central widget's window container.
  ViewportWindow* viewport_{nullptr};
};

}  // namespace editor
