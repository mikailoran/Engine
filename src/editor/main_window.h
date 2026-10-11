#pragma once

#include <QMainWindow>

namespace editor {

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget* parent = nullptr);

 private:
};

}  // namespace editor
