#pragma once

#include <cgltf/cgltf.h>

#include <filesystem>

#include "engine/resource/cpu_mesh.h"

namespace engine::gltf {

// Internal to engine_assets: no public header includes this one.

/**
 * @brief Reads every glTF material, in order, into @p mesh's materials, and
 * decodes each image they use once into @p mesh's images.
 *
 * @param gltf_path The file read, against which image URIs resolve.
 * @throws std::runtime_error Naming the material, if an image can't be read
 *         or decoded, or it uses an unsupported texture feature.
 */
void ReadMaterials(const cgltf_data& data,
                   const std::filesystem::path& gltf_path, CpuMesh& mesh);

}  // namespace engine::gltf
