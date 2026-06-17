/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorApplication.h"

/**
 * @file
 * @brief Page reticle, page title and page strobe selection inspector implementation extracted from `EditorApplicationInspectors`.
 */

#include <algorithm>
#include <array>
#include <string_view>

#include "internal/application/EditorApplicationInternal.h"
#include "EditorTutorialController.h"
#include "EditorTutorialData.h"
#include "EditorUiTheme.h"
#include "internal/application/EditorApplicationAuthoringSupport.h"

namespace
{
using editor::ui::BeginInspectorSection;
using editor::ui::InspectorHelpMarker;
using editor::ui::ShowItemTooltip;
using editor::detail::CopyTextBuffer;
using editor::detail::kTutorialPage1OwnshipReticleId;
using editor::detail::ToColorRgba;
using editor::app::ClipPrimitiveOption;
using editor::app::CollectClipPrimitiveOptions;
using editor::app::EffectiveDefaultBlinkTypeIndex;
using editor::app::kInvalidBlinkTypeIndex;
using editor::app::LineStyleLabel;
using editor::app::PageTitleDecorationLabel;
using editor::app::RefreshPageBlinkStateForEditor;
using editor::app::ReticleClipModeLabel;
}

namespace
{
ImVec4 ToImGuiColor(const mfd::ColorRgba& color)
{
    return ImVec4(
        static_cast<float>(color.r) / 255.0f,
        static_cast<float>(color.g) / 255.0f,
        static_cast<float>(color.b) / 255.0f,
        static_cast<float>(color.a) / 255.0f);
}

constexpr std::array<mfd::Align, 3> kSupportedTextAlignments {{
    mfd::Align::Left,
    mfd::Align::Center,
    mfd::Align::Right}};

const char* AlignLabel(const mfd::Align align) noexcept
{
    switch (align)
    {
    case mfd::Align::Left:
        return "Left";
    case mfd::Align::Right:
        return "Right";
    case mfd::Align::Center:
    default:
        return "Center";
    }
}

template <typename TGeometry>
void ApplyPrimitiveTextAlignmentChange(mfd::Primitive& primitive,
                                       TGeometry& geometry,
                                       const mfd::Align newAlign,
                                       const float width)
{
    if (geometry.align == newAlign)
    {
        return;
    }

    const float currentOriginX = editor::detail::PrimitiveTextOriginX(width, geometry.align);
    const float newOriginX = editor::detail::PrimitiveTextOriginX(width, newAlign);
    const mfd::Vec2 localDelta {newOriginX - currentOriginX, 0.0f};

    geometry.align = newAlign;
    primitive.transform.position =
        primitive.transform.position +
        mfd::Rotate(mfd::Scale(localDelta, primitive.transform.scale), primitive.transform.rotationDegrees);
}

/**
 * @brief Returns the local-space visual center of one reticle template or instance.
 */
mfd::Vec2 ReticleVisualCenterLocal(const mfd::ReticleGroup& reticle)
{
    bool hasBounds = false;
    mfd::Vec2 minPoint {};
    mfd::Vec2 maxPoint {};

    for (const auto& primitive : reticle.primitives)
    {
        editor::detail::ForEachPrimitiveBoundsLocalPoint(
            primitive,
            [&hasBounds, &minPoint, &maxPoint, &primitive](const mfd::Vec2 localPoint)
            {
                const mfd::Vec2 transformedPoint = mfd::ApplyTransform(localPoint, primitive.transform);
                if (!hasBounds)
                {
                    minPoint = transformedPoint;
                    maxPoint = transformedPoint;
                    hasBounds = true;
                    return;
                }

                minPoint.x = std::min(minPoint.x, transformedPoint.x);
                minPoint.y = std::min(minPoint.y, transformedPoint.y);
                maxPoint.x = std::max(maxPoint.x, transformedPoint.x);
                maxPoint.y = std::max(maxPoint.y, transformedPoint.y);
            });
    }

    if (!hasBounds)
    {
        return {};
    }

    return {
        (minPoint.x + maxPoint.x) * 0.5f,
        (minPoint.y + maxPoint.y) * 0.5f};
}

/**
 * @brief Rebuilds one transform while keeping the same local anchor at the same world position.
 */
mfd::Transform2D BuildTransformKeepingLocalPointWorldPosition(const mfd::Transform2D& startTransform,
                                                              const mfd::Vec2 localPoint,
                                                              const float rotationDegrees,
                                                              const mfd::Vec2 scale)
{
    const mfd::Vec2 worldPoint = mfd::ApplyTransform(localPoint, startTransform);
    const mfd::Vec2 offset = mfd::Rotate(mfd::Scale(localPoint, scale), rotationDegrees);

    return mfd::Transform2D {
        worldPoint - offset,
        rotationDegrees,
        scale};
}
}

void EditorApplication::MoveSelectedPageReticleToIndex(mfd::PageDefinition& page,
                                                       mfd::ReticleGroup& reticle,
                                                       const int targetIndex,
                                                       const char* const action)
{
    const int reticleIndex = documentState_.selection.pageReticleIndex;
    if (reticleIndex < 0 ||
        reticleIndex >= static_cast<int>(page.staticReticles.size()) ||
        targetIndex < 0 ||
        targetIndex >= static_cast<int>(page.staticReticles.size()) ||
        targetIndex == reticleIndex)
    {
        return;
    }

    PushUndoSnapshot();
    const std::string movedReticleId = reticle.id;

    auto movedReticle =
        std::move(page.staticReticles[static_cast<std::size_t>(reticleIndex)]);
    page.staticReticles.erase(page.staticReticles.begin() + reticleIndex);

    const int insertionIndex =
        std::clamp(targetIndex, 0, static_cast<int>(page.staticReticles.size()));

    page.staticReticles.insert(page.staticReticles.begin() + insertionIndex, std::move(movedReticle));
    SelectPageReticle(documentState_.selection.pageIndex, insertionIndex);
    RebuildStatus("Reticle '" + movedReticleId + "' moved " + action + " on page '" + page.name + "'.", false);
}

void EditorApplication::ApplyPageReticleIdEdit(mfd::PageDefinition& page,
                                               mfd::ReticleGroup& reticle,
                                               const std::string_view requestedId)
{
    const std::string previousId = reticle.id;
    reticle.id = MakeUniquePageReticleId(page, requestedId, previousId);

    if (tutorial_->TrackedReticleId() == previousId)
    {
        tutorial_->SetTrackedReticleId(reticle.id);
    }
}

