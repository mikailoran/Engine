#pragma once

#include <filesystem>

#include "engine/resource/cpu_mesh.h"

namespace engine {

/**
 * @brief Reads a glTF file's geometry into one mesh, with every node's
 * transform baked in and one submesh per material.
 *
 * @param path A .glb or .gltf file.
 * @throws std::runtime_error Naming @p path, if the file can't be read or
 *         uses something unsupported: a required extension, non-triangle
 *         primitives, or primitives without normals.
 */
auto ReadGltf(const std::filesystem::path& path) -> CpuMesh;

}  // namespace engine
