/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorApplication.h"

/**
 * @file
 * @brief Library reticle and library primitive inspector implementation extracted from `EditorApplicationInspectors`.
 */

#include <algorithm>
#include <array>
#include <cstdint>

#include "internal/application/EditorApplicationInternal.h"
#include "EditorTutorialController.h"
#include "EditorUiTheme.h"
#include "internal/application/EditorApplicationAuthoringSupport.h"

namespace
{
using editor::ui::AccentButton;
using editor::ui::BeginInspectorSection;
using editor::ui::InspectorHelpMarker;
using editor::ui::ShowItemTooltip;
using editor::detail::CopyTextBuffer;
using editor::detail::kPrimitiveTypes;
using editor::detail::ReticleHasFillCapablePrimitive;
using editor::detail::SeedPrimitiveFillColorIfNeeded;
using editor::detail::SeedReticleFillOverrideIfNeeded;
using editor::detail::ToColorRgba;
using editor::detail::VisibleFillColorFromStroke;
using editor::app::LineStyleLabel;
using editor::app::PrimitiveTypeLabel;
using editor::app::SupportsPrimitiveLineStyle;
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

class ScopedImGuiId
{
public:
    explicit ScopedImGuiId(const char* id)
    {
        ImGui::PushID(id);
    }

    ~ScopedImGuiId()
    {
        ImGui::PopID();
    }
};

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
}

