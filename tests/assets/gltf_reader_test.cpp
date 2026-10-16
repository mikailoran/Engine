// Every glTF asset in the source tree must load, so a bad export fails ctest
// instead of the game. Reads files only; nothing here needs bgfx.

#include "engine/resource/gltf_reader.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "engine/resource/cpu_mesh.h"

namespace engine {

namespace {

/** @brief Every .glb and .gltf under the source tree's assets/. */
auto GltfAssets() -> std::vector<std::filesystem::path> {
  std::vector<std::filesystem::path> paths;
  for (const auto& entry :
       std::filesystem::recursive_directory_iterator(ENGINE_ASSET_DIR)) {
    const auto extension = entry.path().extension();
    if (extension == ".glb" || extension == ".gltf") {
      paths.push_back(entry.path());
    }
  }
  return paths;
}

/** @brief Checks @p image holds every mip level down to 1x1, in RGBA8. */
void ExpectFullMipChain(const CpuImage& image) {
  ASSERT_GT(image.width, 0);
  ASSERT_GT(image.height, 0);
  std::size_t bytes = 0;
  std::uint32_t width = image.width;
  std::uint32_t height = image.height;
  std::uint8_t levels = 1;
  bytes += std::size_t{width} * height * 4;
  while (width > 1 || height > 1) {
    width = std::max(width / 2, 1U);
    height = std::max(height / 2, 1U);
    bytes += std::size_t{width} * height * 4;
    ++levels;
  }
  EXPECT_EQ(image.mip_count, levels);
  EXPECT_EQ(image.rgba8.size(), bytes);
}

/** @brief Checks the invariants CpuMesh documents. */
void ExpectWellFormed(const CpuMesh& mesh) {
  ASSERT_FALSE(mesh.indices.empty());
  EXPECT_EQ(mesh.indices.size() % 3, 0U);
  EXPECT_EQ(mesh.normals.size(), mesh.positions.size());
  EXPECT_EQ(mesh.uvs.size(), mesh.positions.size());
  EXPECT_EQ(mesh.bones.size(), mesh.positions.size());
  for (const std::uint32_t index : mesh.indices) {
    ASSERT_LT(index, mesh.positions.size());
  }

  // Submeshes tile the indices in order
  std::size_t next = 0;
  for (const Submesh& submesh : mesh.submeshes) {
    EXPECT_EQ(submesh.first_index, next);
    EXPECT_GT(submesh.index_count, 0U);
    next = submesh.first_index + submesh.index_count;
  }
  EXPECT_EQ(next, mesh.indices.size());

  // Every reference lands inside its array
  for (const Submesh& submesh : mesh.submeshes) {
    if (submesh.material) {
      EXPECT_LT(*submesh.material, mesh.materials.size());
    }
  }
  for (const MeshMaterial& material : mesh.materials) {
    if (material.base_color_image) {
      EXPECT_LT(*material.base_color_image, mesh.images.size());
    }
    EXPECT_GE(material.metallic, 0.0F);
    EXPECT_LE(material.metallic, 1.0F);
    EXPECT_GE(material.roughness, 0.0F);
    EXPECT_LE(material.roughness, 1.0F);
    if (material.emissive_image) {
      EXPECT_LT(*material.emissive_image, mesh.images.size());
    }
  }
  for (const CpuImage& image : mesh.images) {
    ExpectFullMipChain(image);
  }
}

TEST(GltfReader, EveryAssetLoads) {
  const auto paths = GltfAssets();
  ASSERT_FALSE(paths.empty()) << "no glTF under " << ENGINE_ASSET_DIR;

  for (const auto& path : paths) {
    SCOPED_TRACE(path.string());
    const CpuMesh mesh = ReadGltf(path);
    ExpectWellFormed(mesh);
  }
}

TEST(GltfReader, ReadsBaseColorTexture) {
  // A Blender cube with its generated 512x512 color grid packed in
  const CpuMesh mesh =
      ReadGltf(std::filesystem::path(ENGINE_ASSET_DIR) / "meshes/uv_cube.glb");

  ASSERT_EQ(mesh.submeshes.size(), 1U);
  ASSERT_EQ(mesh.materials.size(), 1U);
  ASSERT_EQ(mesh.images.size(), 1U);
  EXPECT_EQ(mesh.submeshes.front().material, 0U);
  EXPECT_EQ(mesh.materials.front().base_color_image, 0U);
  EXPECT_EQ(mesh.images.front().width, 512);
  EXPECT_EQ(mesh.images.front().height, 512);
  EXPECT_EQ(mesh.images.front().mip_count, 10);
}

TEST(GltfReader, ReadsMetallicRoughnessFactors) {
  // Every material in the level is a dielectric at roughness 0.8
  const CpuMesh mesh = ReadGltf(std::filesystem::path(ENGINE_ASSET_DIR) /
                                "levels/level02.glb");

  ASSERT_FALSE(mesh.materials.empty());
  for (const MeshMaterial& material : mesh.materials) {
    EXPECT_FLOAT_EQ(material.metallic, 0.0F);
    EXPECT_FLOAT_EQ(material.roughness, 0.8F);
  }
}

TEST(GltfReader, ScalesEmissiveByStrength) {
  // The shot beam: emissive (1, 0.85, 0.3) at KHR_emissive_strength 4
  const CpuMesh mesh =
      ReadGltf(std::filesystem::path(ENGINE_ASSET_DIR) / "meshes/beam.glb");

  ASSERT_EQ(mesh.materials.size(), 1U);
  const MeshMaterial& material = mesh.materials.front();
  EXPECT_FLOAT_EQ(material.emissive.at(0), 4.0F);
  EXPECT_FLOAT_EQ(material.emissive.at(1), 3.4F);
  EXPECT_FLOAT_EQ(material.emissive.at(2), 1.2F);
  EXPECT_FALSE(material.emissive_image);
}

TEST(GltfReader, UnskinnedVerticesFollowTheirNode) {
  const CpuMesh mesh =
      ReadGltf(std::filesystem::path(ENGINE_ASSET_DIR) / "meshes/cube.glb");

  const std::set<std::uint32_t> bones(mesh.bones.begin(), mesh.bones.end());
  EXPECT_EQ(bones.size(), 1U);
}

TEST(GltfReader, SkinnedVerticesFollowTheirJoints) {
  // Quaternius' T-posed character: 31 joints, one mesh node
  const CpuMesh mesh = ReadGltf(std::filesystem::path(ENGINE_ASSET_DIR) /
                                "meshes/characters/Male_Casual.glb");

  const std::set<std::uint32_t> bones(mesh.bones.begin(), mesh.bones.end());
  EXPECT_GT(bones.size(), 10U);
  EXPECT_LE(bones.size(), 31U);
}

TEST(GltfReader, ErrorNamesTheFile) {
  const std::filesystem::path path = "does/not/exist.glb";
  try {
    ReadGltf(path);
    FAIL() << "expected a throw";
  } catch (const std::runtime_error& e) {
    EXPECT_NE(std::string(e.what()).find(path.string()), std::string::npos);
  }
}

}  // namespace

}  // namespace engine
