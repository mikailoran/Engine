#include <QApplication>

#include "editor/main_window.h"
#include "engine/platform/asset_root.h"
#include "engine/platform/bgfx_file_access.h"

auto main(int argc, char* argv[]) -> int {
  engine::SetAssetRoot(engine::AssetRoot());

  const QApplication app(argc, argv);
  editor::MainWindow window;
  window.show();
  return QApplication::exec();
}
