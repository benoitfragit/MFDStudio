/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation of the integrated runtime debug overlay.
 */

#include "mfd/window/debug/RuntimeDebugOverlay.hpp"

#include "mfd/window/debug/RuntimeDebugInspectorFrameState.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <cmath>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
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
constexpr float kReticleNudgeStep = 0.05f;
constexpr std::size_t kTextBufferBaseSize = 256U;
constexpr auto kTransportMetricsRefreshInterval = std::chrono::milliseconds(250);

struct ReticleUiEntry
{
    ReticleKey key;
    std::string label;
    bool effectiveVisible = false;
};

std::uint8_t ClampUnitFloatToByte(const float value) noexcept
{
    const float clamped = std::clamp(value, 0.0f, 1.0f);
    return static_cast<std::uint8_t>(std::lround(clamped * 255.0f));
}

void DrawDisabledText(const std::string_view text)
{
    ImGui::TextDisabled("%.*s", static_cast<int>(text.size()), text.data());
}

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
    return ColorRgba {
        ClampUnitFloatToByte(color.x),
        ClampUnitFloatToByte(color.y),
        ClampUnitFloatToByte(color.z),
        ClampUnitFloatToByte(color.w)};
}

bool IsLeapYearForTimeUi(const int year) noexcept
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int DaysInMonthForTimeUi(const int year, const int month) noexcept
{
    switch (month)
    {
    case 2:
        return IsLeapYearForTimeUi(year) ? 29 : 28;
    case 4:
    case 6:
    case 9:
    case 11:
        return 30;
    default:
        return 31;
    }
}

bool HasVisibleTimeFieldForUi(const TimeFieldVisibility& fields) noexcept
{
    return fields.year || fields.month || fields.day || fields.hour || fields.minute || fields.second;
}

TimeValue ClampTimeValueForUi(TimeValue value) noexcept
{
    value.year = std::clamp(value.year, 1, 9999);
    value.month = std::clamp(value.month, 1, 12);
    value.day = std::clamp(value.day, 1, DaysInMonthForTimeUi(value.year, value.month));
    value.hour = std::clamp(value.hour, 0, 23);
    value.minute = std::clamp(value.minute, 0, 59);
    value.second = std::clamp(value.second, 0, 59);
    return value;
}

TimeValue CurrentTimeValueForUi(const bool utc) noexcept
{
    const std::time_t now = std::time(nullptr);
    std::tm calendarTime {};
#if defined(_WIN32)
    if (utc)
    {
        gmtime_s(&calendarTime, &now);
    }
    else
    {
        localtime_s(&calendarTime, &now);
    }
#else
    if (utc)
    {
        gmtime_r(&now, &calendarTime);
    }
    else
    {
        localtime_r(&now, &calendarTime);
    }
#endif

    TimeValue value;
    value.year = calendarTime.tm_year + 1900;
    value.month = calendarTime.tm_mon + 1;
    value.day = calendarTime.tm_mday;
    value.hour = calendarTime.tm_hour;
    value.minute = calendarTime.tm_min;
    value.second = calendarTime.tm_sec;
    return ClampTimeValueForUi(value);
}

TimeGeometry* FindEditableTimeGeometry(ReticleGroup& reticle, const std::string_view primitiveId) noexcept
{
    Primitive* editable = FindPrimitive(reticle, primitiveId);
    if (editable == nullptr)
    {
        return nullptr;
    }

    return std::get_if<TimeGeometry>(&editable->geometry);
}

TextGeometry* FindEditableTextGeometry(ReticleGroup& reticle, const std::string_view primitiveId) noexcept
{
    Primitive* editable = FindPrimitive(reticle, primitiveId);
    if (editable == nullptr)
    {
        return nullptr;
    }

    return std::get_if<TextGeometry>(&editable->geometry);
}

void SetReticleVisible(ReticleGroup& reticle, const bool visible) noexcept
{
    reticle.visible = visible;
}

void NudgeReticlePosition(ReticleGroup& reticle, const Vec2& delta) noexcept
{
    reticle.transform.position.x += delta.x;
    reticle.transform.position.y += delta.y;
}

void SetReticleBlinkEnabled(ReticleGroup& reticle, const bool blinkEnabled) noexcept
{
    reticle.blink.enabled = blinkEnabled;
}

void ClearReticleBlinkType(ReticleGroup& reticle)
{
    reticle.blink.typeName.clear();
    reticle.blink.normalizedTypeName.clear();
    reticle.blink.durationMs = 0;
}

void SetReticleBlinkType(ReticleGroup& reticle, const PageBlinkDefinition& blinkType)
{
    reticle.blink.enabled = true;
    reticle.blink.typeName = blinkType.name;
    reticle.blink.normalizedTypeName = blinkType.normalizedName;
    reticle.blink.durationMs = blinkType.durationMs;
}

void SetReticlePosition(ReticleGroup& reticle, const Vec2& position) noexcept
{
    reticle.transform.position = position;
}

void SetReticleRotation(ReticleGroup& reticle, const float rotationDegrees) noexcept
{
    reticle.transform.rotationDegrees = rotationDegrees;
}

void SetReticleColorOverrideEnabled(ReticleGroup& reticle,
                                    const bool enabled,
                                    const ColorRgba& color)
{
    if (enabled)
    {
        reticle.overrides.color = color;
    }
    else
    {
        reticle.overrides.color.reset();
    }
}

void SetReticleColorOverrideValue(ReticleGroup& reticle, const ColorRgba& color)
{
    reticle.overrides.color = color;
}