void EditorApplication::DrawPageReticleInspector()
{
    mfd::PageDefinition* page = ActivePage();
    const std::vector<int> selectedIndices = SelectedPageReticleIndices();
    if (page == nullptr || selectedIndices.empty())
    {
        ImGui::TextDisabled("No page reticle selected.");
        return;
    }

    if (selectedIndices.size() > 1U)
    {
        ImGui::TextColored(ImVec4(0.33f, 0.86f, 0.78f, 1.0f), "Page reticles");
        ImGui::Text("Page: %s", page->name.c_str());
        ImGui::Text("%d reticles selected", static_cast<int>(selectedIndices.size()));
        ImGui::TextDisabled("Ctrl+click in the page or in the tree to add or remove reticles from the selection.");
        ImGui::Separator();

        // Full-width stacked actions so the labels never overflow the inspector panel.
        const ImVec2 actionSize(-1.0f, 0.0f);

        if (ImGui::Button("Copy selection", actionSize))
        {
            CopySelectedPageReticles();
        }
        ShowItemTooltip("Copy all selected page reticle instances.");

        if (ImGui::Button("Cut selection", actionSize))
        {
            CutSelectedPageReticles();
            return;
        }
        ShowItemTooltip("Copy all selected page reticle instances, then remove them from the page.");

        if (ImGui::Button("Delete from page", actionSize))
        {
            DeleteSelection();
            return;
        }
        ShowItemTooltip("Delete all selected reticles from the active page.");

        ImGui::BeginDisabled(clipboardState_.pageReticleClipboard.empty());
        if (ImGui::Button("Paste copies", actionSize))
        {
            PasteCopiedPageReticles();
            ImGui::EndDisabled();
            return;
        }
        ShowItemTooltip("Paste copied page reticles onto the active page.");
        ImGui::EndDisabled();

        if (ImGui::Button("Extract as reticle...", actionSize))
        {
            if (tutorial_->MatchesTarget("page_reticle_extract"))
            {
                tutorial_->AdvancePhase();
            }
            OpenReticleExtractionPopup();
            return;
        }
        ShowItemTooltip("Replace the current selection with one reusable reticle template staged in the shared library.");
        tutorial_->DrawHalo(
            "page_reticle_extract",
            "Click Extract as reticle...",
            "Open the extraction workflow to review how page content can become one reusable library template.");

        ImGui::Spacing();
        ImGui::TextDisabled("Tips (?)");
        ShowItemTooltip(
            "Shortcuts: Ctrl+C / Ctrl+X / Ctrl+V / Del / Esc.\n"
            "Drag one selected reticle in the preview to move the whole group.\n"
            "Direct property editing stays available when a single reticle is selected.");
        return;
    }

    mfd::ReticleGroup* reticle = SelectedPageReticle();
    if (reticle == nullptr)
    {
        ImGui::TextDisabled("No page reticle selected.");
        return;
    }

    ImGui::TextColored(ImVec4(0.33f, 0.86f, 0.78f, 1.0f), "Page reticle");
    ImGui::Text("Page: %s", page->name.c_str());
    std::array<char, 128> reticleId {};
    CopyTextBuffer(reticleId, reticle->id);
    const bool reticleIdChanged = ImGui::InputText("Reticle id", reticleId.data(), reticleId.size());
    ShowItemTooltip("Unique page-local reticle instance id stored in the page JSON. The shared template id stays unchanged.");
    tutorial_->DrawHalo(
        "page_reticle_id",
        "Rename this page reticle",
        "Rename the Page1 ownship instance to `page1_ownship` without changing the shared template id `mfd_tutorial_aircraft`.");
    if (ImGui::IsItemActivated())
    {
        PushUndoSnapshot();
    }
    if (reticleIdChanged)
    {
        ApplyPageReticleIdEdit(*page, *reticle, reticleId.data());
    }
    if (tutorial_->MatchesTarget("page_reticle_id") &&
        page->name == "Page1" &&
        reticle->id == kTutorialPage1OwnshipReticleId)
    {
        tutorial_->CompleteStep();
        return;
    }
    if (!reticle->sourceTemplateId.empty())
    {
        ImGui::TextDisabled("Template: %s", reticle->sourceTemplateId.c_str());
    }
    ImGui::TextDisabled("Move inside the frame, rotate with the blue handle, scale with the corner handles.");
    ImGui::Separator();

    const int reticleIndex = documentState_.selection.pageReticleIndex;
    const int lastReticleIndex = static_cast<int>(page->staticReticles.size()) - 1;
    const bool sectionForceOpen = tutorial_->IsCoachVisible();

    // Primary actions stay visible; secondary clipboard/template actions move into an overflow menu
    // so the top of the inspector stays compact.
    if (ImGui::Button("Delete from page"))
    {
        DeleteSelection();
        return;
    }
    ShowItemTooltip("Delete this reticle instance from the active page.");

    ImGui::SameLine();
    if (ImGui::Button("Extract as reticle..."))
    {
        if (tutorial_->MatchesTarget("page_reticle_extract"))
        {
            tutorial_->AdvancePhase();
        }
        OpenReticleExtractionPopup();
        return;
    }
    ShowItemTooltip("Extract this page reticle as a reusable library template, then replace it with one template instance.");
    tutorial_->DrawHalo(
        "page_reticle_extract",
        "Click Extract as reticle...",
        "Open the extraction workflow to review how one page reticle can be promoted into the shared library.");

    ImGui::SameLine();
    if (ImGui::Button("More..."))
    {
        ImGui::OpenPopup("PageReticleActionsMenu");
    }
    ShowItemTooltip("Copy, cut, paste or jump to the source template of this page reticle.");

    const bool hasSourceTemplate =
        !reticle->sourceTemplateId.empty() &&
        documentState_.loaded.document.reticleLibrary.find(reticle->sourceTemplateId) !=
            documentState_.loaded.document.reticleLibrary.end();

    if (ImGui::BeginPopup("PageReticleActionsMenu"))
    {
        if (ImGui::MenuItem("Copy"))
        {
            CopySelectedPageReticles();
        }
        if (ImGui::MenuItem("Cut"))
        {
            CutSelectedPageReticles();
            ImGui::EndPopup();
            return;
        }
        if (ImGui::MenuItem("Paste copies", nullptr, false, !clipboardState_.pageReticleClipboard.empty()))
        {
            PasteCopiedPageReticles();
            ImGui::EndPopup();
            return;
        }
        if (ImGui::MenuItem("Edit source template", nullptr, false, hasSourceTemplate))
        {
            SelectLibraryReticle(reticle->sourceTemplateId);
            RebuildStatus("Editing template '" + reticle->sourceTemplateId + "' in the reticle studio.", false);
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    InspectorHelpMarker(
        "Shortcuts: Del removes the reticle, Ctrl+X / Ctrl+C / Ctrl+V cut/copy/paste, Esc clears the selection.");

    if (BeginInspectorSection("section_page_reticle_placement", "Placement & draw order", true, sectionForceOpen))
    {
        ImGui::TextDisabled("Draw order: %d / %d", reticleIndex + 1, std::max(1, static_cast<int>(page->staticReticles.size())));

        const std::string currentLayerLabel = reticle->layerId.empty() ? std::string {"<missing>"} : reticle->layerId;
        if (ImGui::BeginCombo("Editor layer", currentLayerLabel.c_str()))
        {
            for (const auto& layer : page->editor.layers)
            {
                const bool selected = reticle->layerId == layer.id;
                const std::string label = layer.id + (layer.visible ? "" : " (hidden)");
                if (ImGui::Selectable(label.c_str(), selected))
                {
                    PushUndoSnapshot();
                    reticle->layerId = layer.id;
                }
                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }
        ShowItemTooltip("Assign this page reticle to an editor-only layer.");
        ImGui::SameLine();
        if (ImGui::Button("Page layers..."))
        {
            SelectPage(documentState_.selection.pageIndex);
            RebuildStatus("Layer editor opened for page '" + page->name + "'.", false);
            return;
        }
        ShowItemTooltip("Open the page inspector to edit the available editor-only layers.");
        InspectorHelpMarker("Hidden layers stay editable in the inspector but are not rendered in the editor preview.");

        const bool canMoveBackward = reticleIndex > 0;
        const bool canMoveForward = reticleIndex >= 0 && reticleIndex < lastReticleIndex;

        ImGui::BeginDisabled(!canMoveBackward);
        if (ImGui::Button("<<##reticle_send_to_back"))
        {
            MoveSelectedPageReticleToIndex(*page, *reticle, 0, "to the back");
            ImGui::EndDisabled();
            return;
        }
        ShowItemTooltip("Send to back (first draw-order slot).");
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(!canMoveBackward);
        if (ImGui::Button("<##reticle_step_back"))
        {
            MoveSelectedPageReticleToIndex(*page, *reticle, reticleIndex - 1, "backward");
            ImGui::EndDisabled();
            return;
        }
        ShowItemTooltip("Step one slot earlier in the draw order.");
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(!canMoveForward);
        if (ImGui::Button(">##reticle_step_forward"))
        {
            MoveSelectedPageReticleToIndex(*page, *reticle, reticleIndex + 1, "forward");
            ImGui::EndDisabled();
            return;
        }
        ShowItemTooltip("Step one slot later in the draw order.");
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(!canMoveForward);
        if (ImGui::Button(">>##reticle_bring_to_front"))
        {
            MoveSelectedPageReticleToIndex(*page, *reticle, lastReticleIndex, "to the front");
            ImGui::EndDisabled();
            return;
        }
        ShowItemTooltip("Bring to front (last draw-order slot).");
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled("Order");
    }

    if (BeginInspectorSection("section_page_reticle_display", "Display & blink", true, sectionForceOpen))
    {
        bool visible = reticle->visible;
        if (ImGui::Checkbox("Visible", &visible))
        {
            PushUndoSnapshot();
            reticle->visible = visible;
        }
        ShowItemTooltip("Toggle whether this page reticle instance is rendered.");

        bool drawOnTop = reticle->drawOnTop;
        if (ImGui::Checkbox("Draw on top", &drawOnTop))
        {
            PushUndoSnapshot();
            reticle->drawOnTop = drawOnTop;
        }
        ShowItemTooltip("Render this page reticle after regular page reticles while keeping the strobe on top.");

        DrawPageReticleBlinkInspector(*page, *reticle);
    }

    if (BeginInspectorSection("section_page_reticle_clipping", "Clipping", false, sectionForceOpen))
    {
    const std::vector<ClipPrimitiveOption> clipOptions = CollectClipPrimitiveOptions(*reticle);
    if (clipOptions.empty())
    {
        ImGui::TextDisabled("No supported convex primitive with an id is available for clipping.");
        ImGui::TextDisabled("Supported mask shapes: triangle, square, rectangle, circle, ellipse.");
    }
    else
    {
        std::string currentClipPrimitiveLabel = reticle->clipping.primitiveId.empty()
                                                    ? std::string {"<select primitive>"}
                                                    : std::string {"<missing primitive>"};
        for (const auto& option : clipOptions)
        {
            if (option.primitiveId == reticle->clipping.primitiveId)
            {
                currentClipPrimitiveLabel = option.label;
                break;
            }
        }

        if (ImGui::BeginCombo("Clip primitive", currentClipPrimitiveLabel.c_str()))
        {
            for (const auto& option : clipOptions)
            {
                const bool selected = option.primitiveId == reticle->clipping.primitiveId;
                if (ImGui::Selectable(option.label.c_str(), selected))
                {
                    ApplyPageReticleClipping(documentState_.selection.pageReticleIndex, reticle->clipping.mode, option.primitiveId);
                }
                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }
        ShowItemTooltip("Choose which convex primitive erases the inside or the outside toward the page background.");

        const char* currentModeLabel = ReticleClipModeLabel(reticle->clipping.mode);
        if (ImGui::BeginCombo("Clip mode", currentModeLabel))
        {
            const std::array modes {
                mfd::ReticleClipMode::None,
                mfd::ReticleClipMode::Inner,
                mfd::ReticleClipMode::Outer};

            for (const mfd::ReticleClipMode mode : modes)
            {
                const bool selected = reticle->clipping.mode == mode;
                if (ImGui::Selectable(ReticleClipModeLabel(mode), selected))
                {
                    const std::string primitiveId =
                        reticle->clipping.primitiveId.empty() ? clipOptions.front().primitiveId : reticle->clipping.primitiveId;
                    ApplyPageReticleClipping(documentState_.selection.pageReticleIndex, mode, primitiveId);
                }
                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }
        ShowItemTooltip(
            "Inner clipping erases the inside of the selected shape. Outer clipping erases everything outside it.");

        if (reticle->clipping.mode != mfd::ReticleClipMode::None && mfd::ResolveClipPrimitive(*reticle) == nullptr)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.42f, 1.0f), "The current clip primitive is missing or unsupported.");
        }

        ImGui::TextDisabled("The selected primitive erases toward the page background when this reticle is drawn.");
    }
    }

    if (BeginInspectorSection("section_page_reticle_transform", "Transform & style", true, sectionForceOpen))
    {
    const bool positionChanged = ImGui::DragFloat2("Position", &reticle->transform.position.x, 0.01f, -1.0f, 1.0f, "%.3f");
    ShowItemTooltip("Logical position of this page reticle on the active page.");
    if (ImGui::IsItemActivated())
    {
        PushUndoSnapshot();
    }
    if (positionChanged)
    {
        reticle->transform.position.x = std::clamp(reticle->transform.position.x, -1.0f, 1.0f);
        reticle->transform.position.y = std::clamp(reticle->transform.position.y, -1.0f, 1.0f);
    }

    const mfd::Transform2D rotationStartTransform = reticle->transform;
    if (ImGui::DragFloat("Rotation", &reticle->transform.rotationDegrees, 0.25f, -360.0f, 360.0f, "%.2f"))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        reticle->transform = BuildTransformKeepingLocalPointWorldPosition(
            rotationStartTransform,
            ReticleVisualCenterLocal(*reticle),
            reticle->transform.rotationDegrees,
            rotationStartTransform.scale);
    }
    ShowItemTooltip("Rotation in degrees around the reticle visual center.");

    const mfd::Transform2D scaleStartTransform = reticle->transform;
    if (ImGui::DragFloat2("Scale", &reticle->transform.scale.x, 0.01f, 0.05f, 10.0f, "%.3f"))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        reticle->transform.scale.x = std::max(0.05f, reticle->transform.scale.x);
        reticle->transform.scale.y = std::max(0.05f, reticle->transform.scale.y);
        reticle->transform = BuildTransformKeepingLocalPointWorldPosition(
            scaleStartTransform,
            ReticleVisualCenterLocal(*reticle),
            scaleStartTransform.rotationDegrees,
            reticle->transform.scale);
    }
    ShowItemTooltip("Per-axis scale applied to this page reticle instance.");

    ImVec4 stroke = ToImGuiColor(reticle->overrides.color.value_or(mfd::ColorRgba {0, 255, 102, 255}));
    if (ImGui::ColorEdit4("Stroke", &stroke.x))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        reticle->overrides.color = ToColorRgba(stroke);
    }
    ShowItemTooltip("Override the template stroke color for this page reticle instance.");

    float thickness = reticle->overrides.thickness.value_or(0.0042f);
    if (ImGui::DragFloat("Thickness", &thickness, 0.0002f, 0.0005f, 0.05f, "%.4f"))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        reticle->overrides.thickness = std::max(0.0005f, thickness);
    }
    ShowItemTooltip("Override the template stroke thickness for this page reticle instance.");
    }

    if (BeginInspectorSection("section_page_reticle_text", "Text & time overrides", true, sectionForceOpen))
    {
    for (auto& primitive : reticle->primitives)
    {
        auto* text = std::get_if<mfd::TextGeometry>(&primitive.geometry);
        auto* time = std::get_if<mfd::TimeGeometry>(&primitive.geometry);
        if ((text == nullptr && time == nullptr) || primitive.id.empty())
        {
            continue;
        }

        if (text != nullptr)
        {
            std::array<char, 128> buffer {};
            CopyTextBuffer(buffer, text->text);
            const std::string label = "Text##" + primitive.id;
            const bool changed = ImGui::InputText(label.c_str(), buffer.data(), buffer.size());
            ShowItemTooltip("Override the literal text for this text primitive on the page instance.");
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            if (changed)
            {
                text->text = buffer.data();
            }

            const std::string alignLabel = "Alignment##" + primitive.id;
            if (ImGui::BeginCombo(alignLabel.c_str(), AlignLabel(text->align)))
            {
                for (const mfd::Align candidate : kSupportedTextAlignments)
                {
                    const bool selected = text->align == candidate;
                    if (ImGui::Selectable(AlignLabel(candidate), selected) && !selected)
                    {
                        PushUndoSnapshot();
                        ApplyPrimitiveTextAlignmentChange(
                            primitive,
                            *text,
                            candidate,
                            MeasurePreviewTextWidthLogical(*text));
                    }
                    if (selected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }

                ImGui::EndCombo();
            }
            ShowItemTooltip("Choose whether this text grows left, right, or stays centered around its current anchor.");

            float letterSpacing = text->letterSpacing;
            const std::string spacingLabel = "Letter spacing##" + primitive.id;
            const bool spacingChanged =
                ImGui::DragFloat(spacingLabel.c_str(), &letterSpacing, 0.0005f, -0.05f, 0.10f, "%.4f");
            ShowItemTooltip("Override the letter spacing for this text primitive on the page instance.");
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            if (spacingChanged)
            {
                text->letterSpacing = letterSpacing;
            }
            continue;
        }

        if (time != nullptr)
        {
            const std::string formatLabel = "Time format##" + primitive.id;
            EditTimeFormatField(formatLabel,
                                "Override the strftime-style format used by this time primitive. "
                                "Changes apply when the field loses focus and invalid directives are rejected.",
                                *time);

            const std::string alignLabel = "Alignment##" + primitive.id;
            if (ImGui::BeginCombo(alignLabel.c_str(), AlignLabel(time->align)))
            {
                for (const mfd::Align candidate : kSupportedTextAlignments)
                {
                    const bool selected = time->align == candidate;
                    if (ImGui::Selectable(AlignLabel(candidate), selected) && !selected)
                    {
                        PushUndoSnapshot();
                        ApplyPrimitiveTextAlignmentChange(
                            primitive,
                            *time,
                            candidate,
                            MeasurePreviewTextWidthLogical(*time));
                    }
                    if (selected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }

                ImGui::EndCombo();
            }
            ShowItemTooltip("Choose whether this time display grows left, right, or stays centered around its current anchor.");

            bool utc = time->utc;
            const std::string utcLabel = "UTC##" + primitive.id;
            if (ImGui::Checkbox(utcLabel.c_str(), &utc))
            {
                PushUndoSnapshot();
                time->utc = utc;
            }
            ShowItemTooltip("Render this time primitive in UTC instead of local time.");

            float letterSpacing = time->letterSpacing;
            const std::string spacingLabel = "Letter spacing##" + primitive.id;
            const bool spacingChanged =
                ImGui::DragFloat(spacingLabel.c_str(), &letterSpacing, 0.0005f, -0.05f, 0.10f, "%.4f");
            ShowItemTooltip("Override the character spacing for this time primitive on the page instance.");
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            if (spacingChanged)
            {
                time->letterSpacing = letterSpacing;
            }
        }
    }
    }

}

mfd::Vec2 EditorApplication::PageTitleVisualCenterLocal(const mfd::PageDefinition& page) const
{
    return ReticleVisualCenterLocal(BuildPageTitlePreviewReticle(page));
}

void EditorApplication::DrawSelectedPageTitleInspector()
{
    mfd::PageDefinition* page = ActivePage();
    mfd::PageTitleDisplayDefinition* titleDisplay = SelectedPageTitleDisplay();
    if (page == nullptr || titleDisplay == nullptr)
    {
        ImGui::TextDisabled("No page title selected.");
        return;
    }

    const std::string displayedTitle = mfd::ResolvePageDisplayTitleText(page->name, page->title);
    ImGui::TextColored(ImVec4(0.33f, 0.86f, 0.78f, 1.0f), "Page title");
    InspectorHelpMarker(
        "Move inside the frame, rotate with the blue handle, scale with the corner handles.\n"
        "Edit the title text from the page inspector.");
    ImGui::Text("Page: %s", page->name.c_str());
    ImGui::TextWrapped("Displayed text: %s", displayedTitle.c_str());
    ImGui::Separator();

    const bool sectionForceOpen = tutorial_->IsCoachVisible();

    bool visible = titleDisplay->visible;
    if (ImGui::Checkbox("Visible", &visible))
    {
        PushUndoSnapshot();
        titleDisplay->visible = visible;
    }
    ShowItemTooltip("Show or hide both the title text and its decoration.");

    if (ImGui::Button("Edit page properties"))
    {
        SelectPage(documentState_.selection.pageIndex, false);
        return;
    }
    ShowItemTooltip("Return to the page inspector to edit the page name and title text.");

    if (BeginInspectorSection("section_title_appearance", "Decoration & color", true, sectionForceOpen))
    {
    const char* currentDecoration = PageTitleDecorationLabel(titleDisplay->decoration);
    const bool decorationComboOpen = ImGui::BeginCombo("Decoration", currentDecoration);
    if (ImGui::IsItemClicked() && tutorial_->MatchesTarget("page_title_decoration"))
    {
        tutorial_->AdvancePhase();
    }
    tutorial_->DrawHalo(
        "page_title_decoration",
        "Open Decoration",
        "Open the title decoration chooser. This inspector is the dedicated place to frame, move, scale, hide, or recolor the Page1 title.");
    if (decorationComboOpen)
    {
        const std::array decorations {
            mfd::PageTitleDecoration::Underline,
            mfd::PageTitleDecoration::Frame,
            mfd::PageTitleDecoration::None};

        for (const mfd::PageTitleDecoration decoration : decorations)
        {
            const bool selected = titleDisplay->decoration == decoration;
            if (ImGui::Selectable(PageTitleDecorationLabel(decoration), selected) && !selected)
            {
                PushUndoSnapshot();
                titleDisplay->decoration = decoration;
                if (page->name == "Page1" &&
                    decoration == mfd::PageTitleDecoration::Frame &&
                    tutorial_->MatchesTarget("page_title_decoration_frame"))
                {
                    tutorial_->CompleteStep();
                }
            }
            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }
            if (decoration == mfd::PageTitleDecoration::Frame)
            {
                tutorial_->DrawHalo(
                    "page_title_decoration_frame",
                    "Choose Frame",
                    "Frame the Page1 title so the generated chrome becomes a boxed heading. You can still tune its color, line style, and transform afterwards.");
                if (selected &&
                    page->name == "Page1" &&
                    tutorial_->MatchesTarget("page_title_decoration_frame"))
                {
                    tutorial_->CompleteStep();
                }
            }
        }

        ImGui::EndCombo();
    }
    ShowItemTooltip("Choose whether the title is underlined, boxed, or rendered without decoration.");

    ImVec4 color = ToImGuiColor(titleDisplay->color);
    if (ImGui::ColorEdit4("Color", &color.x))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        titleDisplay->color = ToColorRgba(color);
    }
    ShowItemTooltip("Shared color applied to the title text and its decoration.");

    ImGui::BeginDisabled(titleDisplay->decoration == mfd::PageTitleDecoration::None);
    const char* currentLineStyle = LineStyleLabel(titleDisplay->lineStyle);
    if (ImGui::BeginCombo("Line style", currentLineStyle))
    {
        const std::array lineStyles {
            mfd::LineStyle::Solid,
            mfd::LineStyle::Dotted,
            mfd::LineStyle::Dashed};

        for (const mfd::LineStyle lineStyle : lineStyles)
        {
            const bool selected = titleDisplay->lineStyle == lineStyle;
            if (ImGui::Selectable(LineStyleLabel(lineStyle), selected) && !selected)
            {
                PushUndoSnapshot();
                titleDisplay->lineStyle = lineStyle;
            }
            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }

        ImGui::EndCombo();
    }
    ShowItemTooltip("Outline pattern used by the underline or frame.");

    float lineWidth = titleDisplay->lineWidth;
    if (ImGui::DragFloat("Line width", &lineWidth, 0.0002f, 0.0005f, 0.05f, "%.4f"))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        titleDisplay->lineWidth = std::max(0.0005f, lineWidth);
    }
    ShowItemTooltip("Stroke thickness used by the underline or the frame.");
    ImGui::EndDisabled();
    }

    if (BeginInspectorSection("section_title_transform", "Transform", true, sectionForceOpen))
    {
    const bool positionChanged =
        ImGui::DragFloat2("Position", &titleDisplay->transform.position.x, 0.01f, -4.0f, 4.0f, "%.3f");
    ShowItemTooltip("Logical page-space anchor of the title chrome.");
    if (ImGui::IsItemActivated())
    {
        PushUndoSnapshot();
    }
    if (positionChanged)
    {
        if (!std::isfinite(titleDisplay->transform.position.x))
        {
            titleDisplay->transform.position.x = 0.0f;
        }
        if (!std::isfinite(titleDisplay->transform.position.y))
        {
            titleDisplay->transform.position.y = 0.0f;
        }
    }

    const mfd::Transform2D rotationStartTransform = titleDisplay->transform;
    if (ImGui::DragFloat("Rotation", &titleDisplay->transform.rotationDegrees, 0.25f, -360.0f, 360.0f, "%.2f"))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        titleDisplay->transform = BuildTransformKeepingLocalPointWorldPosition(
            rotationStartTransform,
            PageTitleVisualCenterLocal(*page),
            titleDisplay->transform.rotationDegrees,
            rotationStartTransform.scale);
    }
    ShowItemTooltip("Rotation in degrees around the title chrome visual center.");

    const mfd::Transform2D scaleStartTransform = titleDisplay->transform;
    if (ImGui::DragFloat2("Scale", &titleDisplay->transform.scale.x, 0.01f, 0.05f, 10.0f, "%.3f"))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        titleDisplay->transform.scale.x = std::max(0.05f, std::abs(titleDisplay->transform.scale.x));
        titleDisplay->transform.scale.y = std::max(0.05f, std::abs(titleDisplay->transform.scale.y));
        titleDisplay->transform = BuildTransformKeepingLocalPointWorldPosition(
            scaleStartTransform,
            PageTitleVisualCenterLocal(*page),
            scaleStartTransform.rotationDegrees,
            titleDisplay->transform.scale);
    }
    ShowItemTooltip("Per-axis scale applied to the generated title chrome.");
    }
}

