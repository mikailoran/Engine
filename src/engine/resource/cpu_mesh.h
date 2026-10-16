#pragma once

#include <bx/bounds.h>
#include <bx/math.h>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include "engine/resource/skeleton.h"

namespace engine {

/**
 * @brief A run of a mesh's indices that shares one material, drawn in one
 * call. Shared by CpuMesh and GpuMesh.
 */
struct Submesh {
  /// Offset into the mesh's indices where the run starts.
  std::uint32_t first_index{0};
  /// Number of indices in the run, a multiple of 3.
  std::uint32_t index_count{0};
  /// Index into the mesh's materials; empty draws the default (white,
  /// untextured).
  std::optional<std::uint32_t> material;
  /// Box around the vertices this run uses, in mesh space.
  bx::Aabb bounds{.min = bx::InitZero, .max = bx::InitZero};
};

/**
 * @brief How a submesh looks: glTF's base color, metallic-roughness factors
 * and emission. Shared by CpuMesh and GpuMesh.
 *
 * Defaults are a white dielectric, used for primitives without a glTF
 * material (glTF's own defaults would make them fully metallic).
 */
struct MeshMaterial {
  /// Linear RGBA, multiplied with the image's color.
  std::array<float, 4> base_color{1.0F, 1.0F, 1.0F, 1.0F};
  /// Index into the mesh's images (textures on the GPU); empty for none.
  std::optional<std::uint32_t> base_color_image;
  /// 0 dielectric to 1 metal.
  float metallic{0.0F};
  /// Perceptual roughness, 0 mirror-smooth to 1 fully rough.
  float roughness{0.5F};
  /// Linear RGB light the surface gives off, unlit and unshadowed; may
  /// exceed 1 (KHR_emissive_strength). Multiplied with the image's color.
  std::array<float, 3> emissive{0.0F, 0.0F, 0.0F};
  /// Index into the mesh's images for the emissive color; empty for none.
  std::optional<std::uint32_t> emissive_image;
};

/**
 * @brief An image decoded to RGBA8 with its full mip chain, ready to upload.
 * Its colors are sRGB, as glTF base color and emissive images are.
 */
struct CpuImage {
  std::uint16_t width{0};
  std::uint16_t height{0};
  /// Levels in rgba8, down to 1x1.
  std::uint8_t mip_count{0};
  /// Every mip level in turn, largest first, 4 bytes per pixel.
  std::vector<std::uint8_t> rgba8;
};

/**
 * @brief A mesh in RAM: its geometry, one array per vertex attribute, and the
 * materials and images its submeshes use.
 *
 * Made by ReadGltf; needs no bgfx. UploadMesh turns it into a GpuMesh, and
 * AssetRegistry builds collision meshes from its positions and indices.
 */
struct CpuMesh {
  /// In mesh space, in metres: right-handed, +Y up. Skinned meshes are in
  /// bind pose.
  std::vector<bx::Vec3> positions;
  /// Unit length, one per position.
  std::vector<bx::Vec3> normals;
  /// One per position; zeros when the source has no UVs.
  std::vector<std::array<float, 2>> uvs;
  /// One per position: the glTF node that moves it most, i.e. its heaviest
  /// joint, or the node holding its mesh when unskinned.
  std::vector<std::uint32_t> bones;
  /// One per position when skinned, else empty: four indices into
  /// skeleton's joints.
  std::vector<std::array<std::uint16_t, 4>> joints;
  /// One per position when skinned, else empty: the matching weights,
  /// summing to 1.
  std::vector<std::array<float, 4>> weights;
  /// Three per triangle, counter-clockwise seen from the front.
  std::vector<std::uint32_t> indices;
  /// Cover indices exactly, in order, one per material.
  std::vector<Submesh> submeshes;
  /// Box around every position.
  bx::Aabb bounds{.min = bx::InitZero, .max = bx::InitZero};
  /// Indexed by Submesh::material.
  std::vector<MeshMaterial> materials;
  /// Indexed by MeshMaterial's *_image fields; each decoded once.
  std::vector<CpuImage> images;
  /// The file's skin; empty when it has none.
  std::optional<Skeleton> skeleton;
};

}  // namespace engine