void SetReticleThicknessOverrideEnabled(ReticleGroup& reticle,
                                        const bool enabled,
                                        const float thickness) noexcept
{
    if (enabled)
    {
        reticle.overrides.thickness = thickness;
    }
    else
    {
        reticle.overrides.thickness.reset();
    }
}

void SetReticleThicknessOverrideValue(ReticleGroup& reticle, const float thickness) noexcept
{
    reticle.overrides.thickness = thickness;
}

void SetTextPrimitiveText(ReticleGroup& reticle,
                          const std::string_view primitiveId,
                          const std::string_view text)
{
    if (TextGeometry* editableText = FindEditableTextGeometry(reticle, primitiveId); editableText != nullptr)
    {
        editableText->text = text;
    }
}

void SetTextPrimitiveLetterSpacing(ReticleGroup& reticle,
                                   const std::string_view primitiveId,
                                   const float letterSpacing) noexcept
{
    if (TextGeometry* editableText = FindEditableTextGeometry(reticle, primitiveId); editableText != nullptr)
    {
        editableText->letterSpacing = letterSpacing;
    }
}

void SetTimePrimitiveValueBypassEnabled(ReticleGroup& reticle,
                                        const std::string_view primitiveId,
                                        const bool enabled,
                                        const TimeValue& seedValue)
{
    if (TimeGeometry* editableTime = FindEditableTimeGeometry(reticle, primitiveId); editableTime != nullptr)
    {
        if (enabled)
        {
            editableTime->runtimeValueOverride = seedValue;
        }
        else
        {
            editableTime->runtimeValueOverride.reset();
        }
    }
}

void SetTimePrimitiveOverrideValue(ReticleGroup& reticle,
                                   const std::string_view primitiveId,
                                   const TimeValue& value)
{
    if (TimeGeometry* editableTime = FindEditableTimeGeometry(reticle, primitiveId); editableTime != nullptr)
    {
        editableTime->runtimeValueOverride = value;
    }
}

void SetTimePrimitiveUtc(ReticleGroup& reticle,
                         const std::string_view primitiveId,
                         const bool utc) noexcept
{
    if (TimeGeometry* editableTime = FindEditableTimeGeometry(reticle, primitiveId); editableTime != nullptr)
    {
        editableTime->runtimeUtc = utc;
    }
}

void SetTimePrimitiveFields(ReticleGroup& reticle,
                            const std::string_view primitiveId,
                            const TimeFieldVisibility& fields)
{
    if (TimeGeometry* editableTime = FindEditableTimeGeometry(reticle, primitiveId); editableTime != nullptr)
    {
        editableTime->runtimeFields = fields;
    }
}

