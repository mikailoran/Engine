#include "engine/platform/bgfx_file_access.h"

#include <bx/allocator.h>
#include <bx/error.h>
#include <bx/file.h>
#include <bx/filepath.h>
#include <entry/entry.h>  // declares the two functions bgfx_utils calls

#include <string>
#include <utility>

namespace {

/** @brief The root every asset path is prefixed with. */
auto Root() -> std::string& {
  static std::string root;
  return root;
}

/** @brief A file reader that opens paths relative to the asset root. */
class RootedFileReader : public bx::FileReader {
 public:
  /** @brief Opens @p path under the asset root. */
  auto open(const bx::FilePath& path, bx::Error* err) -> bool override {
    const std::string full = Root() + path.getCPtr();
    return bx::FileReader::open(bx::FilePath(full.c_str()), err);
  }
};

}  // namespace

namespace engine {

void SetAssetRoot(std::string root) { Root() = std::move(root); }

}  // namespace engine

// bgfx_utils.cpp calls these by entry's names; entry itself is not linked
namespace entry {

/** @brief The reader bgfx_utils loads shaders, meshes and textures through. */
// NOLINTNEXTLINE(readability-identifier-naming)
auto getFileReader() -> bx::FileReaderI* {
  static RootedFileReader reader;
  return &reader;
}

/** @brief The allocator bgfx_utils loads files into and frees them from. */
// NOLINTNEXTLINE(readability-identifier-naming)
auto getAllocator() -> bx::AllocatorI* {
  static bx::DefaultAllocator allocator;
  return &allocator;
}

}  // namespace entry