void EditorApplication::DrawLibraryReticleInspector()
{
    mfd::ReticleGroup* reticle = SelectedLibraryReticle();
    if (reticle == nullptr)
    {
        ImGui::TextDisabled("Select a library reticle.");
        return;
    }

    const ScopedImGuiId scopedId("LibraryReticleInspector");

    ImGui::TextColored(ImVec4(0.33f, 0.86f, 0.78f, 1.0f), "Library reticle");
    ImGui::Text("Template id: %s", reticle->id.c_str());
    if (const auto fileIt = documentState_.files.templateFiles.find(reticle->id); fileIt != documentState_.files.templateFiles.end())
    {
        ImGui::TextDisabled("File: %s", fileIt->second.filename().string().c_str());
    }
    InspectorHelpMarker(
        "Drag this reticle from the library tree to the page preview, or edit it directly in the reticle studio.\n"
        "Ctrl+C / Ctrl+V copies and pastes the template while it stays focused.");

    const bool sectionForceOpen = tutorial_->IsCoachVisible();

    const bool canAddToPage = ActivePage() != nullptr;
    ImGui::BeginDisabled(!canAddToPage);
    if (AccentButton("Add to active page"))
    {
        const bool tutorialAddMatched = tutorial_->MatchesTarget("library_add_to_page");
        const mfd::PageDefinition* page = ActivePage();
        const mfd::Vec2 dropPosition = page == nullptr ? mfd::Vec2 {} : layoutState_.pagePreviewView.center;
        std::string tutorialError;
        if (tutorialAddMatched && !tutorial_->ValidateAddToPage(page, *reticle, tutorialError))
        {
            RebuildStatus(tutorialError, true);
            ImGui::EndDisabled();
            return;
        }
        if (CreatePageReticleInstanceFromTemplate(reticle->id, dropPosition) && tutorialAddMatched)
        {
            if (const mfd::ReticleGroup* createdReticle = SelectedPageReticle(); createdReticle != nullptr)
            {
                tutorial_->SetTrackedReticleId(createdReticle->id);
            }
            tutorial_->CompleteStep();
        }
    }
    ImGui::EndDisabled();
    ShowItemTooltip("Instantiate this template on the active page at the current editor camera center.");
    tutorial_->DrawHalo(
        "library_add_to_page",
        "Click Add to active page",
        tutorial_->LibraryAddToPageHaloReason());

    ImGui::SameLine();
    if (ImGui::Button("More..."))
    {
        ImGui::OpenPopup("LibraryReticleActionsMenu");
    }
    ShowItemTooltip("Copy, paste, rename or delete this shared reticle template.");

    if (ImGui::BeginPopup("LibraryReticleActionsMenu"))
    {
        if (ImGui::MenuItem("Copy"))
        {
            CopySelectedLibraryReticle();
        }
        if (ImGui::MenuItem("Paste copy", nullptr, false, clipboardState_.libraryReticleClipboard.has_value()))
        {
            PasteCopiedLibraryReticle();
            ImGui::EndPopup();
            return;
        }
        if (ImGui::MenuItem("Rename globally..."))
        {
            OpenReticleRenamePopup(reticle->id);
            ImGui::EndPopup();
            return;
        }
        if (ImGui::MenuItem("Delete library reticle"))
        {
            DeleteSelectedLibraryReticle();
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }

    if (BeginInspectorSection("section_library_reticle_defaults", "Default appearance", true, sectionForceOpen))
    {
    {
        bool visible = reticle->visible;
        if (ImGui::Checkbox("Visible", &visible))
        {
            PushUndoSnapshot();
            reticle->visible = visible;
        }
        ShowItemTooltip("Toggle whether this template is visible by default.");
    }

    {
        bool drawOnTop = reticle->drawOnTop;
        if (ImGui::Checkbox("Draw on top", &drawOnTop))
        {
            PushUndoSnapshot();
            reticle->drawOnTop = drawOnTop;
        }
        ShowItemTooltip("Default draw tier used when this template is instantiated on a page.");
    }

    if (ImGui::DragFloat2("Position", &reticle->transform.position.x, 0.01f, -1.0f, 1.0f, "%.3f"))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
    }
    ShowItemTooltip("Default logical position inside the reticle template.");

    if (ImGui::DragFloat("Rotation", &reticle->transform.rotationDegrees, 0.25f, -360.0f, 360.0f, "%.2f"))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
    }
    ShowItemTooltip("Default template rotation in degrees.");

    if (ImGui::DragFloat2("Scale", &reticle->transform.scale.x, 0.01f, 0.05f, 10.0f, "%.3f"))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        reticle->transform.scale.x = std::max(0.05f, reticle->transform.scale.x);
        reticle->transform.scale.y = std::max(0.05f, reticle->transform.scale.y);
    }
    ShowItemTooltip("Default per-axis scale applied to this template.");

    ImVec4 stroke = ToImGuiColor(reticle->overrides.color.value_or(mfd::ColorRgba {0, 255, 102, 255}));
    if (ImGui::ColorEdit4("Default stroke", &stroke.x))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        reticle->overrides.color = ToColorRgba(stroke);
    }
    ShowItemTooltip("Default stroke color inherited by page instances unless they override it.");

    float thickness = reticle->overrides.thickness.value_or(0.0042f);
    if (ImGui::DragFloat("Default thickness", &thickness, 0.0002f, 0.0005f, 0.05f, "%.4f"))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        reticle->overrides.thickness = std::max(0.0005f, thickness);
    }
    ShowItemTooltip("Default stroke thickness inherited by page instances unless they override it.");

    if (ReticleHasFillCapablePrimitive(*reticle))
    {
        const mfd::ColorRgba fallbackFillColor =
            VisibleFillColorFromStroke(reticle->overrides.color.value_or(mfd::PrimitiveStyle {}.color));
        ImVec4 fill = ToImGuiColor(reticle->overrides.fillColor.value_or(fallbackFillColor));
        if (ImGui::ColorEdit4("Default fill", &fill.x))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            reticle->overrides.fillColor = ToColorRgba(fill);
        }
        ShowItemTooltip("Default fill color inherited by fill-capable primitives unless they override it locally.");

        bool filled = reticle->overrides.filled.value_or(false);
        if (ImGui::Checkbox("Default filled", &filled))
        {
            PushUndoSnapshot();
            reticle->overrides.filled = filled;
            SeedReticleFillOverrideIfNeeded(
                reticle->overrides,
                reticle->overrides.color.value_or(mfd::PrimitiveStyle {}.color));
        }
        ShowItemTooltip("Default filled state inherited by fill-capable primitives unless they override it locally.");
    }

    }

    if (BeginInspectorSection("section_library_reticle_primitives", "Primitives", true, sectionForceOpen))
    {
    InspectorHelpMarker("Click a primitive below or directly in the studio preview to focus and edit it.");

    ImGui::BeginChild("PrimitiveCatalog", ImVec2(0.0f, 210.0f), true);
    for (int index = 0; index < static_cast<int>(reticle->primitives.size()); ++index)
    {
        const auto& primitive = reticle->primitives[static_cast<std::size_t>(index)];
        const bool selected = documentState_.selection.kind == SelectionKind::LibraryPrimitive &&
                              documentState_.selection.libraryReticleId == reticle->id &&
                              documentState_.selection.primitiveIndex == index;
        const std::string header =
            std::to_string(index + 1) + ". " +
            (primitive.id.empty() ? PrimitiveTypeLabel(primitive.type) : primitive.id);
        if (ImGui::Selectable((header + "##primitive_" + std::to_string(index)).c_str(), selected))
        {
            SelectLibraryPrimitive(reticle->id, index);
        }
        ShowItemTooltip("Click to focus this primitive in the inspector and the reticle studio.");

        ImGui::SameLine();
        ImGui::TextDisabled("[%s]", PrimitiveTypeLabel(primitive.type).c_str());
    }
    ImGui::EndChild();

    const bool hasSelectedPrimitive = documentState_.selection.kind == SelectionKind::LibraryPrimitive &&
                                      documentState_.selection.libraryReticleId == reticle->id &&
                                      SelectedLibraryPrimitive() != nullptr;
    ImGui::BeginDisabled(!hasSelectedPrimitive);
    if (AccentButton("Copy selected primitive"))
    {
        CopySelectedLibraryPrimitive();
    }
    ShowItemTooltip("Copy the focused primitive into the reticle-studio clipboard.");
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!clipboardState_.libraryPrimitiveClipboard.has_value());
    if (ImGui::Button("Paste copied primitive"))
    {
        PasteCopiedLibraryPrimitive();
    }
    ShowItemTooltip("Paste the copied primitive into this reticle template.");
    ImGui::EndDisabled();

    if (hasSelectedPrimitive || clipboardState_.libraryPrimitiveClipboard.has_value())
    {
        ImGui::TextDisabled("Primitive shortcuts: Ctrl+C / Ctrl+V");
    }

    if (ImGui::BeginCombo("Add primitive", PrimitiveTypeLabel(kPrimitiveTypes[static_cast<std::size_t>(workflowState_.newLibraryReticleDraft.primitiveTypeIndex)]).c_str()))
    {
        for (int index = 0; index < static_cast<int>(kPrimitiveTypes.size()); ++index)
        {
            if (ImGui::Selectable(PrimitiveTypeLabel(kPrimitiveTypes[static_cast<std::size_t>(index)]).c_str(),
                                  workflowState_.newLibraryReticleDraft.primitiveTypeIndex == index))
            {
                workflowState_.newLibraryReticleDraft.primitiveTypeIndex = index;
            }
        }
        ImGui::EndCombo();
    }
    ShowItemTooltip("Choose the primitive type that will be appended to this reticle.");

    if (AccentButton("Append primitive"))
    {
        const bool tutorialAppendMatched = tutorial_->MatchesTarget("library_append_primitive");
        const mfd::PrimitiveType primitiveType =
            kPrimitiveTypes[static_cast<std::size_t>(workflowState_.newLibraryReticleDraft.primitiveTypeIndex)];
        if (tutorialAppendMatched)
        {
            std::string tutorialError;
            if (!tutorial_->ValidateAppendPrimitive(*reticle, primitiveType, tutorialError))
            {
                RebuildStatus(tutorialError, true);
                return;
            }
        }

        PushUndoSnapshot();
        mfd::ReticleGroup seed = MakePrimitiveReticle("seed", primitiveType);
        mfd::Primitive primitive = seed.primitives.front();
        primitive.id = "primitive_" + std::to_string(reticle->primitives.size() + 1);
        if (tutorialAppendMatched)
        {
            tutorial_->ConfigureAppendedPrimitive(primitive);
        }
        reticle->primitives.push_back(std::move(primitive));
        SelectLibraryPrimitive(reticle->id, static_cast<int>(reticle->primitives.size()) - 1);
        if (tutorialAppendMatched)
        {
            tutorial_->CompleteStep();
        }
    }
    ShowItemTooltip("Append a new primitive of the selected type to this reticle.");
    tutorial_->DrawHalo(
        "library_append_primitive",
        "Click Append primitive",
        tutorial_->LibraryAppendPrimitiveHaloReason());

    ImGui::SameLine();
    if (ImGui::Button("Remove selected primitive"))
    {
        mfd::Primitive* primitive = SelectedLibraryPrimitive();
        if (primitive != nullptr)
        {
            PushUndoSnapshot();
            reticle->primitives.erase(reticle->primitives.begin() + documentState_.selection.primitiveIndex);
            documentState_.selection.kind = SelectionKind::LibraryReticle;
            documentState_.selection.primitiveIndex = -1;
        }
    }
    ShowItemTooltip("Delete the currently selected primitive from this reticle.");
    }
}

