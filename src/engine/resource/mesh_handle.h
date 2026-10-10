#pragma once

#include <cstdint>

#include "engine/util/handle.h"  // IWYU pragma: export

namespace engine {

/**
 * @brief Opaque id of a GpuMesh in an AssetRegistry. Meshes live until the
 * registry is destroyed, so a valid id never goes stale.
 */
using MeshHandle = Handle<struct MeshTag, std::uint16_t>;

}  // namespace engine
