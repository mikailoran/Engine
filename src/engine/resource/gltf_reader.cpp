#include "engine/resource/gltf_reader.h"

#include <cgltf/cgltf.h>

#include <cstdint>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <utility>

#include "engine/resource/cpu_mesh.h"
#include "engine/resource/gltf_document.h"
#include "engine/resource/gltf_geometry.h"
#include "engine/resource/gltf_materials.h"
#include "engine/resource/gltf_skin.h"

namespace engine {

namespace {

/**
 * @brief Builds the mesh from the default scene, else the first one, with
 * every material and the images they use.
 */
auto BuildMesh(const cgltf_data& data, const std::filesystem::path& gltf_path)
    -> CpuMesh {
  const cgltf_scene* scene = data.scene;
  if (scene == nullptr && data.scenes_count > 0) {
    scene = data.scenes;
  }
  if (scene == nullptr) {
    throw std::runtime_error("no scene");
  }

  gltf::MeshBuilder builder;
  builder.buckets.resize(data.materials_count + 1);
  for (std::uint32_t i = 0; i < data.materials_count; ++i) {
    builder.buckets.at(i + 1).material = i;
  }
  gltf::AppendScene(data, *scene, builder);

  if (builder.mesh.positions.size() >
      std::numeric_limits<std::uint32_t>::max()) {
    throw std::runtime_error("too many vertices");
  }
  bool has_triangles = false;
  for (const gltf::MaterialBucket& bucket : builder.buckets) {
    has_triangles = has_triangles || !bucket.indices.empty();
  }
  if (!has_triangles) {
    throw std::runtime_error("no triangles");
  }
  // Static parts would need a joint of their own to follow the skeleton
  if (builder.skinned_primitives > 0 && builder.static_primitives > 0) {
    throw std::runtime_error("mixed skinned and static meshes");
  }
  gltf::CutSubmeshes(builder);
  gltf::ReadMaterials(data, gltf_path, builder.mesh);
  // One skin at most: an entity poses one skeleton
  if (data.skins_count > 1) {
    throw std::runtime_error("more than one skin");
  }
  if (data.skins_count == 1) {
    builder.mesh.skeleton = gltf::ReadSkeleton(*data.skins);
  }
  return std::move(builder.mesh);
}

}  // namespace

auto ReadGltf(const std::filesystem::path& path) -> CpuMesh {
  // Tag every error with the file
  try {
    const gltf::CgltfData data = gltf::Parse(path);
    return BuildMesh(*data, path);
  } catch (const std::runtime_error& e) {
    throw std::runtime_error(path.string() + ": " + e.what());
  }
}

}  // namespace engine