void EditorApplication::ApplySelectedPageStrobeClipping(mfd::ReticleGroup& reticle,
                                                        const mfd::ReticleClipMode mode,
                                                        std::string primitiveId)
{
    if (primitiveId.empty())
    {
        primitiveId = reticle.clipping.primitiveId;
    }

    if (reticle.clipping.mode == mode && reticle.clipping.primitiveId == primitiveId)
    {
        return;
    }

    PushUndoSnapshot();
    reticle.clipping.mode = mode;
    reticle.clipping.primitiveId = std::move(primitiveId);
    if (mode == mfd::ReticleClipMode::None)
    {
        RebuildStatus("Clipping disabled for page strobe '" + reticle.id + "'.", false);
    }
    else
    {
        RebuildStatus(std::string(ReticleClipModeLabel(mode)) + " enabled on primitive '" +
                          reticle.clipping.primitiveId + "' for page strobe '" + reticle.id + "'.",
                      false);
    }
}

void EditorApplication::DrawSelectedPageStrobeInspector()
{
    mfd::PageDefinition* page = ActivePage();
    mfd::ReticleGroup* reticle = SelectedPageStrobeReticle();
    const mfd::PageStrobeDefinition* strobe = SelectedPageStrobe();
    if (page == nullptr || reticle == nullptr || strobe == nullptr)
    {
        ImGui::TextDisabled("No page strobe selected.");
        return;
    }

    ImGui::TextColored(ImVec4(0.33f, 0.86f, 0.78f, 1.0f), "Page strobe");
    ImGui::Text("Page: %s", page->name.c_str());
    ImGui::Text("Name: %s", strobe->name.c_str());
    ImGui::Text("Strobe id: %s", reticle->id.c_str());
    if (!reticle->sourceTemplateId.empty())
    {
        ImGui::TextDisabled("Template: %s", reticle->sourceTemplateId.c_str());
    }
    InspectorHelpMarker(
        "Move inside the frame, rotate with the blue handle, scale with the corner handles.\n"
        "Del removes the page strobe, Esc clears the selection. The strobe renders after regular page reticles.");
    ImGui::Separator();

    const bool sectionForceOpen = tutorial_->IsCoachVisible();

    if (!reticle->sourceTemplateId.empty() &&
        documentState_.loaded.document.reticleLibrary.find(reticle->sourceTemplateId) != documentState_.loaded.document.reticleLibrary.end() &&
        ImGui::Button("Edit source template"))
    {
        SelectLibraryReticle(reticle->sourceTemplateId);
        RebuildStatus("Editing template '" + reticle->sourceTemplateId + "' in the reticle studio.", false);
        return;
    }
    if (!reticle->sourceTemplateId.empty() &&
        documentState_.loaded.document.reticleLibrary.find(reticle->sourceTemplateId) != documentState_.loaded.document.reticleLibrary.end())
    {
        ShowItemTooltip("Open the shared template that this page strobe instance was created from.");
        ImGui::SameLine();
    }

    if (BeginInspectorSection("section_strobe_display", "Display & blink", true, sectionForceOpen))
    {
        bool visible = reticle->visible;
        if (ImGui::Checkbox("Visible", &visible))
        {
            PushUndoSnapshot();
            reticle->visible = visible;
        }
        ShowItemTooltip("Toggle whether this page strobe instance is rendered.");

        DrawPageReticleBlinkInspector(*page, *reticle);
    }

    if (BeginInspectorSection("section_strobe_clipping", "Clipping", false, sectionForceOpen))
    {
    const std::vector<ClipPrimitiveOption> clipOptions = CollectClipPrimitiveOptions(*reticle);

    if (clipOptions.empty())
    {
        ImGui::TextDisabled("No supported convex primitive with an id is available for clipping.");
        ImGui::TextDisabled("Supported mask shapes: triangle, square, rectangle, circle, ellipse.");
    }
    else
    {
        std::string currentClipPrimitiveLabel = reticle->clipping.primitiveId.empty()
                                                    ? std::string {"<select primitive>"}
                                                    : std::string {"<missing primitive>"};
        for (const auto& option : clipOptions)
        {
            if (option.primitiveId == reticle->clipping.primitiveId)
            {
                currentClipPrimitiveLabel = option.label;
                break;
            }
        }

        if (ImGui::BeginCombo("Clip primitive", currentClipPrimitiveLabel.c_str()))
        {
            for (const auto& option : clipOptions)
            {
                const bool selected = option.primitiveId == reticle->clipping.primitiveId;
                if (ImGui::Selectable(option.label.c_str(), selected))
                {
                    ApplySelectedPageStrobeClipping(*reticle, reticle->clipping.mode, option.primitiveId);
                }
                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }
        ShowItemTooltip("Choose which convex primitive erases the inside or the outside toward the page background.");

        const char* currentModeLabel = ReticleClipModeLabel(reticle->clipping.mode);
        if (ImGui::BeginCombo("Clip mode", currentModeLabel))
        {
            const std::array modes {
                mfd::ReticleClipMode::None,
                mfd::ReticleClipMode::Inner,
                mfd::ReticleClipMode::Outer};

            for (const mfd::ReticleClipMode mode : modes)
            {
                const bool selected = reticle->clipping.mode == mode;
                if (ImGui::Selectable(ReticleClipModeLabel(mode), selected))
                {
                    const std::string primitiveId =
                        reticle->clipping.primitiveId.empty() ? clipOptions.front().primitiveId : reticle->clipping.primitiveId;
                    ApplySelectedPageStrobeClipping(*reticle, mode, primitiveId);
                }
                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }
        ShowItemTooltip(
            "Inner clipping erases the inside of the selected shape. Outer clipping erases everything outside it.");

        if (reticle->clipping.mode != mfd::ReticleClipMode::None && mfd::ResolveClipPrimitive(*reticle) == nullptr)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.42f, 1.0f), "The current clip primitive is missing or unsupported.");
        }

        ImGui::TextDisabled("The selected primitive erases toward the page background when this strobe is drawn.");
    }
    }

    if (BeginInspectorSection("section_strobe_transform", "Transform & style", true, sectionForceOpen))
    {
    const bool positionChanged = ImGui::DragFloat2("Position", &reticle->transform.position.x, 0.01f, -1.0f, 1.0f, "%.3f");
    ShowItemTooltip("Logical position of this page strobe on the active page.");
    if (ImGui::IsItemActivated())
    {
        PushUndoSnapshot();
    }
    if (positionChanged)
    {
        reticle->transform.position.x = std::clamp(reticle->transform.position.x, -1.0f, 1.0f);
        reticle->transform.position.y = std::clamp(reticle->transform.position.y, -1.0f, 1.0f);
    }

    const mfd::Transform2D rotationStartTransform = reticle->transform;
    if (ImGui::DragFloat("Rotation", &reticle->transform.rotationDegrees, 0.25f, -360.0f, 360.0f, "%.2f"))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        reticle->transform = BuildTransformKeepingLocalPointWorldPosition(
            rotationStartTransform,
            ReticleVisualCenterLocal(*reticle),
            reticle->transform.rotationDegrees,
            rotationStartTransform.scale);
    }
    ShowItemTooltip("Rotation in degrees around the strobe visual center.");

    const mfd::Transform2D scaleStartTransform = reticle->transform;
    if (ImGui::DragFloat2("Scale", &reticle->transform.scale.x, 0.01f, 0.05f, 10.0f, "%.3f"))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        reticle->transform.scale.x = std::max(0.05f, reticle->transform.scale.x);
        reticle->transform.scale.y = std::max(0.05f, reticle->transform.scale.y);
        reticle->transform = BuildTransformKeepingLocalPointWorldPosition(
            scaleStartTransform,
            ReticleVisualCenterLocal(*reticle),
            scaleStartTransform.rotationDegrees,
            reticle->transform.scale);
    }
    ShowItemTooltip("Per-axis scale applied to this page strobe instance.");

    ImVec4 stroke = ToImGuiColor(reticle->overrides.color.value_or(mfd::ColorRgba {0, 255, 102, 255}));
    if (ImGui::ColorEdit4("Stroke", &stroke.x))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        reticle->overrides.color = ToColorRgba(stroke);
    }
    ShowItemTooltip("Override the template stroke color for this page strobe instance.");

    float thickness = reticle->overrides.thickness.value_or(0.0042f);
    if (ImGui::DragFloat("Thickness", &thickness, 0.0002f, 0.0005f, 0.05f, "%.4f"))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        reticle->overrides.thickness = std::max(0.0005f, thickness);
    }
    ShowItemTooltip("Override the template stroke thickness for this page strobe instance.");
    }

    if (BeginInspectorSection("section_strobe_text", "Text & time overrides", true, sectionForceOpen))
    {
    int unnamedTextPrimitiveIndex = 0;
    int unnamedTimePrimitiveIndex = 0;
    for (int primitiveIndex = 0; primitiveIndex < static_cast<int>(reticle->primitives.size()); ++primitiveIndex)
    {
        auto& primitive = reticle->primitives[static_cast<std::size_t>(primitiveIndex)];
        auto* text = std::get_if<mfd::TextGeometry>(&primitive.geometry);
        auto* time = std::get_if<mfd::TimeGeometry>(&primitive.geometry);
        if (text == nullptr && time == nullptr)
        {
            continue;
        }

        if (text != nullptr)
        {
            const bool hasPrimitiveId = !primitive.id.empty();
            const int fallbackIndex = unnamedTextPrimitiveIndex++;
            std::array<char, 128> buffer {};
            CopyTextBuffer(buffer, text->text);
            const std::string label = hasPrimitiveId ? "Text##strobe_" + primitive.id
                                                     : "Text #" + std::to_string(fallbackIndex) + "##strobe_text_" +
                                                           std::to_string(primitiveIndex);
            const bool changed = ImGui::InputText(label.c_str(), buffer.data(), buffer.size());
            ShowItemTooltip("Override the literal text for this text primitive on the page strobe instance.");
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            if (changed)
            {
                text->text = buffer.data();
            }

            const std::string alignLabel = hasPrimitiveId ? "Alignment##strobe_" + primitive.id
                                                          : "Alignment #" + std::to_string(fallbackIndex) +
                                                                "##strobe_text_align_" + std::to_string(primitiveIndex);
            if (ImGui::BeginCombo(alignLabel.c_str(), AlignLabel(text->align)))
            {
                for (const mfd::Align candidate : kSupportedTextAlignments)
                {
                    const bool selected = text->align == candidate;
                    if (ImGui::Selectable(AlignLabel(candidate), selected) && !selected)
                    {
                        PushUndoSnapshot();
                        ApplyPrimitiveTextAlignmentChange(
                            primitive,
                            *text,
                            candidate,
                            MeasurePreviewTextWidthLogical(*text));
                    }
                    if (selected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }

                ImGui::EndCombo();
            }
            ShowItemTooltip("Choose whether this text grows left, right, or stays centered around its current anchor.");

            float letterSpacing = text->letterSpacing;
            const std::string spacingLabel = hasPrimitiveId ? "Letter spacing##strobe_" + primitive.id
                                                            : "Letter spacing #" + std::to_string(fallbackIndex) +
                                                                  "##strobe_text_spacing_" + std::to_string(primitiveIndex);
            const bool spacingChanged =
                ImGui::DragFloat(spacingLabel.c_str(), &letterSpacing, 0.0005f, -0.05f, 0.10f, "%.4f");
            ShowItemTooltip("Override the letter spacing for this text primitive on the page strobe instance.");
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            if (spacingChanged)
            {
                text->letterSpacing = letterSpacing;
            }
            continue;
        }

        if (time != nullptr)
        {
            const bool hasPrimitiveId = !primitive.id.empty();
            const int fallbackIndex = unnamedTimePrimitiveIndex++;
            const std::string formatLabel = hasPrimitiveId ? "Time format##strobe_" + primitive.id
                                                           : "Time format #" + std::to_string(fallbackIndex) +
                                                                 "##strobe_time_" + std::to_string(primitiveIndex);
            EditTimeFormatField(formatLabel,
                                "Override the strftime-style format used by this time primitive on the page strobe. "
                                "Changes apply when the field loses focus and invalid directives are rejected.",
                                *time);

            const std::string alignLabel = hasPrimitiveId ? "Alignment##strobe_" + primitive.id
                                                          : "Alignment #" + std::to_string(fallbackIndex) +
                                                                "##strobe_time_align_" + std::to_string(primitiveIndex);
            if (ImGui::BeginCombo(alignLabel.c_str(), AlignLabel(time->align)))
            {
                for (const mfd::Align candidate : kSupportedTextAlignments)
                {
                    const bool selected = time->align == candidate;
                    if (ImGui::Selectable(AlignLabel(candidate), selected) && !selected)
                    {
                        PushUndoSnapshot();
                        ApplyPrimitiveTextAlignmentChange(
                            primitive,
                            *time,
                            candidate,
                            MeasurePreviewTextWidthLogical(*time));
                    }
                    if (selected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }

                ImGui::EndCombo();
            }
            ShowItemTooltip("Choose whether this time display grows left, right, or stays centered around its current anchor.");

            bool utc = time->utc;
            const std::string utcLabel = hasPrimitiveId ? "UTC##strobe_" + primitive.id
                                                        : "UTC #" + std::to_string(fallbackIndex) + "##strobe_time_utc_" +
                                                              std::to_string(primitiveIndex);
            if (ImGui::Checkbox(utcLabel.c_str(), &utc))
            {
                PushUndoSnapshot();
                time->utc = utc;
            }
            ShowItemTooltip("Render this time primitive in UTC instead of local time.");

            float letterSpacing = time->letterSpacing;
            const std::string spacingLabel = hasPrimitiveId ? "Letter spacing##strobe_" + primitive.id
                                                            : "Letter spacing #" + std::to_string(fallbackIndex) +
                                                                  "##strobe_time_spacing_" +
                                                                  std::to_string(primitiveIndex);
            const bool spacingChanged =
                ImGui::DragFloat(spacingLabel.c_str(), &letterSpacing, 0.0005f, -0.05f, 0.10f, "%.4f");
            ShowItemTooltip("Override the character spacing for this time primitive on the page strobe instance.");
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            if (spacingChanged)
            {
                time->letterSpacing = letterSpacing;
            }
        }
    }
    }

    if (BeginInspectorSection("section_strobe_behaviour", "Strobe behaviour", true, sectionForceOpen))
    {
        DrawPageStrobeInspector(*page);
    }
}

void EditorApplication::DrawPageReticleBlinkInspector(mfd::PageDefinition& page, mfd::ReticleGroup& reticle)
{
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.96f, 0.81f, 0.52f, 1.0f), "Blink");
    ImGui::TextDisabled("Blink is managed by the page, not by the reticle template.");

    if (page.blinkTypes.empty())
    {
        ImGui::TextDisabled("This page has no blink type yet. Add one in the page inspector.");
        return;
    }

    bool blinkEnabled = reticle.blink.enabled;
    if (ImGui::Checkbox("Blink enabled", &blinkEnabled))
    {
        PushUndoSnapshot();
        if (!blinkEnabled)
        {
            reticle.blink = {};
        }
        else
        {
            reticle.blink.enabled = true;
        }

        RefreshPageBlinkStateForEditor(page);
    }
    ShowItemTooltip("Enable or disable blinking for this page reticle instance.");

    if (!reticle.blink.enabled)
    {
        ImGui::TextDisabled("Enable blink to use the page default or pick a named page type.");
        return;
    }

    const std::size_t effectiveDefaultIndex = EffectiveDefaultBlinkTypeIndex(page);
    const std::string defaultBlinkName =
        effectiveDefaultIndex == kInvalidBlinkTypeIndex ? std::string {"<none>"} : page.blinkTypes[effectiveDefaultIndex].name;
    const std::string currentSelection =
        reticle.blink.typeName.empty() ? "<page default: " + defaultBlinkName + ">" : reticle.blink.typeName;

    if (ImGui::BeginCombo("Blink type", currentSelection.c_str()))
    {
        const std::string defaultItem = "<page default> - " + defaultBlinkName;
        const bool defaultSelected = reticle.blink.typeName.empty();
        if (ImGui::Selectable(defaultItem.c_str(), defaultSelected))
        {
            PushUndoSnapshot();
            reticle.blink.enabled = true;
            reticle.blink.typeName.clear();
            reticle.blink.normalizedTypeName.clear();
            RefreshPageBlinkStateForEditor(page);
        }
        if (defaultSelected)
        {
            ImGui::SetItemDefaultFocus();
        }

        for (const auto& blinkType : page.blinkTypes)
        {
            const bool selected = reticle.blink.normalizedTypeName == blinkType.normalizedName;
            const std::string label =
                blinkType.name + " - " + std::to_string(blinkType.durationMs) + " ms";
            if (ImGui::Selectable(label.c_str(), selected))
            {
                PushUndoSnapshot();
                reticle.blink.enabled = true;
                reticle.blink.typeName = blinkType.name;
                reticle.blink.normalizedTypeName = blinkType.normalizedName;
                RefreshPageBlinkStateForEditor(page);
            }
        }

        ImGui::EndCombo();
    }
    ShowItemTooltip("Choose a page-local blink type, or keep using the page default blink.");

    ImGui::TextDisabled("Current effective duration: %u ms", reticle.blink.durationMs);
}

