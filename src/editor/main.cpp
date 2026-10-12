#include <QApplication>
#include <QtEnvironmentVariables>
#include <clocale>

#include "editor/main_window.h"
#include "engine/platform/asset_root.h"
#include "engine/platform/bgfx_file_access.h"

/**
 * @brief Sets the asset root, forces Qt onto X11, then runs the editor until
 * its window closes.
 */
auto main(int argc, char* argv[]) -> int {
  engine::SetAssetRoot(engine::AssetRoot());

  // X11 (XWayland on Wayland): its window handle is public Qt API
  qputenv("QT_QPA_PLATFORM", "xcb");
  const QApplication app(argc, argv);
  // Qt applied the system locale; C parsers (cgltf's atof) need '.' decimals
  // NOLINTNEXTLINE(concurrency-mt-unsafe)
  std::setlocale(LC_NUMERIC, "C");
  editor::MainWindow window;
  window.show();
  return QApplication::exec();
}
