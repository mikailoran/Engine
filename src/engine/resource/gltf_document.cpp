#include "engine/resource/gltf_document.h"

#include <cgltf/cgltf.h>

#include <cstddef>
#include <filesystem>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace engine::gltf {

auto Parse(const std::filesystem::path& path) -> CgltfData {
  const cgltf_options options{};
  cgltf_data* raw = nullptr;
  if (cgltf_parse_file(&options, path.c_str(), &raw) != cgltf_result_success) {
    throw std::runtime_error("cannot parse glTF");
  }
  CgltfData data(raw);

  if (cgltf_load_buffers(&options, data.get(), path.c_str()) !=
      cgltf_result_success) {
    throw std::runtime_error("cannot load glTF buffers");
  }
  if (cgltf_validate(data.get()) != cgltf_result_success) {
    throw std::runtime_error("invalid glTF");
  }
  // Compressed or quantized data would read back wrong
  const std::span required(data->extensions_required,
                           data->extensions_required_count);
  if (!required.empty()) {
    throw std::runtime_error("unsupported required extension '" +
                             std::string(required.front()) + "'");
  }
  return data;
}

auto UnpackFloats(const cgltf_accessor& accessor, std::size_t components,
                  const char* what) -> std::vector<float> {
  if (cgltf_num_components(accessor.type) != components) {
    throw std::runtime_error(std::string(what) + " has the wrong element type");
  }
  std::vector<float> floats(accessor.count * components);
  cgltf_accessor_unpack_floats(&accessor, floats.data(), floats.size());
  return floats;
}

}  // namespace engine::gltf
