#include "engine/platform/asset_root.h"

#include <bx/filepath.h>
#include <bx/string.h>

#include <cassert>
#include <cstddef>
#include <string>

namespace engine {

auto AssetRoot() -> std::string {
  const bx::FilePath exe_path(bx::Dir::Executable);
  const bx::StringView dir = exe_path.getPath();

  assert(!dir.isEmpty() && "could not resolve the executable's directory");

  return {dir.getPtr(), static_cast<std::size_t>(dir.getLength())};
}

}  // namespace engine
