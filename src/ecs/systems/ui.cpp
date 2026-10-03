#include "ecs/systems/ui.h"

#include <dear-imgui/imgui.h>
#include <entry/entry.h>
#include <imgui/imgui.h>

#include <cstdint>
#include <format>

#include "ecs/components/configurable.h"
#include "ecs/components/renderable.h"
#include "ecs/components/rigid_body.h"
#include "ecs/components/spin.h"
#include "ecs/components/transform.h"
#include "ecs/core/ecs.h"
#include "ecs/core/frame_context.h"
#include "ecs/core/types.h"
#include "resource/asset_registry.h"
#include "resource/mesh_handle.h"

void UiSystem::Init(AssetRegistry& asset_registry) {
  imguiCreate();
  asset_registry_ = &asset_registry;
}

void UiSystem::Shutdown() { imguiDestroy(); }

auto UiSystem::SpawnEntity(Ecs& ecs) -> Entity {
  // TODO: remove paths
  const MeshHandle mesh_handle =
      asset_registry_->LoadMesh("assets/meshes/bunny.bin");
  const auto entity = ecs.CreateEntity();
  ecs.AddComponent(entity, Transform{.position = {0.0F, 3.0F, 0.0F}});
  ecs.AddComponent(entity, RigidBody{});
  ecs.AddComponent(entity, Spin{});
  ecs.AddComponent(entity, Configurable{});
  ecs.AddComponent(entity, Renderable{.mesh_handle = mesh_handle});

  return entity;
}

void UiSystem::Update(Ecs& ecs, const FrameContext& ctx) {
  // Wait for windowing set up to finish
  if (ctx.width <= 1 || ctx.height <= 1) {
    return;
  }

  const auto& mouse = *ctx.mouse;
  imguiBeginFrame(
      mouse.m_mx, mouse.m_my,
      (mouse.m_buttons[entry::MouseButton::Left] ? IMGUI_MBUT_LEFT : 0) |
          (mouse.m_buttons[entry::MouseButton::Right] ? IMGUI_MBUT_RIGHT : 0) |
          (mouse.m_buttons[entry::MouseButton::Middle] ? IMGUI_MBUT_MIDDLE : 0),
      mouse.m_mz, static_cast<std::uint16_t>(ctx.width),
      static_cast<std::uint16_t>(ctx.height));

  const auto width = static_cast<float>(ctx.width);
  const auto height = static_cast<float>(ctx.height);
  ImGui::SetNextWindowPos(ImVec2(width - (width / 5.0f) - 10.0f, 10.0f),
                          ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(width / 5.0f, height / 3.5f),
                           ImGuiCond_FirstUseEver);
  ImGui::Begin("Settings", nullptr, 0);

  if (ImGui::Button("Spawn Entity")) {
    SpawnEntity(ecs);
  }

  // Sliders of transforms of entities
  ecs.View<Configurable, Transform, Spin, RigidBody, Renderable>().ForEach(
      [&ecs](Entity entity, Configurable&, Transform& transform, Spin& spin,
             RigidBody& rigid_body, Renderable& renderable) -> void {
        ImGui::PushID(static_cast<int>(entity));

        ImGui::TextUnformatted(std::format("Entity {}", entity).c_str());
        if (ImGui::Button("Destroy")) {
          ecs.DestroyEntity(entity);
        }
        ImGui::SliderFloat("x", &transform.position.x, -10.0F, 10.0F);
        ImGui::SliderFloat("y", &transform.position.y, 0.0F, 10.0F);
        ImGui::SliderFloat("Scale", &transform.scale.x, 0.1F, 10.0F);
        if (ImGui::CollapsingHeader("Color Picker")) {
          ImGui::ColorPicker3(renderable.color.data());
        }
        ImGui::Checkbox("Gravity", &rigid_body.has_gravity);
        ImGui::SameLine();
        ImGui::Checkbox("Spin", &spin.should_spin);
        ImGui::NewLine();

        ImGui::PopID();
        transform.scale.z = transform.scale.y = transform.scale.x;
      });

  ImGui::End();

  imguiEndFrame();
}