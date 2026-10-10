#include "engine/scene/builtin_loaders.h"

#include <bx/math.h>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <stdexcept>
#include <string>

#include "engine/ecs/components/character_body.h"
#include "engine/ecs/components/collider.h"
#include "engine/ecs/components/directional_light.h"
#include "engine/ecs/components/renderable.h"
#include "engine/ecs/components/rigid_body.h"
#include "engine/ecs/components/transform.h"
#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"
#include "engine/math/rotation.h"
#include "engine/physics/physics_world.h"
#include "engine/physics/shape.h"
#include "engine/resource/asset_registry.h"
#include "engine/scene/json_read.h"
#include "engine/scene/scene_loader.h"

namespace engine {

namespace {

using nlohmann::json;

/** @brief Adds a Transform; every field is optional. Rotation is in degrees. */
void LoadTransform(const json& data, Entity entity, Ecs& ecs,
                   AssetRegistry& /*assets*/,
                   physics::PhysicsWorld& /*physics*/) {
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
 * @brief Adds a Renderable. "mesh" (a glTF path) is required; "color", an
 * RGBA tint, is optional and defaults to white.
 */
void LoadRenderable(const json& data, Entity entity, Ecs& ecs,
                    AssetRegistry& assets, physics::PhysicsWorld& /*physics*/) {
  CheckKeys(data, {"mesh", "color"});

  Renderable renderable{};
  renderable.mesh_handle = assets.LoadMesh(data.at("mesh").get<std::string>());
  if (data.contains("color")) {
    renderable.color = ReadFloats<4>(data.at("color"));
  }
  ecs.AddComponent(entity, renderable);
}

/** @brief Adds a DirectionalLight; every field is optional. */
void LoadDirectionalLight(const json& data, Entity entity, Ecs& ecs,
                          AssetRegistry& /*assets*/,
                          physics::PhysicsWorld& /*physics*/) {
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

/** @brief Parses a Collider's "shape" name. */
auto ParseShapeKind(const std::string& name) -> physics::ShapeKind {
  if (name == "box") {
    return physics::ShapeKind::kBox;
  }
  if (name == "sphere") {
    return physics::ShapeKind::kSphere;
  }
  if (name == "mesh") {
    return physics::ShapeKind::kMesh;
  }
  throw std::runtime_error(R"(shape must be "box", "sphere" or "mesh")");
}

/**
 * @brief Reads a Collider's optional "shape" ("box", "sphere" or "mesh"), the
 * field that shape needs ("half_extents", "radius" or the "mesh" path) and
 * "offset". A mesh's collision mesh is built in @p physics, once per path.
 */
void ReadShapeKind(const json& data, Collider& collider, AssetRegistry& assets,
                   physics::PhysicsWorld& physics) {
  // TODO: Who defines default values?
  if (data.contains("shape")) {
    collider.shape.kind = ParseShapeKind(data.at("shape").get<std::string>());
  }
  const auto kind = collider.shape.kind;
  // A field for another shape is a typo, not something to ignore
  if (data.contains("half_extents") && kind != physics::ShapeKind::kBox) {
    throw std::runtime_error(R"(half_extents needs shape "box")");
  }
  if (data.contains("radius") && kind != physics::ShapeKind::kSphere) {
    throw std::runtime_error(R"(radius needs shape "sphere")");
  }
  if (data.contains("mesh") && kind != physics::ShapeKind::kMesh) {
    throw std::runtime_error(R"(mesh needs shape "mesh")");
  }
  if (kind == physics::ShapeKind::kMesh && !data.contains("mesh")) {
    throw std::runtime_error(R"(shape "mesh" needs a "mesh" path)");
  }
  if (kind == physics::ShapeKind::kMesh) {
    collider.shape.mesh =
        assets.LoadCollisionMesh(data.at("mesh").get<std::string>(), physics);
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
 * @brief Adds a Collider. Every field is optional except a mesh shape's
 * "mesh" path; defaults to a box fitting the unit primitive meshes.
 */
void LoadCollider(const json& data, Entity entity, Ecs& ecs,
                  AssetRegistry& assets, physics::PhysicsWorld& physics) {
  CheckKeys(data, {"shape", "half_extents", "radius", "mesh", "offset",
                   "restitution", "friction"});
  Collider collider{};
  ReadShapeKind(data, collider, assets, physics);
  ReadColliderMaterial(data, collider);
  ecs.AddComponent(entity, collider);
}

/**
 * @brief Adds a RigidBody; every field is optional. "velocity" is the
 * initial velocity in m/s, "acceleration" in m/s^2.
 */
void LoadRigidBody(const json& data, Entity entity, Ecs& ecs,
                   AssetRegistry& /*assets*/,
                   physics::PhysicsWorld& /*physics*/) {
  CheckKeys(data, {"velocity", "acceleration", "has_gravity"});
  RigidBody rigid_body{};
  if (data.contains("velocity")) {
    rigid_body.velocity = ReadVec3(data.at("velocity"));
  }
  if (data.contains("acceleration")) {
    rigid_body.acceleration = ReadVec3(data.at("acceleration"));
  }
  if (data.contains("has_gravity")) {
    rigid_body.has_gravity = data.at("has_gravity").get<bool>();
  }
  ecs.AddComponent(entity, rigid_body);
}

/**
 * @brief Adds a CharacterBody; every field is optional. Sizes in m, the slope
 * limit in degrees, "mass" in kg and "push_force" in N.
 */
void LoadCharacterBody(const json& data, Entity entity, Ecs& ecs,
                       AssetRegistry& /*assets*/,
                       physics::PhysicsWorld& /*physics*/) {
  CheckKeys(data, {"height", "radius", "max_slope_deg", "mass", "push_force"});
  CharacterBody body{};
  ReadPositive(data, "height", body.height);
  ReadPositive(data, "radius", body.radius);
  ReadPositive(data, "max_slope_deg", body.max_slope_deg);
  ReadPositive(data, "mass", body.mass);
  ReadPositive(data, "push_force", body.push_force);
  // The capsule's two rounded ends must fit in its height
  if (body.height <= 2.0F * body.radius) {
    throw std::runtime_error("height must be more than twice the radius");
  }
  ecs.AddComponent(entity, body);
}

}  // namespace

void RegisterBuiltinLoaders(SceneLoader& loader) {
  loader.Register("transform", LoadTransform);
  loader.Register("renderable", LoadRenderable);
  loader.Register("directional_light", LoadDirectionalLight);
  loader.Register("collider", LoadCollider);
  loader.Register("rigid_body", LoadRigidBody);
  loader.Register("character_body", LoadCharacterBody);
}

}  // namespace engine
