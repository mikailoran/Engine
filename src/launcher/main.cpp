#include <exception>
#include <iostream>

#include "engine/platform/asset_root.h"
#include "engine/platform/bgfx_file_access.h"
#include "launcher/standalone_host.h"

using engine::AssetRoot;
using engine::SetAssetRoot;

/** @brief Sets the asset root, then runs the game until its window closes. */
auto main() -> int {
  // bgfx_utils prepends this to every asset path; must precede any load
  SetAssetRoot(AssetRoot());

  try {
    launcher::StandaloneHost host;
    return host.Run();
  } catch (const std::exception& e) {
    std::cerr << "fatal: " << e.what() << '\n';
    return 1;
  }
}
