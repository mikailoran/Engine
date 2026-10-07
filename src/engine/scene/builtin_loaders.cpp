#include "engine/scene/builtin_loaders.h"

#include <bx/math.h>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <stdexcept>
#include <string>

#include "engine/ecs/components/collider.h"
#include "engine/ecs/components/directional_light.h"
#include "engine/ecs/components/renderable.h"
#include "engine/ecs/components/transform.h"
#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"
#include "engine/math/rotation.h"
#include "engine/physics/shape.h"
#include "engine/resource/asset_registry.h"
#include "engine/scene/json_read.h"
#include "engine/scene/scene_loader.h"

namespace {

using nlohmann::json;

/** @brief Adds a Transform; every field is optional. Rotation is in degrees. */
void LoadTransform(const json& data, Entity entity, Ecs& ecs,
                   AssetRegistry& /*assets*/) {
  CheckKeys(data, {"position", "rotation_deg", "scale"});

  Transform transform{};
  if (data.contains("position")) {
    transform.position = ReadVec3(data.at("position"));
  }
  if (data.contains("rotation_deg")) {
    const auto deg = ReadVec3(data.at("rotation_deg"));
    transform.rotation =
        EulerToQuat({bx::toRad(deg.x), bx::toRad(deg.y), bx::toRad(deg.z)});
  }
  if (data.contains("scale")) {
    transform.scale = ReadVec3(data.at("scale"));
  }
  ecs.AddComponent(entity, transform);
}

/**
 * @brief Adds a Renderable; "mesh" is required, "color", "texture" and
 * "texture_scale" optional.
 */
void LoadRenderable(const json& data, Entity entity, Ecs& ecs,
                    AssetRegistry& assets) {
  CheckKeys(data, {"mesh", "color", "texture", "texture_scale"});

  Renderable renderable{};
  renderable.mesh_handle = assets.LoadMesh(data.at("mesh").get<std::string>());
  if (data.contains("color")) {
    renderable.color = ReadFloats<4>(data.at("color"));
  }
  if (data.contains("texture")) {
    renderable.texture =
        assets.LoadTexture(data.at("texture").get<std::string>());
  }
  if (data.contains("texture_scale")) {
    renderable.texture_scale = data.at("texture_scale").get<float>();
    // The shader divides by it
    if (renderable.texture_scale <= 0.0F) {
      throw std::runtime_error("texture_scale must be positive");
    }
  }
  ecs.AddComponent(entity, renderable);
}

/** @brief Adds a DirectionalLight; every field is optional. */
void LoadDirectionalLight(const json& data, Entity entity, Ecs& ecs,
                          AssetRegistry& /*assets*/) {
  CheckKeys(data,
            {"direction", "color", "intensity", "sky_color", "ground_color"});
  DirectionalLight light{};
  if (data.contains("direction")) {
    light.direction = ReadVec3(data.at("direction"));
  }
  if (data.contains("color")) {
    light.color = ReadVec3(data.at("color"));
  }
  if (data.contains("intensity")) {
    light.intensity = data.at("intensity").get<float>();
  }
  if (data.contains("sky_color")) {
    light.sky_color = ReadVec3(data.at("sky_color"));
  }
  if (data.contains("ground_color")) {
    light.ground_color = ReadVec3(data.at("ground_color"));
  }
  ecs.AddComponent(entity, light);
}

/**
 * @brief Reads a Collider's optional "shape" ("box" or "sphere"), its size
 * ("half_extents" for a box, "radius" for a sphere) and "offset".
 */
void ReadShapeKind(const json& data, Collider& collider) {
  // TODO: Who defines default values?
  if (data.contains("shape")) {
    const auto shape = data.at("shape").get<std::string>();
    if (shape != "box" && shape != "sphere") {
      throw std::runtime_error(R"(shape must be "box" or "sphere")");
    }
    collider.shape.kind =
        shape == "sphere" ? ShapeKind::kSphere : ShapeKind::kBox;
  }
  const bool sphere = collider.shape.kind == ShapeKind::kSphere;
  // A size for the other shape is a typo, not something to ignore
  if (data.contains(sphere ? "half_extents" : "radius")) {
    throw std::runtime_error(sphere ? R"(half_extents needs shape "box")"
                                    : R"(radius needs shape "sphere")");
  }
  if (data.contains("half_extents")) {
    collider.shape.half_extents = ReadVec3(data.at("half_extents"));
    const bx::Vec3& half = collider.shape.half_extents;
    if (half.x <= 0.0F || half.y <= 0.0F || half.z <= 0.0F) {
      throw std::runtime_error("half_extents must be positive");
    }
  }
  if (data.contains("radius")) {
    collider.shape.radius = data.at("radius").get<float>();
    if (collider.shape.radius <= 0.0F) {
      throw std::runtime_error("radius must be positive");
    }
  }
  if (data.contains("offset")) {
    collider.shape.offset = ReadVec3(data.at("offset"));
  }
}

/** @brief Reads a Collider's optional "restitution" and "friction". */
void ReadColliderMaterial(const json& data, Collider& collider) {
  if (data.contains("restitution")) {
    collider.material.restitution = data.at("restitution").get<float>();
    if (collider.material.restitution < 0.0F ||
        collider.material.restitution > 1.0F) {
      throw std::runtime_error("restitution must be within [0, 1]");
    }
  }
  if (data.contains("friction")) {
    collider.material.friction = data.at("friction").get<float>();
    if (collider.material.friction < 0.0F) {
      throw std::runtime_error("friction must not be negative");
    }
  }
}

/**
 * @brief Adds a Collider; every field is optional. Defaults to a box fitting
 * the unit primitive meshes.
 */
void LoadCollider(const json& data, Entity entity, Ecs& ecs,
                  AssetRegistry& /*assets*/) {
  CheckKeys(data, {"shape", "half_extents", "radius", "offset", "restitution",
                   "friction"});
  Collider collider{};
  ReadShapeKind(data, collider);
  ReadColliderMaterial(data, collider);
  ecs.AddComponent(entity, collider);
}

}  // namespace

void RegisterBuiltinLoaders(SceneLoader& loader) {
  loader.Register("transform", LoadTransform);
  loader.Register("renderable", LoadRenderable);
  loader.Register("directional_light", LoadDirectionalLight);
  loader.Register("collider", LoadCollider);
}
