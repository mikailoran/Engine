// Every glTF asset in the source tree must load, so a bad export fails ctest
// instead of the game. Reads files only; nothing here needs bgfx.

#include "engine/resource/gltf_reader.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
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

/** @brief Checks the invariants CpuMesh documents. */
void ExpectWellFormed(const CpuMesh& mesh) {
  ASSERT_FALSE(mesh.indices.empty());
  EXPECT_EQ(mesh.indices.size() % 3, 0U);
  EXPECT_EQ(mesh.normals.size(), mesh.positions.size());
  EXPECT_EQ(mesh.uvs.size(), mesh.positions.size());
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
