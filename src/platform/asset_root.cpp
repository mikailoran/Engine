#include "asset_root.h"

#include <bx/filepath.h>

#include <cassert>

auto AssetRoot() -> std::string {
  // Dir::Executable resolves the executable's own file path (/proc/self/exe on
  // Linux); getPath() trims the file name and keeps the trailing '/'.
  const bx::FilePath exe_path(bx::Dir::Executable);
  const bx::StringView dir = exe_path.getPath();

  // bx yields an empty path on platforms where it cannot query the executable.
  // An empty root would make every asset path resolve against the working
  // directory instead, which is exactly the failure this function exists to
  // remove -- so fail loudly here rather than at the first missing shader.
  assert(!dir.isEmpty() && "could not resolve the executable's directory");

  return {dir.getPtr(), static_cast<std::size_t>(dir.getLength())};
}