void EditorApplication::EditPointArrayField(const char* const label, mfd::Vec2& value)
{
    if (ImGui::DragFloat2(label, &value.x, 0.01f, -1.0f, 1.0f, "%.3f"))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
    }
}

void EditorApplication::DrawLibraryPrimitiveInspector()
{
    mfd::ReticleGroup* reticle = SelectedLibraryReticle();
    mfd::Primitive* primitive = SelectedLibraryPrimitive();
    if (reticle == nullptr || primitive == nullptr)
    {
        ImGui::TextDisabled("Select a primitive inside a library reticle.");
        return;
    }

    const ScopedImGuiId scopedId("LibraryPrimitiveInspector");
    const bool tutorialExposedPrimitiveSelected =
        tutorial_->IsExposedPrimitiveTutorialSelection(documentState_.selection.libraryReticleId, primitive->id);
    const bool tutorialAlternativeStrobeLabelSelected =
        tutorial_->IsAlternativeStrobeLabelSelection(documentState_.selection.libraryReticleId, primitive->id);

    const bool sectionForceOpen = tutorial_->IsCoachVisible();

    ImGui::TextColored(ImVec4(0.33f, 0.86f, 0.78f, 1.0f), "Primitive");
    if (AccentButton("Copy primitive"))
    {
        CopySelectedLibraryPrimitive();
    }
    ShowItemTooltip("Copy this primitive into the reticle-studio clipboard.");

    ImGui::SameLine();
    ImGui::BeginDisabled(!clipboardState_.libraryPrimitiveClipboard.has_value());
    if (ImGui::Button("Paste copied primitive"))
    {
        PasteCopiedLibraryPrimitive();
        ImGui::EndDisabled();
        return;
    }
    ShowItemTooltip("Paste the copied primitive into the current reticle template.");
    ImGui::EndDisabled();
    InspectorHelpMarker(
        "Ctrl+C / Ctrl+V copies and pastes this primitive.\n"
        "In the studio: the green handle moves the primitive, the orange handles edit its geometry.");

    std::array<char, 128> primitiveId {};
    CopyTextBuffer(primitiveId, primitive->id);
    const bool idChanged = ImGui::InputText("Primitive id", primitiveId.data(), primitiveId.size());
    ShowItemTooltip("Primitive identifier stored in the template JSON.");
    if (ImGui::IsItemActivated())
    {
        PushUndoSnapshot();
    }
    if (idChanged)
    {
        primitive->id = primitiveId.data();
    }

    ImGui::TextDisabled("Type: %s", PrimitiveTypeLabel(primitive->type).c_str());

    if (BeginInspectorSection("section_primitive_behaviour", "Visibility & generated API", true, sectionForceOpen))
    {
    {
        bool visible = primitive->style.visible;
        if (ImGui::Checkbox("Visible", &visible))
        {
            PushUndoSnapshot();
            primitive->style.visible = visible;
        }
        ShowItemTooltip("Toggle whether this primitive is rendered inside the template.");
    }

    {
        bool exposed = primitive->exposed;
        if (ImGui::Checkbox("Exposed", &exposed))
        {
            PushUndoSnapshot();
            primitive->exposed = exposed;

            if (exposed && tutorial_->MatchesTarget("primitive_exposed_checkbox") && tutorialExposedPrimitiveSelected)
            {
                tutorial_->CompleteStep();
            }
        }
        ShowItemTooltip("Expose this primitive through the generated client API so runtime code can drive it directly.");
        if (tutorialExposedPrimitiveSelected)
        {
            tutorial_->DrawHalo(
                "primitive_exposed_checkbox",
                "Enable Exposed",
                tutorialAlternativeStrobeLabelSelected
                    ? "Expose the aircraft label so the generated API can mutate one primitive on the active Page1 strobe."
                    : "Expose the fill rectangle so the generated API can animate the progress bar without raw ids.");
        }
    }

    {
        bool rotationSensitive = primitive->reticleRotationSensitive;
        if (ImGui::Checkbox("Affected by reticle rotation", &rotationSensitive))
        {
            PushUndoSnapshot();
            primitive->reticleRotationSensitive = rotationSensitive;

            if (!rotationSensitive &&
                tutorial_->MatchesTarget("primitive_reticle_rotation_checkbox") &&
                tutorial_->IsAlternativeStrobeLabelSelection(documentState_.selection.libraryReticleId, primitive->id))
            {
                tutorial_->AdvancePhase();
            }
        }
        ShowItemTooltip(
            "When disabled, parent page-reticle or strobe rotation no longer rotates this primitive. "
            "Explicit primitive rotation still applies.");
        if (tutorial_->IsAlternativeStrobeLabelSelection(documentState_.selection.libraryReticleId, primitive->id))
        {
            tutorial_->DrawHalo(
                "primitive_reticle_rotation_checkbox",
                "Disable reticle rotation inheritance",
                "Keep the aircraft label upright even when the alternative Page1 strobe rotates.");
        }
    }

    {
        bool scaleSensitive = primitive->reticleScaleSensitive;
        if (ImGui::Checkbox("Affected by reticle scale", &scaleSensitive))
        {
            PushUndoSnapshot();
            primitive->reticleScaleSensitive = scaleSensitive;

            if (!scaleSensitive &&
                tutorial_->MatchesTarget("primitive_reticle_scale_checkbox") &&
                tutorial_->IsAlternativeStrobeLabelSelection(documentState_.selection.libraryReticleId, primitive->id))
            {
                tutorial_->CompleteStep();
            }
        }
        ShowItemTooltip(
            "When disabled, parent page-reticle or strobe scaling no longer scales this primitive. "
            "Explicit primitive scale still applies.");
        if (tutorial_->IsAlternativeStrobeLabelSelection(documentState_.selection.libraryReticleId, primitive->id))
        {
            tutorial_->DrawHalo(
                "primitive_reticle_scale_checkbox",
                "Disable reticle scale inheritance",
                "Keep the aircraft label size stable even when the alternative Page1 strobe scales.");
        }
    }
    }

    if (BeginInspectorSection("section_primitive_transform", "Transform", true, sectionForceOpen))
    {
    if (ImGui::DragFloat2("Position", &primitive->transform.position.x, 0.01f, -1.0f, 1.0f, "%.3f"))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
    }
    ShowItemTooltip("Logical position of this primitive inside the reticle template.");

    if (ImGui::DragFloat("Rotation", &primitive->transform.rotationDegrees, 0.25f, -360.0f, 360.0f, "%.2f"))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
    }
    ShowItemTooltip("Primitive rotation in degrees.");

    if (ImGui::DragFloat2("Scale", &primitive->transform.scale.x, 0.01f, 0.05f, 10.0f, "%.3f"))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        primitive->transform.scale.x = std::max(0.05f, primitive->transform.scale.x);
        primitive->transform.scale.y = std::max(0.05f, primitive->transform.scale.y);
    }
    ShowItemTooltip("Per-axis scale applied to this primitive.");
    }

    if (BeginInspectorSection("section_primitive_appearance", "Stroke & fill", true, sectionForceOpen))
    {
    ImVec4 stroke = ToImGuiColor(primitive->style.color);
    if (ImGui::ColorEdit4("Stroke", &stroke.x))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        primitive->style.color = ToColorRgba(stroke);
    }
    ShowItemTooltip("Stroke color used to render this primitive.");

    if (ImGui::DragFloat("Thickness", &primitive->style.thickness, 0.0002f, 0.0005f, 0.05f, "%.4f"))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        primitive->style.thickness = std::max(0.0005f, primitive->style.thickness);
    }
    ShowItemTooltip("Stroke thickness used by this primitive.");

    if (SupportsPrimitiveLineStyle(primitive->type))
    {
        if (ImGui::BeginCombo("Line style", LineStyleLabel(primitive->style.lineStyle)))
        {
            constexpr std::array<mfd::LineStyle, 3> kLineStyles {{
                mfd::LineStyle::Solid,
                mfd::LineStyle::Dotted,
                mfd::LineStyle::Dashed}};
            for (const mfd::LineStyle candidate : kLineStyles)
            {
                const bool selected = primitive->style.lineStyle == candidate;
                if (ImGui::Selectable(LineStyleLabel(candidate), selected))
                {
                    PushUndoSnapshot();
                    primitive->style.lineStyle = candidate;
                }
                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ShowItemTooltip("Choose whether this primitive outline is solid, dotted, or dashed.");
    }

    if (mfd::SupportsFilledPrimitive(*primitive))
    {
        const bool fillColorOverridden = reticle->overrides.fillColor.has_value();
        const bool filledStateOverridden = reticle->overrides.filled.has_value();
        if (fillColorOverridden || filledStateOverridden)
        {
            const mfd::PrimitiveStyle effectiveStyle = mfd::MergeStyle(primitive->style, reticle->overrides);
            ImGui::Spacing();
            ImGui::TextDisabled("Effective fill preview");
            if (fillColorOverridden)
            {
                ImGui::TextDisabled("Fill color comes from the reticle default: #%02X%02X%02X%02X",
                                    effectiveStyle.fillColor.r,
                                    effectiveStyle.fillColor.g,
                                    effectiveStyle.fillColor.b,
                                    effectiveStyle.fillColor.a);
            }
            if (filledStateOverridden)
            {
                ImGui::TextDisabled("Filled state comes from the reticle default: %s",
                                    effectiveStyle.filled ? "enabled" : "disabled");
            }
            ImGui::TextDisabled("Edit the reticle Default fill / Default filled controls to change this preview.");
        }

        ImGui::BeginDisabled(fillColorOverridden);
        ImVec4 fill = ToImGuiColor(primitive->style.fillColor);
        if (ImGui::ColorEdit4(fillColorOverridden ? "Local fill" : "Fill", &fill.x))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            primitive->style.fillColor = ToColorRgba(fill);
        }
        ImGui::EndDisabled();
        ShowItemTooltip(fillColorOverridden
                            ? "This primitive fill color is masked by the reticle default fill color."
                            : "Fill color used when this primitive supports filled rendering.");

        ImGui::BeginDisabled(filledStateOverridden);
        bool filled = primitive->style.filled;
        if (ImGui::Checkbox(filledStateOverridden ? "Local filled" : "Filled", &filled))
        {
            PushUndoSnapshot();
            primitive->style.filled = filled;
            SeedPrimitiveFillColorIfNeeded(primitive->style);
        }
        ImGui::EndDisabled();
        ShowItemTooltip(filledStateOverridden
                            ? "This primitive filled state is masked by the reticle default filled state."
                            : "Toggle filled rendering for primitives that support it.");
    }
    }

    if (BeginInspectorSection("section_primitive_geometry", "Geometry", true, sectionForceOpen))
    {
    if (auto* text = std::get_if<mfd::TextGeometry>(&primitive->geometry))
    {
        std::array<char, 128> buffer {};
        CopyTextBuffer(buffer, text->text);
        const bool changed = ImGui::InputText("Text", buffer.data(), buffer.size());
        ShowItemTooltip("Literal text displayed by this text primitive.");
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        if (changed)
        {
            text->text = buffer.data();
        }

        const bool alignmentComboOpen = ImGui::BeginCombo("Alignment", AlignLabel(text->align));
        if (ImGui::IsItemClicked() && tutorial_->MatchesTarget("primitive_alignment"))
        {
            tutorial_->AdvancePhase();
        }
        if (alignmentComboOpen)
        {
            for (const mfd::Align candidate : kSupportedTextAlignments)
            {
                const bool selected = text->align == candidate;
                if (ImGui::Selectable(AlignLabel(candidate), selected))
                {
                    if (!selected)
                    {
                        PushUndoSnapshot();
                        ApplyPrimitiveTextAlignmentChange(
                            *primitive,
                            *text,
                            candidate,
                            MeasurePreviewTextWidthLogical(*text));
                    }

                    if (candidate == mfd::Align::Right &&
                        tutorial_->MatchesTarget("primitive_alignment_right") &&
                        tutorialAlternativeStrobeLabelSelected)
                    {
                        tutorial_->CompleteStep();
                    }
                }
                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
                if (candidate == mfd::Align::Right && tutorialAlternativeStrobeLabelSelected)
                {
                    tutorial_->DrawHalo(
                        "primitive_alignment_right",
                        "Choose Right",
                        "Keep the aircraft label anchored near the triangle while longer captions grow to the left.");
                }
            }
            ImGui::EndCombo();
        }
        ShowItemTooltip("Choose whether this text grows left, right, or stays centered around its current anchor.");
        if (tutorialAlternativeStrobeLabelSelected)
        {
            tutorial_->DrawHalo(
                "primitive_alignment",
                "Open Alignment",
                "Choose the aircraft label alignment before the next transform-related tutorial steps.");
        }

        if (ImGui::DragFloat("Font size", &text->fontSize, 0.002f, 0.01f, 0.25f, "%.4f"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
        }
        ShowItemTooltip("Logical font size used for this text primitive.");
        if (ImGui::DragFloat("Letter spacing", &text->letterSpacing, 0.0005f, -0.05f, 0.10f, "%.4f"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
        }
        ShowItemTooltip("Additional spacing inserted between letters.");
        return;
    }

    if (auto* time = std::get_if<mfd::TimeGeometry>(&primitive->geometry))
    {
        EditTimeFormatField("Format",
                            "strftime-style format string used by this time primitive. "
                            "Changes apply when the field loses focus and invalid directives are rejected.",
                            *time);

        if (ImGui::BeginCombo("Alignment", AlignLabel(time->align)))
        {
            for (const mfd::Align candidate : kSupportedTextAlignments)
            {
                const bool selected = time->align == candidate;
                if (ImGui::Selectable(AlignLabel(candidate), selected) && !selected)
                {
                    PushUndoSnapshot();
                    ApplyPrimitiveTextAlignmentChange(
                        *primitive,
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
        if (ImGui::Checkbox("UTC", &utc))
        {
            PushUndoSnapshot();
            time->utc = utc;
        }
        ShowItemTooltip("Render the time in UTC instead of local time.");

        if (ImGui::DragFloat("Font size", &time->fontSize, 0.002f, 0.01f, 0.25f, "%.4f"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
        }
        ShowItemTooltip("Logical font size used for this time primitive.");
        if (ImGui::DragFloat("Letter spacing", &time->letterSpacing, 0.0005f, -0.05f, 0.10f, "%.4f"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
        }
        ShowItemTooltip("Additional spacing inserted between characters.");
        return;
    }

    if (auto* line = std::get_if<mfd::LineGeometry>(&primitive->geometry))
    {
        EditPointArrayField("Start", line->start);
        EditPointArrayField("End", line->end);
        return;
    }

    if (auto* circle = std::get_if<mfd::CircleGeometry>(&primitive->geometry))
    {
        if (ImGui::DragFloat("Radius", &circle->radius, 0.002f, 0.001f, 1.0f, "%.4f"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            circle->radius = std::max(0.001f, circle->radius);
        }
        ShowItemTooltip("Circle radius in logical units.");
        return;
    }

    if (auto* ring = std::get_if<mfd::RingGeometry>(&primitive->geometry))
    {
        if (ImGui::DragFloat("Inner radius", &ring->innerRadius, 0.002f, 0.001f, 1.0f, "%.4f"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            ring->innerRadius = std::clamp(ring->innerRadius, 0.001f, std::max(0.001f, ring->outerRadius - 0.001f));
        }
        ShowItemTooltip("Inner radius of the ring in logical units.");

        if (ImGui::DragFloat("Outer radius", &ring->outerRadius, 0.002f, 0.001f, 1.0f, "%.4f"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            ring->outerRadius = std::max(ring->innerRadius + 0.001f, ring->outerRadius);
        }
        ShowItemTooltip("Outer radius of the ring in logical units.");

        if (ImGui::DragInt("Segments", &ring->segments, 1.0f, 8, 256))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            ring->segments = std::clamp(ring->segments, 8, 256);
        }
        ShowItemTooltip("Number of segments used to approximate the ring circles.");
        return;
    }

    if (auto* rectangle = std::get_if<mfd::RectangleGeometry>(&primitive->geometry))
    {
        if (ImGui::DragFloat2("Size", &rectangle->width, 0.002f, 0.001f, 1.0f, "%.4f"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            rectangle->width = std::max(0.001f, rectangle->width);
            rectangle->height = std::max(0.001f, rectangle->height);
        }
        ShowItemTooltip("Rectangle width and height in logical units.");
        return;
    }

    if (auto* ellipse = std::get_if<mfd::EllipseGeometry>(&primitive->geometry))
    {
        if (ImGui::DragFloat2("Size", &ellipse->width, 0.002f, 0.001f, 1.0f, "%.4f"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            ellipse->width = std::max(0.001f, ellipse->width);
            ellipse->height = std::max(0.001f, ellipse->height);
        }
        ShowItemTooltip("Ellipse width and height in logical units.");
        return;
    }

    if (auto* square = std::get_if<mfd::SquareGeometry>(&primitive->geometry))
    {
        if (ImGui::DragFloat2("Size", &square->width, 0.002f, 0.001f, 1.0f, "%.4f"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            square->width = std::max(0.001f, square->width);
            square->height = std::max(0.001f, square->height);
        }
        ShowItemTooltip("Square width and height in logical units.");
        return;
    }

    if (auto* diamond = std::get_if<mfd::DiamondGeometry>(&primitive->geometry))
    {
        if (ImGui::DragFloat2("Size", &diamond->width, 0.002f, 0.001f, 1.0f, "%.4f"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            diamond->width = std::max(0.001f, diamond->width);
            diamond->height = std::max(0.001f, diamond->height);
        }
        ShowItemTooltip("Diamond width and height in logical units.");
        return;
    }

    if (auto* triangle = std::get_if<mfd::TriangleGeometry>(&primitive->geometry))
    {
        EditPointArrayField("Point A", triangle->points[0]);
        EditPointArrayField("Point B", triangle->points[1]);
        EditPointArrayField("Point C", triangle->points[2]);
        return;
    }

    if (auto* polyline = std::get_if<mfd::PolylineGeometry>(&primitive->geometry))
    {
        bool closed = polyline->closed;
        if (ImGui::Checkbox("Closed", &closed))
        {
            PushUndoSnapshot();
            polyline->closed = closed;
            if (!polyline->closed)
            {
                primitive->style.filled = false;
            }
        }
        ShowItemTooltip("Close the polyline by linking the last point back to the first.");

        for (int index = 0; index < static_cast<int>(polyline->points.size()); ++index)
        {
            const std::string label = "Point " + std::to_string(index + 1);
            EditPointArrayField(label.c_str(), polyline->points[static_cast<std::size_t>(index)]);
        }

        if (ImGui::Button("Add point"))
        {
            PushUndoSnapshot();
            polyline->points.push_back({});
        }
        ShowItemTooltip("Append one new point to the end of the polyline.");
        ImGui::SameLine();
        if (ImGui::Button("Remove last point") && !polyline->points.empty())
        {
            PushUndoSnapshot();
            polyline->points.pop_back();
        }
        ShowItemTooltip("Remove the last point from the polyline.");
        return;
    }

    if (auto* bezier = std::get_if<mfd::BezierGeometry>(&primitive->geometry))
    {
        for (int index = 0; index < static_cast<int>(bezier->controlPoints.size()); ++index)
        {
            const std::string label = "Control " + std::to_string(index + 1);
            EditPointArrayField(label.c_str(), bezier->controlPoints[static_cast<std::size_t>(index)]);
        }

        if (ImGui::Button("Add control point"))
        {
            PushUndoSnapshot();
            bezier->controlPoints.push_back({});
        }
        ShowItemTooltip("Append one new control point to this bezier curve.");
        ImGui::SameLine();
        if (ImGui::Button("Remove last control point") && !bezier->controlPoints.empty())
        {
            PushUndoSnapshot();
            bezier->controlPoints.pop_back();
        }
        ShowItemTooltip("Remove the last control point from this bezier curve.");

        if (ImGui::DragInt("Segments", &bezier->segments, 1.0f, 2, 128))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            bezier->segments = std::clamp(bezier->segments, 2, 128);
        }
        ShowItemTooltip("Number of line segments used to approximate the bezier curve.");
        return;
    }

    if (auto* arc = std::get_if<mfd::ArcGeometry>(&primitive->geometry))
    {
        if (ImGui::DragFloat("Radius", &arc->radius, 0.002f, 0.001f, 1.0f, "%.4f"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            arc->radius = std::max(0.001f, arc->radius);
        }
        ShowItemTooltip("Arc radius in logical units.");

        if (ImGui::DragFloat("Start angle", &arc->startAngleDegrees, 0.5f, -720.0f, 720.0f, "%.1f deg"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
        }
        ShowItemTooltip("Arc start angle in degrees.");

        if (ImGui::DragFloat("End angle", &arc->endAngleDegrees, 0.5f, -720.0f, 720.0f, "%.1f deg"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
        }
        ShowItemTooltip("Arc end angle in degrees.");

        if (ImGui::DragInt("Segments", &arc->segments, 1.0f, 2, 256))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            arc->segments = std::clamp(arc->segments, 2, 256);
        }
        ShowItemTooltip("Number of line segments used to approximate the arc.");
        return;
    }

    if (auto* image = std::get_if<mfd::ImageGeometry>(&primitive->geometry))
    {
        std::array<char, kPathTextCapacity> imagePath {};
        CopyTextBuffer(imagePath, image->file.string());
        const bool pathChanged = ImGui::InputText("Image file", imagePath.data(), imagePath.size());
        ShowItemTooltip("Path to the raster image displayed by this primitive.");
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        if (pathChanged)
        {
            image->file = std::filesystem::path(imagePath.data()).lexically_normal();
        }

        if (ImGui::DragFloat2("Size", &image->width, 0.002f, 0.001f, 2.0f, "%.4f"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            image->width = std::max(0.001f, image->width);
            image->height = std::max(0.001f, image->height);
        }
        ShowItemTooltip("Logical size of the image before the primitive scale is applied.");
    }
    }
}
