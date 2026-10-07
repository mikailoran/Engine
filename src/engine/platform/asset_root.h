#pragma once

#include <string>

/**
 * @brief Directory that runtime asset paths resolve against.
 *
 * Generated assets (compiled shaders and compiled meshes) are emitted next
 * to the executable by the build, so the executable's own directory is the
 * asset root.
 *
 * @return Absolute path ending in '/', as SetAssetRoot requires: it is
 *         string-appended to each asset path rather than used as a chdir.
 * @pre The platform must support querying the executable path (Linux, Windows,
 *      macOS); asserted, since bx returns an empty path otherwise.
 */
auto AssetRoot() -> std::string;