void SetTimePrimitiveLetterSpacing(ReticleGroup& reticle,
                                   const std::string_view primitiveId,
                                   const float letterSpacing) noexcept
{
    if (TimeGeometry* editableTime = FindEditableTimeGeometry(reticle, primitiveId); editableTime != nullptr)
    {
        editableTime->letterSpacing = letterSpacing;
    }
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

const PageBlinkDefinition* ResolvePageDefaultBlink(const PageDefinition& page)
{
    if (page.defaultBlinkTypeName.empty())
    {
        return nullptr;
    }

    return FindPageBlinkDefinition(page, page.defaultBlinkTypeName);
}

bool CanResolveBlinkState(const PageDefinition& page, const ReticleBlinkState& blink)
{
    if (!blink.typeName.empty())
    {
        return FindPageBlinkDefinition(page, blink.typeName) != nullptr;
    }

    if (!blink.enabled)
    {
        return true;
    }

    return ResolvePageDefaultBlink(page) != nullptr;
}

std::string BuildBlinkTypePreviewLabel(const PageDefinition& page, const ReticleBlinkState& blink)
{
    if (!blink.typeName.empty())
    {
        return blink.typeName;
    }

    return ResolvePageDefaultBlink(page) != nullptr ? std::string {"<page default>"} : std::string {"<no page default>"};
}

ReticleKind ClassifyReticle(const PageDefinition& page, std::string_view reticleId)
{
    if (std::any_of(
            page.strobes.begin(),
            page.strobes.end(),
            [reticleId](const PageStrobeDefinition& strobe)
            {
                return strobe.reticle.id == reticleId;
            }))
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
        ImGui::Text(
            "UTC: %s%s",
            geometry->runtimeUtc.value_or(geometry->utc) ? "true" : "false",
            geometry->runtimeUtc.has_value() ? " (runtime)" : "");
        if (geometry->runtimeValueOverride.has_value())
        {
            const TimeValue& value = *geometry->runtimeValueOverride;
            ImGui::Text(
                "Runtime time: %04d-%02d-%02d %02d:%02d:%02d",
                value.year,
                value.month,
                value.day,
                value.hour,
                value.minute,
                value.second);
        }
        if (geometry->runtimeFields.has_value())
        {
            const TimeFieldVisibility& fields = *geometry->runtimeFields;
            ImGui::Text(
                "Runtime fields: Y=%d M=%d D=%d h=%d m=%d s=%d",
                fields.year ? 1 : 0,
                fields.month ? 1 : 0,
                fields.day ? 1 : 0,
                fields.hour ? 1 : 0,
                fields.minute ? 1 : 0,
                fields.second ? 1 : 0);
        }
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

void DrawTransportMetrics(const UdpRuntimeBridgeMetrics& metrics)
{
    ImGui::Text(
        "Queues: inbound %llu batch(es), outbound %llu feedback payload(s)",
        static_cast<unsigned long long>(metrics.inboundQueueDepth),
        static_cast<unsigned long long>(metrics.outboundQueueDepth));
    ImGui::Text(
        "Received: %llu packet(s), %llu byte(s)",
        static_cast<unsigned long long>(metrics.receivedPackets),
        static_cast<unsigned long long>(metrics.receivedBytes));
    ImGui::Text(
        "Batches: decoded %llu, queued %llu, drained %llu, dropped %llu",
        static_cast<unsigned long long>(metrics.decodedBatches),
        static_cast<unsigned long long>(metrics.queuedBatches),
        static_cast<unsigned long long>(metrics.drainedBatches),
        static_cast<unsigned long long>(metrics.droppedBatches));
    ImGui::Text(
        "Commands: drained %llu, applied %llu, skipped %llu, coalesced %llu, truncated %llu",
        static_cast<unsigned long long>(metrics.drainedCommands),
        static_cast<unsigned long long>(metrics.appliedCommands),
        static_cast<unsigned long long>(metrics.skippedCommands),
        static_cast<unsigned long long>(metrics.coalescedCommands),
        static_cast<unsigned long long>(metrics.truncatedBatches));
    ImGui::Text(
        "Feedback: queued %llu, sent %llu, dropped %llu",
        static_cast<unsigned long long>(metrics.feedbackQueued),
        static_cast<unsigned long long>(metrics.feedbackSent),
        static_cast<unsigned long long>(metrics.feedbackDropped));
    if (metrics.decodeErrors > 0U)
    {
        ImGui::TextColored(
            ImVec4(1.00f, 0.62f, 0.25f, 1.00f),
            "Decode errors: %llu",
            static_cast<unsigned long long>(metrics.decodeErrors));
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

bool RuntimeDebugOverlay::HandleShortcut(const SceneRegistry&)
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
        Activate();
    }

    return true;
}

void RuntimeDebugOverlay::OnRuntimeReloaded(const SceneRegistry& liveScene)
{
    state_.ResetObservedRuntimeState();
    state_.ResetInteractiveState();
    transportMetricsRefreshInitialized_ = false;

    if (state_.Active() && state_.HasInteractiveOverrides())
    {
        RefreshPreviewFromLive(liveScene);
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
                                      const std::vector<CommandBatch>& appliedBatches,
                                      const std::size_t appliedBatchCount,
                                      const std::size_t appliedCommandCount)
{
    state_.NoteCommandTraffic(appliedBatchCount, appliedCommandCount);

    // The transport snapshot below locks bridge state shared with the UDP I/O thread
    // and copies both status strings; skip all of it while nothing displays it.
    if (!state_.Active())
    {
        return;
    }

    const bool commandConfigured = bridge != nullptr && bridge->HasCommandReceiver();
    const bool commandReady = bridge != nullptr && bridge->CommandTransportReady();
    const bool feedbackConfigured = bridge != nullptr && bridge->HasFeedbackSender();
    const bool feedbackReady = bridge != nullptr && bridge->FeedbackTransportReady();
    const UdpRuntimeBridgeMetrics metrics = CollectTransportMetrics(bridge, appliedCommandCount);

    state_.UpdateTransportState(
        commandConfigured,
        commandReady,
        feedbackConfigured,
        feedbackReady,
        metrics,
        std::string(commandStatus),
        std::string(feedbackStatus));

    if (!state_.HasInteractiveOverrides())
    {
        return;
    }

    if (!preview_.Ready())
    {
        RefreshPreviewFromLive(liveScene);
        return;
    }

    if (!preview_.ApplyLiveBatches(appliedBatches, state_))
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

void RuntimeDebugOverlay::SelectFirstReticleOnActivePage(const SceneRegistry& displayScene, std::string* statusOut)
{
    const std::string activePage = displayScene.ActivePageName();
    const PageDefinition* page = FindPage(displayScene.Document(), activePage);
    if (page == nullptr)
    {
        if (statusOut != nullptr)
        {
            *statusOut = "No active page is available for the manual test panel.";
        }
        return;
    }

    const auto reticlePointers = displayScene.CollectPageReticlePointers(page->name);
    if (reticlePointers.empty())
    {
        if (statusOut != nullptr)
        {
            *statusOut = "The active page does not expose any reticle.";
        }
        return;
    }

    const ReticleGroup* firstReticle = reticlePointers.front();
    if (firstReticle == nullptr)
    {
        if (statusOut != nullptr)
        {
            *statusOut = "The first reticle entry is invalid.";
        }
        return;
    }

    state_.SelectReticle(ReticleKey {page->name, firstReticle->id, ClassifyReticle(*page, firstReticle->id)});
    if (statusOut != nullptr)
    {
        *statusOut = "Selected reticle '" + firstReticle->id + "' on page '" + page->name + "'.";
    }
}

const ReticleGroup* RuntimeDebugOverlay::FindSelectedReticle(const SceneRegistry& displayScene) const
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
}

template <typename Mutation, typename... Args>
bool RuntimeDebugOverlay::MutateSelectedReticleWithRollback(const SceneRegistry& liveScene,
                                                            const SceneRegistry& displayScene,
                                                            Mutation&& mutation,
                                                            const char* const successMessage,
                                                            Args&&... args)
{
    if (!state_.SelectedReticle().has_value())
    {
        state_.SetTestPanelStatus("Select one reticle before using this action.");
        return false;
    }

    const ReticleKey key = *state_.SelectedReticle();
    const ReticleGroup* currentReticle = FindSelectedReticle(displayScene);
    if (currentReticle == nullptr)
    {
        state_.SetTestPanelStatus("The selected reticle is no longer available in the preview.");
        return false;
    }

    const ReticleBypassState* previousBypass = state_.FindReticleBypass(key);
    std::optional<ReticleBypassState> rollbackSnapshot;
    if (previousBypass != nullptr)
    {
        rollbackSnapshot = *previousBypass;
    }

    ReticleBypassState& bypass = state_.EnsureReticleBypass(key, *currentReticle);
    std::invoke(
        std::forward<Mutation>(mutation),
        bypass.draft,
        std::forward<Args>(args)...);
    if (!RefreshPreviewFromLive(liveScene))
    {
        const std::string mutationError =
            preview_.LastError().empty() ? std::string {"Unknown preview rebuild failure."} : preview_.LastError();

        if (rollbackSnapshot.has_value())
        {
            if (ReticleBypassState* restore = state_.FindReticleBypass(key); restore != nullptr)
            {
                *restore = *rollbackSnapshot;
            }
        }
        else
        {
            state_.ReleaseReticleBypass(key);
        }

        if (!RefreshPreviewFromLive(liveScene))
        {
            const std::string rollbackError =
                preview_.LastError().empty() ? std::string {"Unknown rollback failure."} : preview_.LastError();
            state_.SetTestPanelStatus(
                "Rejected the local reticle mutation and failed to restore the preview: " + rollbackError);
        }
        else
        {
            state_.SetTestPanelStatus("Rejected the local reticle mutation: " + mutationError);
        }
        return false;
    }

    state_.SetTestPanelStatus(successMessage);
    return true;
}

void RuntimeDebugOverlay::DrawManualTestPanel(const SceneRegistry& liveScene, const SceneRegistry& displayScene)
{
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
        state_.ReleaseAllStrobeBypasses();
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
        std::string status = "No selectable reticle is available on the active page.";
        SelectFirstReticleOnActivePage(displayScene, &status);
        state_.SetTestPanelStatus(status);
    }

    if (ImGui::Button("Toggle selected reticle visibility"))
    {
        const ReticleGroup* reticle = FindSelectedReticle(displayScene);
        if (reticle == nullptr)
        {
            state_.SetTestPanelStatus("Select one reticle before toggling its visibility.");
        }
        else
        {
            MutateSelectedReticleWithRollback(
                liveScene,
                displayScene,
                SetReticleVisible,
                "Toggled the selected reticle visibility.",
                !reticle->visible);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Nudge selected reticle +X"))
    {
        const ReticleGroup* reticle = FindSelectedReticle(displayScene);
        if (reticle == nullptr)
        {
            state_.SetTestPanelStatus("Select one reticle before nudging it.");
        }
        else
        {
            MutateSelectedReticleWithRollback(
                liveScene,
                displayScene,
                NudgeReticlePosition,
                "Moved the selected reticle by +0.05 on the X axis.",
                Vec2 {kReticleNudgeStep, 0.0f});
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
}

int RuntimeDebugOverlay::PreferredPanelWidth() const noexcept
{
    return kDebugPanelWidth;
}

const SceneRegistry& RuntimeDebugOverlay::RenderScene(const SceneRegistry& liveScene) const noexcept
{
    return UsesPreviewScene() ? preview_.Scene() : liveScene;
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

    const SceneRegistry& displayScene = UsesPreviewScene() ? preview_.Scene() : liveScene;
    const auto pages = displayScene.Pages();
    RuntimeDebugInspectorFrameState sceneFrameState;

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
    DrawTransportMetrics(transport.metrics);

    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.72f, 0.86f, 0.95f, 1.00f), "Page override");

    std::string comboPreview = state_.PageBypassed()
                                   ? state_.ForcedActivePage()
                                   : displayScene.ActivePageName();
    if (comboPreview.empty() && !pages.empty())
    {
        comboPreview = pages.front().name;
    }

    ImGui::PushID("PageOverride");
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

        RefreshPreviewFromLive(liveScene);
        sceneFrameState.InvalidateSnapshot();
    }

    if (sceneFrameState.SnapshotValid())
    {
        ImGui::SameLine();
        if (ImGui::BeginCombo("Preview page", comboPreview.c_str()))
        {
            for (const PageSummary& page : pages)
            {
                ImGui::PushID(page.name.c_str());
                const bool selected = comboPreview == page.name;
                if (ImGui::Selectable(page.name.c_str(), selected))
                {
                    state_.EnablePageBypass(page.name);
                    RefreshPreviewFromLive(liveScene);
                    sceneFrameState.InvalidateSnapshot();
                }

                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
    }
    ImGui::PopID();

    if (sceneFrameState.SnapshotValid())
    {
        ImGui::Text("Live active page: %s", liveScene.ActivePageName().c_str());
        ImGui::Text("Preview active page: %s", displayScene.ActivePageName().c_str());
    }
    else
    {
        DrawDisabledText(RuntimeDebugInspectorFrameState::RefreshNotice());
    }

    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.72f, 0.86f, 0.95f, 1.00f), "Strobe override");

    if (!sceneFrameState.SnapshotValid())
    {
        DrawDisabledText(RuntimeDebugInspectorFrameState::RefreshNotice());
    }
    else
    {
        const std::string strobePageName = displayScene.ActivePageName();
        const PageDefinition* strobePageDefinition = FindPage(displayScene.Document(), strobePageName);
        if (strobePageDefinition == nullptr || strobePageDefinition->strobes.empty())
        {
            ImGui::TextDisabled("The preview page does not expose any strobe.");
        }
        else
        {
            const std::optional<StrobeSummary> liveStrobe = liveScene.StrobeForPage(strobePageName);
            const std::optional<StrobeSummary> previewStrobe = displayScene.StrobeForPage(strobePageName);
            const StrobeScanResult previewScan = displayScene.ScanStrobeForPage(strobePageName);
            const std::optional<StrobeMagnetSummary>& previewMagnet = previewScan.magnet;
            const std::optional<StrobeCaptureResult>& previewCapture = previewScan.capture;
            const PageStrobeDefinition* authoredActiveStrobe = FindActivePageStrobeDefinition(*strobePageDefinition);

            std::string previewStrobeName;
            if (const std::string* forcedStrobe = state_.ForcedActiveStrobe(strobePageName); forcedStrobe != nullptr)
            {
                previewStrobeName = *forcedStrobe;
            }
            else if (previewStrobe.has_value())
            {
                previewStrobeName = previewStrobe->strobeName;
            }
            else if (authoredActiveStrobe != nullptr)
            {
                previewStrobeName = authoredActiveStrobe->name;
            }
            else
            {
                previewStrobeName = strobePageDefinition->strobes.front().name;
            }

            bool strobeBypassed = state_.StrobeBypassed(strobePageName);
            ImGui::PushID("StrobeOverride");
            if (ImGui::Checkbox("Bypass active strobe selection", &strobeBypassed))
            {
                if (strobeBypassed)
                {
                    state_.EnableStrobeBypass(strobePageName, previewStrobeName);
                    state_.SetTestPanelStatus(
                        "Preview now forces strobe '" + previewStrobeName + "' on page '" + strobePageName + "'.");
                }
                else
                {
                    state_.DisableStrobeBypass(strobePageName);
                    state_.SetTestPanelStatus(
                        "Preview now follows the live strobe selection again on page '" + strobePageName + "'.");
                }

                RefreshPreviewFromLive(liveScene);
                sceneFrameState.InvalidateSnapshot();
            }

            if (sceneFrameState.SnapshotValid())
            {
                ImGui::SameLine();
                if (ImGui::BeginCombo("Preview strobe", previewStrobeName.c_str()))
                {
                    for (const PageStrobeDefinition& strobe : strobePageDefinition->strobes)
                    {
                        const bool selected = previewStrobeName == strobe.name;
                        if (ImGui::Selectable(strobe.name.c_str(), selected))
                        {
                            state_.EnableStrobeBypass(strobePageName, strobe.name);
                            state_.SetTestPanelStatus(
                                "Preview now forces strobe '" + strobe.name + "' on page '" + strobePageName + "'.");
                            RefreshPreviewFromLive(liveScene);
                            sceneFrameState.InvalidateSnapshot();
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
            ImGui::PopID();

            if (!sceneFrameState.SnapshotValid())
            {
                DrawDisabledText(RuntimeDebugInspectorFrameState::RefreshNotice());
            }
            else
            {
                if (liveStrobe.has_value())
                {
                    ImGui::Text("Live active strobe: %s", liveStrobe->strobeName.c_str());
                }
                else
                {
                    if (liveScene.ActivePageName() != strobePageName)
                    {
                        ImGui::TextDisabled("Live active strobe: page is not active in the live runtime.");
                    }
                    else
                    {
                        ImGui::TextDisabled("Live active strobe: unavailable.");
                    }
                }

                if (previewStrobe.has_value())
                {
                    ImGui::Text("Preview active strobe: %s", previewStrobe->strobeName.c_str());
                    bool previewStrobeVisible = previewStrobe->visible;
                    ImGui::BeginDisabled(true);
                    ImGui::Checkbox("Preview strobe visible", &previewStrobeVisible);
                    ImGui::EndDisabled();
                    ImGui::Text(
                        "Preview position: (%.4f, %.4f)",
                        previewStrobe->position.x,
                        previewStrobe->position.y);

                    if (previewStrobe->capture.shape == StrobeCaptureShape::Circle)
                    {
                        ImGui::Text("Capture: circle radius %.4f", previewStrobe->capture.radius);
                    }
                    else
                    {
                        ImGui::Text(
                            "Capture: rectangle (%.4f, %.4f)",
                            previewStrobe->capture.size.x,
                            previewStrobe->capture.size.y);
                    }
                }
                else
                {
                    ImGui::TextDisabled("Preview active strobe: unavailable.");
                }

                if (previewMagnet.has_value())
                {
                    ImGui::Text(
                        "Preview magnet: %s | radius %.4f | strength %.4f",
                        previewMagnet->enabled ? "enabled" : "disabled",
                        previewMagnet->radius,
                        previewMagnet->strength);
                    if (previewMagnet->magnetized)
                    {
                        ImGui::Text(
                            "Magnetized target: %s at (%.4f, %.4f)",
                            previewMagnet->reticleId.c_str(),
                            previewMagnet->targetPosition.x,
                            previewMagnet->targetPosition.y);
                    }
                }

                if (previewCapture.has_value())
                {
                    ImGui::Text(
                        "Capture candidate: %s at (%.4f, %.4f)",
                        previewCapture->reticleId.c_str(),
                        previewCapture->position.x,
                        previewCapture->position.y);
                }
            }
        }
    }

    ImGui::BeginChild("DebugBody", ImVec2(0.0f, -kBottomPanelHeight), false);
    if (!sceneFrameState.SnapshotValid())
    {
        DrawDisabledText(RuntimeDebugInspectorFrameState::RefreshNotice());
    }
    else
    {
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
            ImGui::PushID(page.name.c_str());
            const bool pageOpened = ImGui::TreeNodeEx("##page_tree", pageFlags, "%s", pageLabel.c_str());
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
            {
                state_.SelectPage(page.name);
            }

            if (!pageOpened)
            {
                ImGui::PopID();
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

                ImGui::PushID(static_cast<int>(entry.key.kind));
                ImGui::PushID(reticle->id.c_str());
                ImGui::PushID(static_cast<int>(index));
                ImGui::TreeNodeEx("##reticle_tree", reticleFlags, "%s", entry.label.c_str());
                if (ImGui::IsItemClicked())
                {
                    state_.SelectReticle(entry.key);
                }
                ImGui::PopID();
                ImGui::PopID();
                ImGui::PopID();
            }

            ImGui::TreePop();
            ImGui::PopID();
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
            const ReticleGroup* inspectedReticle = FindSelectedReticle(displayScene);
            if (page == nullptr || inspectedReticle == nullptr)
            {
                ImGui::TextColored(ImVec4(1.00f, 0.45f, 0.45f, 1.00f), "The selected reticle is no longer available.");
            }
            else
            {
                const ReticleBypassState* bypass = state_.FindReticleBypass(selectedKey);
                bool bypassed = bypass != nullptr;
                const PageBlinkDefinition* defaultBlink = ResolvePageDefaultBlink(*page);
                const bool pageHasNamedBlinkTypes = !page->blinkTypes.empty();
                RuntimeDebugInspectorFrameState inspectorFrameState;
                ImGui::PushID(selectedKey.pageName.c_str());
                ImGui::PushID(static_cast<int>(selectedKey.kind));
                ImGui::PushID(selectedKey.reticleId.c_str());
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

                    RefreshPreviewFromLive(liveScene);
                    inspectorFrameState.InvalidateSnapshot();
                }

                if (inspectorFrameState.SnapshotValid())
                {
                    ImGui::Separator();

                    bool visible = inspectedReticle->visible;
                    if (ImGui::Checkbox("Visible", &visible))
                    {
                        MutateSelectedReticleWithRollback(
                            liveScene,
                            displayScene,
                            SetReticleVisible,
                            "Updated reticle visibility through the manual debug bypass.",
                            visible);
                        inspectorFrameState.InvalidateSnapshot();
                    }
                }

                if (inspectorFrameState.SnapshotValid())
                {
                    bool blinkEnabled = inspectedReticle->blink.enabled;
                    const bool canEnableBlink =
                        blinkEnabled || !inspectedReticle->blink.typeName.empty() || defaultBlink != nullptr;
                    ImGui::BeginDisabled(!canEnableBlink);
                    if (ImGui::Checkbox("Blink enabled", &blinkEnabled))
                    {
                        MutateSelectedReticleWithRollback(
                            liveScene,
                            displayScene,
                            SetReticleBlinkEnabled,
                            "Updated reticle blink state.",
                            blinkEnabled);
                        inspectorFrameState.InvalidateSnapshot();
                    }
                    ImGui::EndDisabled();
                }

                if (inspectorFrameState.SnapshotValid())
                {
                    std::string blinkTypeName = inspectedReticle->blink.typeName;
                    const bool canClearToPageDefault = !blinkTypeName.empty() && defaultBlink != nullptr;
                    const bool canSelectNamedBlinkType = pageHasNamedBlinkTypes;
                    const bool canEditBlinkType = canClearToPageDefault || canSelectNamedBlinkType;
                    const std::string blinkTypePreview = BuildBlinkTypePreviewLabel(*page, inspectedReticle->blink);
                    ImGui::BeginDisabled(!canEditBlinkType);
                    if (ImGui::BeginCombo("Blink type", blinkTypePreview.c_str()))
                    {
                        const bool noTypeSelected = blinkTypeName.empty();
                        if (defaultBlink != nullptr && ImGui::Selectable("<page default>", noTypeSelected))
                        {
                            MutateSelectedReticleWithRollback(
                                liveScene,
                                displayScene,
                                ClearReticleBlinkType,
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
                                MutateSelectedReticleWithRollback(
                                    liveScene,
                                    displayScene,
                                    SetReticleBlinkType,
                                    "Updated the reticle-specific blink type.",
                                    blinkType);
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
                    ImGui::EndDisabled();

                    if (!pageHasNamedBlinkTypes && defaultBlink == nullptr)
                    {
                        ImGui::TextDisabled("This page does not define any blink type.");
                    }
                    else if (blinkTypeName.empty() && !CanResolveBlinkState(*page, inspectedReticle->blink))
                    {
                        ImGui::TextDisabled("Pick one named blink type before enabling blinking on this page.");
                    }
                }

                if (inspectorFrameState.SnapshotValid())
                {
                    std::array<float, 2> position {
                        inspectedReticle->transform.position.x,
                        inspectedReticle->transform.position.y};
                    if (ImGui::DragFloat2("Position", position.data(), 0.005f, -1.5f, 1.5f, "%.4f"))
                    {
                        MutateSelectedReticleWithRollback(
                            liveScene,
                            displayScene,
                            SetReticlePosition,
                            "Updated the reticle position.",
                            Vec2 {position[0], position[1]});
                        inspectorFrameState.InvalidateSnapshot();
                    }
                }

                if (inspectorFrameState.SnapshotValid())
                {
                    float rotation = inspectedReticle->transform.rotationDegrees;
                    if (ImGui::DragFloat("Rotation", &rotation, 0.25f, -360.0f, 360.0f, "%.3f"))
                    {
                        MutateSelectedReticleWithRollback(
                            liveScene,
                            displayScene,
                            SetReticleRotation,
                            "Updated the reticle rotation.",
                            rotation);
                        inspectorFrameState.InvalidateSnapshot();
                    }
                }

                if (inspectorFrameState.SnapshotValid())
                {
                    bool colorOverride = inspectedReticle->overrides.color.has_value();
                    ImVec4 color = ToImGuiColor(ResolveReticleColor(*inspectedReticle));
                    if (ImGui::Checkbox("Color override", &colorOverride))
                    {
                        const ColorRgba overrideColor = FromImGuiColor(color);
                        MutateSelectedReticleWithRollback(
                            liveScene,
                            displayScene,
                            SetReticleColorOverrideEnabled,
                            colorOverride ? "Enabled one local color override."
                                          : "Removed the local color override.",
                            colorOverride,
                            overrideColor);
                        inspectorFrameState.InvalidateSnapshot();
                    }
                    if (inspectorFrameState.SnapshotValid() && colorOverride && ImGui::ColorEdit4("Color", &color.x))
                    {
                        const ColorRgba overrideColor = FromImGuiColor(color);
                        MutateSelectedReticleWithRollback(
                            liveScene,
                            displayScene,
                            SetReticleColorOverrideValue,
                            "Updated the local reticle color override.",
                            overrideColor);
                        inspectorFrameState.InvalidateSnapshot();
                    }
                }

                if (inspectorFrameState.SnapshotValid())
                {
                    bool thicknessOverride = inspectedReticle->overrides.thickness.has_value();
                    float thickness = ResolveReticleThickness(*inspectedReticle);
                    if (ImGui::Checkbox("Thickness override", &thicknessOverride))
                    {
                        MutateSelectedReticleWithRollback(
                            liveScene,
                            displayScene,
                            SetReticleThicknessOverrideEnabled,
                            thicknessOverride ? "Enabled one local thickness override."
                                              : "Removed the local thickness override.",
                            thicknessOverride,
                            thickness);
                        inspectorFrameState.InvalidateSnapshot();
                    }
                    if (inspectorFrameState.SnapshotValid() &&
                        thicknessOverride &&
                        ImGui::DragFloat("Thickness", &thickness, 0.0005f, 0.0001f, 0.05f, "%.4f"))
                    {
                        MutateSelectedReticleWithRollback(
                            liveScene,
                            displayScene,
                            SetReticleThicknessOverrideValue,
                            "Updated the local reticle thickness override.",
                            thickness);
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
                                MutateSelectedReticleWithRollback(
                                    liveScene,
                                    displayScene,
                                    SetTextPrimitiveText,
                                    "Updated one text primitive.",
                                    primitive.id,
                                    text);
                                inspectorFrameState.InvalidateSnapshot();
                                break;
                            }

                            float letterSpacing = geometry->letterSpacing;
                            const std::string spacingLabel =
                                "Letter spacing##" + primitive.id + "_" + std::to_string(primitiveIndex);
                            if (ImGui::DragFloat(spacingLabel.c_str(), &letterSpacing, 0.0005f, 0.0f, 0.2f, "%.4f"))
                            {
                                MutateSelectedReticleWithRollback(
                                    liveScene,
                                    displayScene,
                                    SetTextPrimitiveLetterSpacing,
                                    "Updated one text primitive letter spacing.",
                                    primitive.id,
                                    letterSpacing);
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
                    ImGui::TextColored(ImVec4(0.72f, 0.86f, 0.95f, 1.00f), "Time primitives");
                    bool anyEditableTime = false;
                    for (std::size_t primitiveIndex = 0; primitiveIndex < inspectedReticle->primitives.size(); ++primitiveIndex)
                    {
                        const Primitive& primitive = inspectedReticle->primitives[primitiveIndex];
                        const auto* geometry = std::get_if<TimeGeometry>(&primitive.geometry);
                        if (geometry == nullptr)
                        {
                            continue;
                        }

                        anyEditableTime = true;
                        ImGui::PushID(primitive.id.c_str());
                        ImGui::PushID(static_cast<int>(primitiveIndex));
                        ImGui::Text("%s", primitive.id.empty() ? "<unnamed time>" : primitive.id.c_str());

                        bool valueBypass = geometry->runtimeValueOverride.has_value();
                        if (ImGui::Checkbox("Numeric value bypass", &valueBypass))
                        {
                            const TimeValue seedValue =
                                geometry->runtimeValueOverride.value_or(CurrentTimeValueForUi(
                                    geometry->runtimeUtc.value_or(geometry->utc)));
                            MutateSelectedReticleWithRollback(
                                liveScene,
                                displayScene,
                                SetTimePrimitiveValueBypassEnabled,
                                valueBypass ? "Enabled one time numeric bypass."
                                            : "Cleared one time numeric bypass.",
                                primitive.id,
                                valueBypass,
                                seedValue);
                            inspectorFrameState.InvalidateSnapshot();
                            ImGui::PopID();
                            ImGui::PopID();
                            break;
                        }

                        if (geometry->runtimeValueOverride.has_value())
                        {
                            TimeValue value = *geometry->runtimeValueOverride;
                            std::array<int, 3> date {value.year, value.month, value.day};
                            if (ImGui::InputInt3("Date Y/M/D", date.data()))
                            {
                                value.year = date[0];
                                value.month = date[1];
                                value.day = date[2];
                                value = ClampTimeValueForUi(value);
                                MutateSelectedReticleWithRollback(
                                    liveScene,
                                    displayScene,
                                    SetTimePrimitiveOverrideValue,
                                    "Updated one time bypass date.",
                                    primitive.id,
                                    value);
                                inspectorFrameState.InvalidateSnapshot();
                                ImGui::PopID();
                                ImGui::PopID();
                                break;
                            }

                            std::array<int, 3> clock {value.hour, value.minute, value.second};
                            if (ImGui::InputInt3("Time H/M/S", clock.data()))
                            {
                                value.hour = clock[0];
                                value.minute = clock[1];
                                value.second = clock[2];
                                value = ClampTimeValueForUi(value);
                                MutateSelectedReticleWithRollback(
                                    liveScene,
                                    displayScene,
                                    SetTimePrimitiveOverrideValue,
                                    "Updated one time bypass clock.",
                                    primitive.id,
                                    value);
                                inspectorFrameState.InvalidateSnapshot();
                                ImGui::PopID();
                                ImGui::PopID();
                                break;
                            }
                        }

                        bool utc = geometry->runtimeUtc.value_or(geometry->utc);
                        if (ImGui::Checkbox("UTC", &utc))
                        {
                            MutateSelectedReticleWithRollback(
                                liveScene,
                                displayScene,
                                SetTimePrimitiveUtc,
                                "Updated one time UTC mode.",
                                primitive.id,
                                utc);
                            inspectorFrameState.InvalidateSnapshot();
                            ImGui::PopID();
                            ImGui::PopID();
                            break;
                        }

                        TimeFieldVisibility fields = geometry->runtimeFields.value_or(TimeFieldVisibility {});
                        bool fieldsChanged = false;
                        fieldsChanged = ImGui::Checkbox("Show year", &fields.year) || fieldsChanged;
                        ImGui::SameLine();
                        fieldsChanged = ImGui::Checkbox("Show month", &fields.month) || fieldsChanged;
                        ImGui::SameLine();
                        fieldsChanged = ImGui::Checkbox("Show day", &fields.day) || fieldsChanged;
                        fieldsChanged = ImGui::Checkbox("Show hour", &fields.hour) || fieldsChanged;
                        ImGui::SameLine();
                        fieldsChanged = ImGui::Checkbox("Show minute", &fields.minute) || fieldsChanged;
                        ImGui::SameLine();
                        fieldsChanged = ImGui::Checkbox("Show second", &fields.second) || fieldsChanged;
                        if (fieldsChanged)
                        {
                            if (HasVisibleTimeFieldForUi(fields))
                            {
                                MutateSelectedReticleWithRollback(
                                    liveScene,
                                    displayScene,
                                    SetTimePrimitiveFields,
                                    "Updated one time field visibility.",
                                    primitive.id,
                                    fields);
                                inspectorFrameState.InvalidateSnapshot();
                                ImGui::PopID();
                                ImGui::PopID();
                                break;
                            }

                            ImGui::TextDisabled("At least one time field must stay visible.");
                        }

                        float letterSpacing = geometry->letterSpacing;
                        if (ImGui::DragFloat("Letter spacing", &letterSpacing, 0.0005f, 0.0f, 0.2f, "%.4f"))
                        {
                            MutateSelectedReticleWithRollback(
                                liveScene,
                                displayScene,
                                SetTimePrimitiveLetterSpacing,
                                "Updated one time primitive letter spacing.",
                                primitive.id,
                                letterSpacing);
                            inspectorFrameState.InvalidateSnapshot();
                            ImGui::PopID();
                            ImGui::PopID();
                            break;
                        }

                        ImGui::Spacing();
                        ImGui::PopID();
                        ImGui::PopID();
                    }
                    if (inspectorFrameState.SnapshotValid() && !anyEditableTime)
                    {
                        ImGui::TextDisabled("No editable time primitive is attached to this reticle.");
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
                    DrawDisabledText(RuntimeDebugInspectorFrameState::RefreshNotice());
                }
                ImGui::PopID();
                ImGui::PopID();
                ImGui::PopID();
            }
        }
        ImGui::EndChild();
    }
    ImGui::EndChild();

    DrawManualTestPanel(liveScene, displayScene);

    ImGui::End();
    rlImGuiEnd();

    if (!open)
    {
        Deactivate();
    }
}

bool RuntimeDebugOverlay::Activate()
{
    state_.Activate();
    state_.ResetInteractiveState();
    preview_.Invalidate();
    transportMetricsRefreshInitialized_ = false;
    state_.SetTestPanelStatus(
        "Runtime debug mode enabled. The live scene stays in control until one local bypass is enabled.");
    return true;
}

void RuntimeDebugOverlay::Deactivate()
{
    state_.Deactivate();
    preview_.Invalidate();
    transportMetricsRefreshInitialized_ = false;
}

bool RuntimeDebugOverlay::RefreshPreviewFromLive(const SceneRegistry& liveScene)
{
    if (!state_.Active())
    {
        return true;
    }

    return preview_.ResetFromLive(liveScene, state_);
}

bool RuntimeDebugOverlay::UsesPreviewScene() const noexcept
{
    return state_.Active() && state_.HasInteractiveOverrides() && preview_.Ready();
}

// Only called while the overlay is active: Synchronize() skips the transport
// snapshot entirely when nothing displays it.
UdpRuntimeBridgeMetrics RuntimeDebugOverlay::CollectTransportMetrics(const UdpRuntimeBridge* const bridge,
                                                                    const std::size_t observedCommandCount)
{
    if (bridge == nullptr)
    {
        return {};
    }

    UdpRuntimeBridgeMetrics metrics = state_.Transport().metrics;
    const RuntimeDebugState::Clock::time_point now = RuntimeDebugState::Clock::now();
    const bool refreshDue =
        !transportMetricsRefreshInitialized_ ||
        observedCommandCount > 0U ||
        now - lastTransportMetricsRefresh_ >= kTransportMetricsRefreshInterval;
    if (!refreshDue)
    {
        return metrics;
    }

    metrics = bridge->MetricsSnapshot();
    lastTransportMetricsRefresh_ = now;
    transportMetricsRefreshInitialized_ = true;
    return metrics;
}
} // namespace mfd::window::debug
