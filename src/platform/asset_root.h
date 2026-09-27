#pragma once

#include <string>

/**
 * @brief Directory that runtime asset paths resolve against.
 *
 * Generated assets (compiled shaders and compiled meshes) are emitted next
 * to the executable by the build, so the executable's own directory is the
 * asset root.
 *
 * @return Absolute path ending in '/', as entry::setCurrentDir requires: entry
 *         string-appends it to each asset path rather than using it as a chdir.
 * @pre The platform must support querying the executable path (Linux, Windows,
 *      macOS); asserted, since bx returns an empty path otherwise.
 */
auto AssetRoot() -> std::string;
