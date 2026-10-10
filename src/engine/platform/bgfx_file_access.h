#pragma once

#include <string>

namespace engine {

/**
 * @brief Sets the directory every asset path is resolved against.
 *
 * bgfx_utils (loadProgram) opens files through
 * entry::getFileReader, which bgfx_file_access.cpp defines in place of bgfx's
 * entry layer; it prepends this root to each path.
 *
 * @param root Absolute path ending in '/', as AssetRoot returns. Set it before
 *        any asset loads.
 */
void SetAssetRoot(std::string root);

}  // namespace engine
