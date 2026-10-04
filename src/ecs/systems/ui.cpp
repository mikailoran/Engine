#include "ecs/systems/ui.h"

#include <bx/math.h>
#include <dear-imgui/imgui.h>
#include <entry/entry.h>
#include <imgui/imgui.h>

#include <cassert>
#include <cstdint>
#include <format>
#include <limits>

#include "ecs/components/renderable.h"
#include "ecs/components/rigid_body.h"
#include "ecs/components/selected.h"
#include "ecs/components/spin.h"
#include "ecs/components/transform.h"
#include "ecs/core/ecs.h"
#include "ecs/core/types.h"
#include "platform/frame_context.h"
#include "resource/asset_registry.h"
#include "resource/mesh_handle.h"
#include "resource/texture_handle.h"

namespace {

/** @brief Spawns a bunny. @return The new entity. */
auto SpawnEntity(Ecs& ecs, AssetRegistry& assets) -> Entity {
  // TODO: remove paths
  const MeshHandle mesh_handle = assets.LoadMesh("assets/meshes/bunny.bin");
  const auto entity = ecs.CreateEntity();
  ecs.AddComponent(entity, Transform{.position = {0.0F, 3.0F, 0.0F}});
  ecs.AddComponent(entity, RigidBody{});
  ecs.AddComponent(entity, Spin{});
  ecs.AddComponent(entity, Renderable{.mesh_handle = mesh_handle});

  return entity;
}

/** @brief Runs imguiCreate. @return The context it made current. */
auto CreateImguiContext() -> ImGuiContext* {
  imguiCreate();
  auto* context = ImGui::GetCurrentContext();
  assert(context != nullptr && "imguiCreate made no ImGui context");
  return context;
}

}  // namespace

UiSystem::UiSystem() : context_(CreateImguiContext()) {}

UiSystem::~UiSystem() { imguiDestroy(); }

auto UiSystem::WantsMouse() const -> bool {
  ImGui::SetCurrentContext(context_);
  return ImGui::GetIO().WantCaptureMouse;
}

void UiSystem::Update(Ecs& ecs, AssetRegistry& assets,
                      const FrameContext& ctx) {
  // Draw into this system's context, not whichever is current
  ImGui::SetCurrentContext(context_);

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
    SpawnEntity(ecs, assets);
  }

  ecs.View<Selected>().ForEach([&ecs](Entity entity, const Selected&) -> void {
    ImGui::PushID(static_cast<int>(entity));
    ImGui::TextUnformatted(std::format("Entity {}", entity).c_str());
    if (ImGui::Button("Destroy")) {
      ecs.DestroyEntity(entity);
    }
    if (auto* transform = ecs.TryGetComponent<Transform>(entity)) {
      ImGui::DragFloat3("Position", &transform->position.x, 0.1F);
      ImGui::DragFloat3("Rotation", &transform->rotation.x, bx::toRad(1.0F));
      ImGui::DragFloat3("Scale", &transform->scale.x, 0.1F, 0.01F,
                        std::numeric_limits<float>::max(), "%.2f",
                        ImGuiSliderFlags_AlwaysClamp);
    }
    if (auto* renderable = ecs.TryGetComponent<Renderable>(entity)) {
      if (IsValid(renderable->texture)) {
        ImGui::DragFloat("Texture Scale", &renderable->texture_scale, 0.1F,
                         0.01F, 10.0F, "%.2f", ImGuiSliderFlags_AlwaysClamp);
      }
      if (ImGui::CollapsingHeader("Color Picker")) {
        ImGui::ColorPicker3(renderable->color.data());
      }
    }
    if (auto* rigid_body = ecs.TryGetComponent<RigidBody>(entity)) {
      ImGui::Checkbox("Gravity", &rigid_body->has_gravity);
    }
    if (auto* spin = ecs.TryGetComponent<Spin>(entity)) {
      ImGui::Checkbox("Spin", &spin->should_spin);
    }

    ImGui::PopID();
  });
  ImGui::End();

  imguiEndFrame();
}