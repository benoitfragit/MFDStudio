/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation of the integrated runtime debug overlay.
 */

#include "RuntimeDebugOverlay.hpp"

#include "RuntimeDebugInspectorFrameState.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <imgui.h>
#include <raylib.h>
#include <rlImGui.h>

namespace mfd::window::debug
{
namespace
{
constexpr int kDebugPanelWidth = 620;
constexpr float kLeftPaneWidth = 330.0f;
constexpr float kBottomPanelHeight = 168.0f;
constexpr std::size_t kTextBufferBaseSize = 256U;

struct ReticleUiEntry
{
    ReticleKey key;
    std::string label;
    bool effectiveVisible = false;
};

ImVec4 ToImGuiColor(const ColorRgba& color) noexcept
{
    return ImVec4(
        static_cast<float>(color.r) / 255.0f,
        static_cast<float>(color.g) / 255.0f,
        static_cast<float>(color.b) / 255.0f,
        static_cast<float>(color.a) / 255.0f);
}

ColorRgba FromImGuiColor(const ImVec4& color) noexcept
{
    const auto clampChannel =
        [](const float value) noexcept -> std::uint8_t
    {
        const float clamped = std::clamp(value, 0.0f, 1.0f);
        return static_cast<std::uint8_t>(clamped * 255.0f + 0.5f);
    };

    return ColorRgba {
        clampChannel(color.x),
        clampChannel(color.y),
        clampChannel(color.z),
        clampChannel(color.w)};
}

const char* ReticleKindLabel(const ReticleKind kind) noexcept
{
    switch (kind)
    {
    case ReticleKind::Static:
        return "Static";

    case ReticleKind::Dynamic:
        return "Dynamic";

    case ReticleKind::Strobe:
        return "Strobe";
    }

    return "Unknown";
}

const PageDefinition* FindPage(const MfdDocument& document, std::string_view pageName)
{
    return FindPageDefinition(document, pageName);
}

ReticleKind ClassifyReticle(const PageDefinition& page, std::string_view reticleId)
{
    if (page.strobe.has_value() && page.strobe->reticle.id == reticleId)
    {
        return ReticleKind::Strobe;
    }

    const auto iterator = std::find_if(
        page.staticReticles.begin(),
        page.staticReticles.end(),
        [reticleId](const ReticleGroup& reticle)
        {
            return reticle.id == reticleId;
        });
    return iterator == page.staticReticles.end() ? ReticleKind::Dynamic : ReticleKind::Static;
}

const ReticleGroup* FindReticle(const SceneRegistry& scene, const ReticleKey& key)
{
    for (const ReticleGroup* reticle : scene.CollectPageReticlePointers(key.pageName))
    {
        if (reticle != nullptr && reticle->id == key.reticleId)
        {
            return reticle;
        }
    }

    return nullptr;
}

ColorRgba ResolveReticleColor(const ReticleGroup& reticle)
{
    if (reticle.overrides.color.has_value())
    {
        return *reticle.overrides.color;
    }

    return reticle.primitives.empty() ? ColorRgba {} : reticle.primitives.front().style.color;
}

float ResolveReticleThickness(const ReticleGroup& reticle)
{
    if (reticle.overrides.thickness.has_value())
    {
        return *reticle.overrides.thickness;
    }

    return reticle.primitives.empty() ? 0.0042f : reticle.primitives.front().style.thickness;
}

bool EditStringField(const char* label, std::string& value)
{
    const std::size_t bufferSize = std::max(kTextBufferBaseSize, value.size() + 32U);
    std::vector<char> buffer(bufferSize, '\0');
    std::copy(value.begin(), value.end(), buffer.begin());

    if (!ImGui::InputText(label, buffer.data(), buffer.size()))
    {
        return false;
    }

    value = buffer.data();
    return true;
}

void DrawPrimitiveGeometryReadOnly(const Primitive& primitive)
{
    if (const auto* geometry = std::get_if<TextGeometry>(&primitive.geometry))
    {
        ImGui::Text("Text: %s", geometry->text.c_str());
        ImGui::Text("Font size: %.4f", geometry->fontSize);
        ImGui::Text("Letter spacing: %.4f", geometry->letterSpacing);
        return;
    }

    if (const auto* geometry = std::get_if<TimeGeometry>(&primitive.geometry))
    {
        ImGui::Text("Format: %s", geometry->format.c_str());
        ImGui::Text("UTC: %s", geometry->utc ? "true" : "false");
        ImGui::Text("Font size: %.4f", geometry->fontSize);
        ImGui::Text("Letter spacing: %.4f", geometry->letterSpacing);
        return;
    }

    if (const auto* geometry = std::get_if<LineGeometry>(&primitive.geometry))
    {
        ImGui::Text("Start: (%.4f, %.4f)", geometry->start.x, geometry->start.y);
        ImGui::Text("End: (%.4f, %.4f)", geometry->end.x, geometry->end.y);
        return;
    }

    if (const auto* geometry = std::get_if<CircleGeometry>(&primitive.geometry))
    {
        ImGui::Text("Radius: %.4f", geometry->radius);
        return;
    }

    if (const auto* geometry = std::get_if<RingGeometry>(&primitive.geometry))
    {
        ImGui::Text("Inner radius: %.4f", geometry->innerRadius);
        ImGui::Text("Outer radius: %.4f", geometry->outerRadius);
        ImGui::Text("Segments: %d", geometry->segments);
        return;
    }

    if (const auto* geometry = std::get_if<RectangleGeometry>(&primitive.geometry))
    {
        ImGui::Text("Width: %.4f", geometry->width);
        ImGui::Text("Height: %.4f", geometry->height);
        return;
    }

    if (const auto* geometry = std::get_if<EllipseGeometry>(&primitive.geometry))
    {
        ImGui::Text("Width: %.4f", geometry->width);
        ImGui::Text("Height: %.4f", geometry->height);
        return;
    }

    if (const auto* geometry = std::get_if<SquareGeometry>(&primitive.geometry))
    {
        ImGui::Text("Width: %.4f", geometry->width);
        ImGui::Text("Height: %.4f", geometry->height);
        return;
    }

    if (const auto* geometry = std::get_if<DiamondGeometry>(&primitive.geometry))
    {
        ImGui::Text("Width: %.4f", geometry->width);
        ImGui::Text("Height: %.4f", geometry->height);
        return;
    }

    if (const auto* geometry = std::get_if<TriangleGeometry>(&primitive.geometry))
    {
        for (std::size_t index = 0; index < geometry->points.size(); ++index)
        {
            ImGui::Text("Point %d: (%.4f, %.4f)",
                        static_cast<int>(index),
                        geometry->points[index].x,
                        geometry->points[index].y);
        }
        return;
    }

    if (const auto* geometry = std::get_if<PolylineGeometry>(&primitive.geometry))
    {
        ImGui::Text("Points: %d", static_cast<int>(geometry->points.size()));
        ImGui::Text("Closed: %s", geometry->closed ? "true" : "false");
        return;
    }

    if (const auto* geometry = std::get_if<BezierGeometry>(&primitive.geometry))
    {
        ImGui::Text("Control points: %d", static_cast<int>(geometry->controlPoints.size()));
        ImGui::Text("Segments: %d", geometry->segments);
    }
}

void DrawPrimitiveTree(const ReticleGroup& reticle)
{
    if (reticle.primitives.empty())
    {
        ImGui::TextDisabled("No primitive is attached to this reticle.");
        return;
    }

    for (std::size_t index = 0; index < reticle.primitives.size(); ++index)
    {
        const Primitive& primitive = reticle.primitives[index];
        const std::string primitiveId =
            primitive.id.empty() ? "primitive_" + std::to_string(index) : primitive.id;
        const std::string label = primitiveId + "##primitive_" + std::to_string(index);

        if (ImGui::TreeNode(label.c_str()))
        {
            ImGui::Text("Type: %d", static_cast<int>(primitive.type));
            ImGui::Text("Visible: %s", primitive.style.visible ? "true" : "false");
            ImGui::Text("Position: (%.4f, %.4f)", primitive.transform.position.x, primitive.transform.position.y);
            ImGui::Text("Rotation: %.4f", primitive.transform.rotationDegrees);
            ImGui::Text("Scale: (%.4f, %.4f)", primitive.transform.scale.x, primitive.transform.scale.y);
            ImGui::Text("Color: (%u, %u, %u, %u)",
                        primitive.style.color.r,
                        primitive.style.color.g,
                        primitive.style.color.b,
                        primitive.style.color.a);
            ImGui::Text("Thickness: %.4f", primitive.style.thickness);
            DrawPrimitiveGeometryReadOnly(primitive);
            ImGui::TreePop();
        }
    }
}
} // namespace

void RuntimeDebugOverlay::Initialize()
{
    if (initialized_)
    {
        return;
    }

    rlImGuiSetup(true);
    ImGui::StyleColorsDark();
    initialized_ = true;
}

void RuntimeDebugOverlay::Shutdown() noexcept
{
    if (!initialized_)
    {
        return;
    }

    preview_.Invalidate();
    rlImGuiShutdown();
    initialized_ = false;
}

bool RuntimeDebugOverlay::HandleShortcut(const SceneRegistry& liveScene)
{
    if (!IsKeyPressed(KEY_F1))
    {
        return false;
    }

    if (state_.Active())
    {
        Deactivate();
    }
    else
    {
        (void)Activate(liveScene);
    }

    return true;
}

void RuntimeDebugOverlay::OnRuntimeReloaded(const SceneRegistry& liveScene)
{
    state_.ResetObservedRuntimeState();
    state_.ResetInteractiveState();

    if (state_.Active())
    {
        (void)RefreshPreviewFromLive(liveScene);
    }
    else
    {
        preview_.Invalidate();
    }
}

void RuntimeDebugOverlay::Synchronize(const SceneRegistry& liveScene,
                                      const UdpRuntimeBridge* const bridge,
                                      const std::string_view commandStatus,
                                      const std::string_view feedbackStatus,
                                      const std::vector<CommandBatch>& drainedBatches)
{
    const bool commandConfigured = bridge != nullptr && bridge->HasCommandReceiver();
    const bool commandReady = bridge != nullptr && bridge->CommandTransportReady();
    const bool feedbackConfigured = bridge != nullptr && bridge->HasFeedbackSender();
    const bool feedbackReady = bridge != nullptr && bridge->FeedbackTransportReady();

    state_.UpdateTransportState(
        commandConfigured,
        commandReady,
        feedbackConfigured,
        feedbackReady,
        std::string(commandStatus),
        std::string(feedbackStatus));

    std::size_t commandCount = 0;
    for (const CommandBatch& batch : drainedBatches)
    {
        commandCount += batch.commands.size();
    }

    state_.NoteCommandTraffic(drainedBatches.size(), commandCount);
    RecordObservedRuntimeState(drainedBatches);

    if (!state_.Active())
    {
        return;
    }

    if (!preview_.Ready())
    {
        (void)RefreshPreviewFromLive(liveScene);
        return;
    }

    if (!preview_.ApplyLiveBatches(drainedBatches, state_))
    {
        state_.SetTestPanelStatus(preview_.LastError());
    }
}

bool RuntimeDebugOverlay::Active() const noexcept
{
    return state_.Active();
}

bool RuntimeDebugOverlay::SuppressStartupSplash() const noexcept
{
    return state_.Active();
}

bool RuntimeDebugOverlay::ConsumeRuntimeShortcuts() const noexcept
{
    return state_.Active();
}

int RuntimeDebugOverlay::PreferredPanelWidth() const noexcept
{
    return kDebugPanelWidth;
}

const SceneRegistry& RuntimeDebugOverlay::RenderScene(const SceneRegistry& liveScene) const noexcept
{
    return state_.Active() && preview_.Ready() ? preview_.Scene() : liveScene;
}

void RuntimeDebugOverlay::Draw(const SceneRegistry& liveScene,
                               const WindowAssetDefinition& windowDefinition,
                               const std::string_view applicationName,
                               const std::string_view runtimeError)
{
    if (!state_.Active() || !initialized_)
    {
        return;
    }

    const SceneRegistry& displayScene = preview_.Ready() ? preview_.Scene() : liveScene;
    const auto pages = displayScene.Pages();

    auto selectFirstReticle =
        [&](std::string* statusOut) -> bool
    {
        const std::string activePage = displayScene.ActivePageName();
        const PageDefinition* page = FindPage(displayScene.Document(), activePage);
        if (page == nullptr)
        {
            if (statusOut != nullptr)
            {
                *statusOut = "No active page is available for the manual test panel.";
            }
            return false;
        }

        const auto reticleViews = displayScene.CollectPageReticleViews(page->name);
        const auto reticlePointers = displayScene.CollectPageReticlePointers(page->name);
        if (reticleViews.empty() || reticlePointers.empty())
        {
            if (statusOut != nullptr)
            {
                *statusOut = "The active page does not expose any reticle.";
            }
            return false;
        }

        const ReticleGroup* firstReticle = reticlePointers.front();
        if (firstReticle == nullptr)
        {
            if (statusOut != nullptr)
            {
                *statusOut = "The first reticle entry is invalid.";
            }
            return false;
        }

        state_.SelectReticle(ReticleKey {page->name, firstReticle->id, ClassifyReticle(*page, firstReticle->id)});
        if (statusOut != nullptr)
        {
            *statusOut = "Selected reticle '" + firstReticle->id + "' on page '" + page->name + "'.";
        }
        return true;
    };

    auto findSelectedReticle =
        [&]() -> const ReticleGroup*
    {
        if (!state_.SelectedReticle().has_value())
        {
            return nullptr;
        }

        if (const ReticleBypassState* bypass = state_.FindReticleBypass(*state_.SelectedReticle()); bypass != nullptr)
        {
            return &bypass->draft;
        }

        return FindReticle(displayScene, *state_.SelectedReticle());
    };

    auto mutateSelectedReticle =
        [&](auto&& mutation, const char* const successMessage) -> bool
    {
        if (!state_.SelectedReticle().has_value())
        {
            state_.SetTestPanelStatus("Select one reticle before using this action.");
            return false;
        }

        const ReticleKey key = *state_.SelectedReticle();
        const ReticleGroup* currentReticle = findSelectedReticle();
        if (currentReticle == nullptr)
        {
            state_.SetTestPanelStatus("The selected reticle is no longer available in the preview.");
            return false;
        }

        ReticleBypassState& bypass = state_.EnsureReticleBypass(key, *currentReticle);
        mutation(bypass.draft);
        if (!RefreshPreviewFromLive(liveScene))
        {
            state_.SetTestPanelStatus(preview_.LastError());
            return false;
        }

        state_.SetTestPanelStatus(successMessage);
        return true;
    };

    rlImGuiBegin();

    bool open = true;
    const float panelWidth = static_cast<float>(std::min(std::max(PreferredPanelWidth(), 320), std::max(GetScreenWidth(), 1)));
    const float panelHeight = static_cast<float>(std::max(GetScreenHeight(), 1));
    const float panelX = std::max(0.0f, static_cast<float>(GetScreenWidth()) - panelWidth);
    const float treePaneWidth = std::min(kLeftPaneWidth, std::max(220.0f, panelWidth * 0.42f));

    ImGui::SetNextWindowPos(ImVec2(panelX, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelWidth, panelHeight), ImGuiCond_Always);
    ImGui::Begin(
        "mfd_window Debug",
        &open,
        ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize);

    ImGui::TextColored(ImVec4(0.33f, 0.86f, 0.78f, 1.00f), "%s runtime debug", std::string(applicationName).c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("| %s", windowDefinition.title.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("| F1 toggles this overlay");

    if (!preview_.LastError().empty())
    {
        ImGui::TextColored(ImVec4(1.00f, 0.45f, 0.45f, 1.00f), "Preview warning: %s", preview_.LastError().c_str());
    }
    if (!runtimeError.empty())
    {
        ImGui::TextColored(ImVec4(1.00f, 0.45f, 0.45f, 1.00f), "Runtime warning: %s", std::string(runtimeError).c_str());
    }

    const TransportState& transport = state_.Transport();
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.72f, 0.86f, 0.95f, 1.00f), "Transport");

    bool commandConfigured = transport.commandConfigured;
    bool commandReady = transport.commandReady;
    bool feedbackConfigured = transport.feedbackConfigured;
    bool feedbackReady = transport.feedbackReady;
    bool trafficSeen = transport.observedCommandTraffic;

    ImGui::BeginDisabled(true);
    ImGui::Checkbox("UDP command configured", &commandConfigured);
    ImGui::SameLine();
    ImGui::Checkbox("UDP command ready", &commandReady);
    ImGui::SameLine();
    ImGui::Checkbox("UDP traffic observed", &trafficSeen);
    ImGui::Checkbox("UDP feedback configured", &feedbackConfigured);
    ImGui::SameLine();
    ImGui::Checkbox("UDP feedback ready", &feedbackReady);
    ImGui::EndDisabled();

    const double secondsSinceTraffic = state_.SecondsSinceLastCommandTraffic();
    if (secondsSinceTraffic >= 0.0)
    {
        ImGui::Text("Last command traffic: %.2f s ago", secondsSinceTraffic);
    }
    else
    {
        ImGui::TextDisabled("No command traffic has been observed yet.");
    }
    ImGui::TextWrapped("Command status: %s", transport.commandStatus.c_str());
    ImGui::TextWrapped("Feedback status: %s", transport.feedbackStatus.c_str());

    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.72f, 0.86f, 0.95f, 1.00f), "Page override");

    std::string comboPreview = state_.PageBypassed()
                                   ? state_.ForcedActivePage()
                                   : displayScene.ActivePageName();
    if (comboPreview.empty() && !pages.empty())
    {
        comboPreview = pages.front().name;
    }

    bool pageBypassed = state_.PageBypassed();
    if (ImGui::Checkbox("Bypass active page", &pageBypassed))
    {
        if (pageBypassed)
        {
            state_.EnablePageBypass(comboPreview);
        }
        else
        {
            state_.DisablePageBypass();
        }

        (void)RefreshPreviewFromLive(liveScene);
    }

    ImGui::SameLine();
    if (ImGui::BeginCombo("Preview page", comboPreview.c_str()))
    {
        for (const PageSummary& page : pages)
        {
            const bool selected = comboPreview == page.name;
            if (ImGui::Selectable(page.name.c_str(), selected))
            {
                state_.EnablePageBypass(page.name);
                (void)RefreshPreviewFromLive(liveScene);
            }

            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::Text("Live active page: %s", liveScene.ActivePageName().c_str());
    ImGui::Text("Preview active page: %s", displayScene.ActivePageName().c_str());

    ImGui::BeginChild("DebugBody", ImVec2(0.0f, -kBottomPanelHeight), false);
    ImGui::BeginChild("ReticleTreePane", ImVec2(treePaneWidth, 0.0f), true);
    ImGui::TextColored(ImVec4(0.72f, 0.86f, 0.95f, 1.00f), "Pages and reticles");
    ImGui::TextDisabled("Select one reticle to inspect or bypass it locally.");
    ImGui::Spacing();

    for (const PageSummary& page : pages)
    {
        const PageDefinition* pageDefinition = FindPage(displayScene.Document(), page.name);
        if (pageDefinition == nullptr)
        {
            continue;
        }

        const bool pageSelected = state_.SelectedPage() == page.name;
        ImGuiTreeNodeFlags pageFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
        if (pageSelected)
        {
            pageFlags |= ImGuiTreeNodeFlags_Selected;
        }

        std::string pageLabel = page.name;
        if (page.active)
        {
            pageLabel.append(" [active]");
        }
        if (state_.PageBypassed() && state_.ForcedActivePage() == page.name)
        {
            pageLabel.append(" [bypassed]");
        }
        pageLabel.append("##page_tree_").append(page.name);

        const bool pageOpened = ImGui::TreeNodeEx(pageLabel.c_str(), pageFlags);
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        {
            state_.SelectPage(page.name);
        }

        if (!pageOpened)
        {
            continue;
        }

        const auto reticleViews = displayScene.CollectPageReticleViews(page.name);
        const auto reticlePointers = displayScene.CollectPageReticlePointers(page.name);
        const std::size_t reticleCount = std::min(reticleViews.size(), reticlePointers.size());

        for (std::size_t index = 0; index < reticleCount; ++index)
        {
            const ReticleGroup* reticle = reticlePointers[index];
            if (reticle == nullptr)
            {
                continue;
            }

            ReticleUiEntry entry;
            entry.key = ReticleKey {page.name, reticle->id, ClassifyReticle(*pageDefinition, reticle->id)};
            entry.effectiveVisible = reticleViews[index].visible;
            entry.label = std::string("[") + ReticleKindLabel(entry.key.kind) + "] " +
                          (reticle->id.empty() ? std::string {"<unnamed>"} : reticle->id);
            if (state_.ReticleBypassed(entry.key))
            {
                entry.label.append(" [bypassed]");
            }
            if (!entry.effectiveVisible)
            {
                entry.label.append(" [hidden]");
            }
            entry.label.append("##reticle_tree_").append(page.name).append("_").append(reticle->id);

            const bool reticleSelected =
                state_.SelectedReticle().has_value() && *state_.SelectedReticle() == entry.key;
            ImGuiTreeNodeFlags reticleFlags =
                ImGuiTreeNodeFlags_Leaf |
                ImGuiTreeNodeFlags_NoTreePushOnOpen |
                ImGuiTreeNodeFlags_SpanAvailWidth;
            if (reticleSelected)
            {
                reticleFlags |= ImGuiTreeNodeFlags_Selected;
            }

            ImGui::TreeNodeEx(entry.label.c_str(), reticleFlags);
            if (ImGui::IsItemClicked())
            {
                state_.SelectReticle(entry.key);
            }
        }

        ImGui::TreePop();
    }
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("ReticleInspectorPane", ImVec2(0.0f, 0.0f), true);
    ImGui::TextColored(ImVec4(0.72f, 0.86f, 0.95f, 1.00f), "Inspector");
    if (!state_.SelectedReticle().has_value())
    {
        ImGui::TextDisabled("Select one reticle in the tree to inspect it.");
    }
    else
    {
        const ReticleKey selectedKey = *state_.SelectedReticle();
        const PageDefinition* page = FindPage(displayScene.Document(), selectedKey.pageName);
        const ReticleGroup* inspectedReticle = findSelectedReticle();
        if (page == nullptr || inspectedReticle == nullptr)
        {
            ImGui::TextColored(ImVec4(1.00f, 0.45f, 0.45f, 1.00f), "The selected reticle is no longer available.");
        }
        else
        {
            const ReticleBypassState* bypass = state_.FindReticleBypass(selectedKey);
            bool bypassed = bypass != nullptr;
            RuntimeDebugInspectorFrameState inspectorFrameState;
            ImGui::Text("Page: %s", selectedKey.pageName.c_str());
            ImGui::Text("Reticle: %s", selectedKey.reticleId.c_str());
            ImGui::Text("Kind: %s", ReticleKindLabel(selectedKey.kind));
            if (!inspectedReticle->sourceTemplateId.empty())
            {
                ImGui::Text("Template: %s", inspectedReticle->sourceTemplateId.c_str());
            }

            if (ImGui::Checkbox("Bypassed", &bypassed))
            {
                if (bypassed)
                {
                    state_.EnsureReticleBypass(selectedKey, *inspectedReticle);
                    state_.SetTestPanelStatus(
                        "Reticle '" + selectedKey.reticleId + "' is now owned by the debug overlay.");
                }
                else
                {
                    state_.ReleaseReticleBypass(selectedKey);
                    state_.SetTestPanelStatus(
                        "Reticle '" + selectedKey.reticleId + "' now follows the last UDP state again.");
                }

                (void)RefreshPreviewFromLive(liveScene);
                inspectorFrameState.InvalidateSnapshot();
            }

            if (inspectorFrameState.SnapshotValid())
            {
                ImGui::Separator();

                bool visible = inspectedReticle->visible;
                if (ImGui::Checkbox("Visible", &visible))
                {
                    (void)mutateSelectedReticle(
                        [&](ReticleGroup& draft)
                        {
                            draft.visible = visible;
                        },
                        "Updated reticle visibility through the manual debug bypass.");
                    inspectorFrameState.InvalidateSnapshot();
                }
            }

            if (inspectorFrameState.SnapshotValid())
            {
                bool blinkEnabled = inspectedReticle->blink.enabled;
                if (ImGui::Checkbox("Blink enabled", &blinkEnabled))
                {
                    (void)mutateSelectedReticle(
                        [&](ReticleGroup& draft)
                        {
                            draft.blink.enabled = blinkEnabled;
                        },
                        "Updated reticle blink state.");
                    inspectorFrameState.InvalidateSnapshot();
                }
            }

            if (inspectorFrameState.SnapshotValid())
            {
                std::string blinkTypeName = inspectedReticle->blink.typeName;
                if (ImGui::BeginCombo("Blink type", blinkTypeName.empty() ? "<page default>" : blinkTypeName.c_str()))
                {
                    const bool noTypeSelected = blinkTypeName.empty();
                    if (ImGui::Selectable("<page default>", noTypeSelected))
                    {
                        (void)mutateSelectedReticle(
                            [&](ReticleGroup& draft)
                            {
                                draft.blink.typeName.clear();
                                draft.blink.normalizedTypeName.clear();
                                draft.blink.durationMs = 0;
                            },
                            "Cleared the reticle-specific blink type.");
                        inspectorFrameState.InvalidateSnapshot();
                    }
                    if (inspectorFrameState.SnapshotValid() && noTypeSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }

                    for (const PageBlinkDefinition& blinkType : page->blinkTypes)
                    {
                        if (!inspectorFrameState.SnapshotValid())
                        {
                            break;
                        }

                        const bool selected = blinkTypeName == blinkType.name;
                        if (ImGui::Selectable(blinkType.name.c_str(), selected))
                        {
                            (void)mutateSelectedReticle(
                                [&](ReticleGroup& draft)
                                {
                                    draft.blink.enabled = true;
                                    draft.blink.typeName = blinkType.name;
                                    draft.blink.normalizedTypeName = blinkType.normalizedName;
                                    draft.blink.durationMs = blinkType.durationMs;
                                },
                                "Updated the reticle-specific blink type.");
                            inspectorFrameState.InvalidateSnapshot();
                            break;
                        }
                        if (selected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
            }

            if (inspectorFrameState.SnapshotValid())
            {
                std::array<float, 2> position {
                    inspectedReticle->transform.position.x,
                    inspectedReticle->transform.position.y};
                if (ImGui::DragFloat2("Position", position.data(), 0.005f, -1.5f, 1.5f, "%.4f"))
                {
                    (void)mutateSelectedReticle(
                        [&](ReticleGroup& draft)
                        {
                            draft.transform.position = Vec2 {position[0], position[1]};
                        },
                        "Updated the reticle position.");
                    inspectorFrameState.InvalidateSnapshot();
                }
            }

            if (inspectorFrameState.SnapshotValid())
            {
                float rotation = inspectedReticle->transform.rotationDegrees;
                if (ImGui::DragFloat("Rotation", &rotation, 0.25f, -360.0f, 360.0f, "%.3f"))
                {
                    (void)mutateSelectedReticle(
                        [&](ReticleGroup& draft)
                        {
                            draft.transform.rotationDegrees = rotation;
                        },
                        "Updated the reticle rotation.");
                    inspectorFrameState.InvalidateSnapshot();
                }
            }

            if (inspectorFrameState.SnapshotValid())
            {
                bool colorOverride = inspectedReticle->overrides.color.has_value();
                ImVec4 color = ToImGuiColor(ResolveReticleColor(*inspectedReticle));
                if (ImGui::Checkbox("Color override", &colorOverride))
                {
                    (void)mutateSelectedReticle(
                        [&](ReticleGroup& draft)
                        {
                            if (colorOverride)
                            {
                                draft.overrides.color = FromImGuiColor(color);
                            }
                            else
                            {
                                draft.overrides.color.reset();
                            }
                        },
                        colorOverride ? "Enabled one local color override." : "Removed the local color override.");
                    inspectorFrameState.InvalidateSnapshot();
                }
                if (inspectorFrameState.SnapshotValid() && colorOverride && ImGui::ColorEdit4("Color", &color.x))
                {
                    (void)mutateSelectedReticle(
                        [&](ReticleGroup& draft)
                        {
                            draft.overrides.color = FromImGuiColor(color);
                        },
                        "Updated the local reticle color override.");
                    inspectorFrameState.InvalidateSnapshot();
                }
            }

            if (inspectorFrameState.SnapshotValid())
            {
                bool thicknessOverride = inspectedReticle->overrides.thickness.has_value();
                float thickness = ResolveReticleThickness(*inspectedReticle);
                if (ImGui::Checkbox("Thickness override", &thicknessOverride))
                {
                    (void)mutateSelectedReticle(
                        [&](ReticleGroup& draft)
                        {
                            if (thicknessOverride)
                            {
                                draft.overrides.thickness = thickness;
                            }
                            else
                            {
                                draft.overrides.thickness.reset();
                            }
                        },
                        thicknessOverride ? "Enabled one local thickness override."
                                          : "Removed the local thickness override.");
                    inspectorFrameState.InvalidateSnapshot();
                }
                if (inspectorFrameState.SnapshotValid() &&
                    thicknessOverride &&
                    ImGui::DragFloat("Thickness", &thickness, 0.0005f, 0.0001f, 0.05f, "%.4f"))
                {
                    (void)mutateSelectedReticle(
                        [&](ReticleGroup& draft)
                        {
                            draft.overrides.thickness = thickness;
                        },
                        "Updated the local reticle thickness override.");
                    inspectorFrameState.InvalidateSnapshot();
                }
            }

            if (inspectorFrameState.SnapshotValid())
            {
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.72f, 0.86f, 0.95f, 1.00f), "Text primitives");
                bool anyEditableText = false;
                for (std::size_t primitiveIndex = 0; primitiveIndex < inspectedReticle->primitives.size(); ++primitiveIndex)
                {
                    const Primitive& primitive = inspectedReticle->primitives[primitiveIndex];
                    if (auto* geometry = std::get_if<TextGeometry>(&primitive.geometry); geometry != nullptr)
                    {
                        anyEditableText = true;
                        std::string text = geometry->text;
                        const std::string textLabel =
                            "Text##" + primitive.id + "_" + std::to_string(primitiveIndex);
                        if (EditStringField(textLabel.c_str(), text))
                        {
                            (void)mutateSelectedReticle(
                                [&](ReticleGroup& draft)
                                {
                                    if (Primitive* editable = FindPrimitive(draft, primitive.id); editable != nullptr)
                                    {
                                        if (TextGeometry* editableText = std::get_if<TextGeometry>(&editable->geometry);
                                            editableText != nullptr)
                                        {
                                            editableText->text = text;
                                        }
                                    }
                                },
                                "Updated one text primitive.");
                            inspectorFrameState.InvalidateSnapshot();
                            break;
                        }

                        float letterSpacing = geometry->letterSpacing;
                        const std::string spacingLabel =
                            "Letter spacing##" + primitive.id + "_" + std::to_string(primitiveIndex);
                        if (ImGui::DragFloat(spacingLabel.c_str(), &letterSpacing, 0.0005f, 0.0f, 0.2f, "%.4f"))
                        {
                            (void)mutateSelectedReticle(
                                [&](ReticleGroup& draft)
                                {
                                    if (Primitive* editable = FindPrimitive(draft, primitive.id); editable != nullptr)
                                    {
                                        if (TextGeometry* editableText = std::get_if<TextGeometry>(&editable->geometry);
                                            editableText != nullptr)
                                        {
                                            editableText->letterSpacing = letterSpacing;
                                        }
                                    }
                                },
                                "Updated one text primitive letter spacing.");
                            inspectorFrameState.InvalidateSnapshot();
                            break;
                        }
                    }
                }
                if (inspectorFrameState.SnapshotValid() && !anyEditableText)
                {
                    ImGui::TextDisabled("No editable text primitive is attached to this reticle.");
                }
            }

            if (inspectorFrameState.SnapshotValid())
            {
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.72f, 0.86f, 0.95f, 1.00f), "Primitive tree");
                DrawPrimitiveTree(*inspectedReticle);
            }
            else
            {
                ImGui::Separator();
                ImGui::TextDisabled("%s", RuntimeDebugInspectorFrameState::RefreshNotice().data());
            }
        }
    }
    ImGui::EndChild();
    ImGui::EndChild();

    ImGui::BeginChild("DebugTestPanel", ImVec2(0.0f, 0.0f), true);
    ImGui::TextColored(ImVec4(0.72f, 0.86f, 0.95f, 1.00f), "Manual test panel");
    ImGui::TextDisabled("Use these actions to validate the runtime debug feature without an external client.");

    if (ImGui::Button("Resync preview from live UDP state"))
    {
        if (RefreshPreviewFromLive(liveScene))
        {
            state_.SetTestPanelStatus("Preview scene synchronized from the live UDP state.");
        }
        else
        {
            state_.SetTestPanelStatus(preview_.LastError());
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Release all bypasses"))
    {
        state_.ReleaseAllReticleBypasses();
        state_.DisablePageBypass();
        if (RefreshPreviewFromLive(liveScene))
        {
            state_.SetTestPanelStatus("Released every local bypass and restored the live UDP scene.");
        }
        else
        {
            state_.SetTestPanelStatus(preview_.LastError());
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Select first reticle on active page"))
    {
        std::string status;
        if (!selectFirstReticle(&status))
        {
            state_.SetTestPanelStatus(status);
        }
        else
        {
            state_.SetTestPanelStatus(status);
        }
    }

    if (ImGui::Button("Toggle selected reticle visibility"))
    {
        const ReticleGroup* reticle = findSelectedReticle();
        if (reticle == nullptr)
        {
            state_.SetTestPanelStatus("Select one reticle before toggling its visibility.");
        }
        else
        {
            (void)mutateSelectedReticle(
                [&](ReticleGroup& draft)
                {
                    draft.visible = !draft.visible;
                },
                "Toggled the selected reticle visibility.");
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Nudge selected reticle +X"))
    {
        const ReticleGroup* reticle = findSelectedReticle();
        if (reticle == nullptr)
        {
            state_.SetTestPanelStatus("Select one reticle before nudging it.");
        }
        else
        {
            (void)mutateSelectedReticle(
                [&](ReticleGroup& draft)
                {
                    draft.transform.position.x += 0.05f;
                },
                "Moved the selected reticle by +0.05 on the X axis.");
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Force preview to active live page"))
    {
        state_.EnablePageBypass(liveScene.ActivePageName());
        if (RefreshPreviewFromLive(liveScene))
        {
            state_.SetTestPanelStatus("Forced the preview to the current live active page.");
        }
        else
        {
            state_.SetTestPanelStatus(preview_.LastError());
        }
    }

    if (!state_.TestPanelStatus().empty())
    {
        ImGui::Spacing();
        ImGui::TextWrapped("%s", state_.TestPanelStatus().c_str());
    }
    ImGui::EndChild();

    ImGui::End();
    rlImGuiEnd();

    if (!open)
    {
        Deactivate();
    }
}

bool RuntimeDebugOverlay::Activate(const SceneRegistry& liveScene)
{
    state_.Activate();
    state_.ResetInteractiveState();

    if (!RefreshPreviewFromLive(liveScene))
    {
        Deactivate();
        return false;
    }

    state_.SetTestPanelStatus("Runtime debug mode enabled. All local bypasses start from the current live UDP state.");
    return true;
}

void RuntimeDebugOverlay::Deactivate()
{
    state_.Deactivate();
    preview_.Invalidate();
}

bool RuntimeDebugOverlay::RefreshPreviewFromLive(const SceneRegistry& liveScene)
{
    if (!state_.Active())
    {
        return true;
    }

    return preview_.ResetFromLive(liveScene, state_);
}

void RuntimeDebugOverlay::RecordObservedRuntimeState(const std::vector<CommandBatch>& drainedBatches)
{
    for (const CommandBatch& batch : drainedBatches)
    {
        for (const UserCommand& command : batch.commands)
        {
            std::visit(
                [this](const auto& value)
                {
                    using Command = std::decay_t<decltype(value)>;

                    if constexpr (std::is_same_v<Command, SetDynamicReticleSetVisibilityCommand>)
                    {
                        state_.SetDynamicTemplateVisibility(value.page, value.templateId, value.visible);
                    }
                    else if constexpr (std::is_same_v<Command, ResetWindowCommand>)
                    {
                        state_.ClearDynamicTemplateVisibility();
                    }
                },
                command);
        }
    }
}
} // namespace mfd::window::debug
