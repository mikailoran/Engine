#pragma once

#include <cgltf/cgltf.h>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <vector>

namespace engine::gltf {

// Internal to engine_assets: no public header includes this one.

/** @brief unique_ptr deleter for parsed glTF data. */
struct CgltfFree {
  /** @brief Frees @p data and the buffers loaded into it. */
  void operator()(cgltf_data* data) const noexcept { cgltf_free(data); }
};

/** @brief A parsed glTF file that owns its loaded buffers. */
using CgltfData = std::unique_ptr<cgltf_data, CgltfFree>;

/**
 * @brief Parses @p path, loads its buffers and validates it.
 * @throws std::runtime_error If it can't be parsed, loaded or validated, or
 *         requires an extension.
 */
auto Parse(const std::filesystem::path& path) -> CgltfData;

/**
 * @brief Reads an accessor as flat floats, @p components per element.
 * @param what Names the accessor in errors, e.g. "POSITION".
 * @throws std::runtime_error If its elements don't have @p components.
 */
auto UnpackFloats(const cgltf_accessor& accessor, std::size_t components,
                  const char* what) -> std::vector<float>;

}  // namespace engine::gltf
