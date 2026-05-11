/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorApplication.h"

/**
 * @file
 * @brief Main editor shell implementation (layout, interaction, preview and persistence wiring).
 */

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <exception>
#include <functional>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <rlImGui.h>

#include "EditorTutorialController.h"
#include "EditorTutorialData.h"
#include "EditorFileDialogs.h"
#include "EditorReticleExtractionService.h"
#include "EditorReticleUsageHighlightService.h"
#include "EditorUiTheme.h"
#include "EditorWorkspaceLayout.h"
#include "mfd/model/Types.h"
#include "Canvas2D.h"
#include "RenderTextureUtils.h"
#include "mfd/render/WindowBranding.h"

namespace
{
using editor::ui::AccentButton;
using editor::ui::ApplyEditorTheme;
using editor::ui::DrawVerticalSplitter;
using editor::ui::FormatViewportToolbarInfoLabel;
using editor::ui::ShowItemTooltip;

constexpr float kSidebarWidth = 320.0f;
constexpr float kInspectorWidth = 360.0f;
constexpr float kMinSidebarWidth = 220.0f;
constexpr float kMinInspectorWidth = 280.0f;
constexpr float kMinWorkspaceWidth = 420.0f;
constexpr float kMinPageContextWidth = 320.0f;
constexpr float kMinReticleStudioWidth = 320.0f;
constexpr float kLayerInspectorDockWidth = 248.0f;
constexpr float kLayerInspectorPreviewHeight = 84.0f;
constexpr float kPreviewProblemsDockHeight = 176.0f;
constexpr std::string_view kTutorialStrobeCursorTemplateId = "mfd_tutorial_strobe_cursor";
constexpr const char* kPagePreviewHelpPopupId = "PagePreviewHelpPopup";
constexpr const char* kLibraryPreviewHelpPopupId = "LibraryPreviewHelpPopup";
constexpr const char* kPagePreviewDisplayPopupId = "PagePreviewDisplayPopup";
constexpr const char* kReticleStudioDisplayPopupId = "ReticleStudioDisplayPopup";

std::optional<std::filesystem::path> FindProjectRoot(const std::filesystem::path& start)
{
    std::filesystem::path current = std::filesystem::absolute(start);

    while (true)
    {
        if (std::filesystem::exists(current / "CMakeLists.txt") &&
            std::filesystem::exists(current / "assets") &&
            std::filesystem::exists(current / "mfd_editor"))
        {
            return current;
        }

        if (current == current.root_path())
        {
            break;
        }

        current = current.parent_path();
    }

    return std::nullopt;
}

std::filesystem::path DefaultProjectAssetFolder(const std::string_view relativeAssetFolder)
{
    if (const auto projectRoot = FindProjectRoot(std::filesystem::current_path()); projectRoot.has_value())
    {
        return (*projectRoot / std::filesystem::path(relativeAssetFolder)).lexically_normal();
    }

    return std::filesystem::absolute(std::filesystem::path(relativeAssetFolder)).lexically_normal();
}

std::filesystem::path DefaultSiblingAssetFile(const std::filesystem::path& anchorFile,
                                              const std::string_view siblingFolder,
                                              const std::string_view fileName)
{
    if (anchorFile.empty())
    {
        return (DefaultProjectAssetFolder(std::string("assets/") + std::string(siblingFolder)) /
                std::filesystem::path(fileName))
            .lexically_normal();
    }

    const std::filesystem::path anchorFolder = anchorFile.parent_path();
    const std::filesystem::path candidateRoot =
        anchorFolder.filename() == std::filesystem::path(siblingFolder)
            ? anchorFolder
            : (anchorFolder.has_parent_path() ? anchorFolder.parent_path() / std::filesystem::path(siblingFolder)
                                              : anchorFolder / std::filesystem::path(siblingFolder));
    return (candidateRoot / std::filesystem::path(fileName)).lexically_normal();
}

std::filesystem::path JsonFileNameOrFallback(const std::filesystem::path& candidate, const std::string_view fallbackFileName)
{
    std::filesystem::path fileName = candidate.filename();
    if (fileName.empty() || fileName == "." || fileName == "..")
    {
        fileName = std::filesystem::path(fallbackFileName);
    }

    if (fileName.extension().empty())
    {
        fileName += ".json";
    }

    return fileName;
}

std::string Lowercase(const std::string_view value)
{
    std::string lowered;
    lowered.reserve(value.size());

    for (const char character : value)
    {
        lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
    }

    return lowered;
}

const char* ReticleReferenceKindLabel(const editor::ReticleReferenceKind kind) noexcept
{
    switch (kind)
    {
    case editor::ReticleReferenceKind::PageStrobeTemplate:
        return "Page strobe";
    case editor::ReticleReferenceKind::PageDynamicTemplate:
        return "Page dynamic template";
    case editor::ReticleReferenceKind::PageReticleTemplate:
    default:
        return "Page reticle";
    }
}

bool PathContainsSegment(const std::filesystem::path& path, const std::string_view segment)
{
    if (segment.empty())
    {
        return false;
    }

    std::string normalizedSegment(segment);
    std::transform(normalizedSegment.begin(),
                   normalizedSegment.end(),
                   normalizedSegment.begin(),
                   [](const unsigned char ch)
                   {
                       return static_cast<char>(std::tolower(ch));
                   });

    for (const auto& part : path)
    {
        std::string candidate = part.string();
        std::transform(candidate.begin(),
                       candidate.end(),
                       candidate.begin(),
                       [](const unsigned char ch)
                       {
                           return static_cast<char>(std::tolower(ch));
                       });
        if (candidate == normalizedSegment)
        {
            return true;
        }
    }

    return false;
}

bool IsExecStagingPath(const std::filesystem::path& path)
{
    if (path.empty())
    {
        return false;
    }

    const std::filesystem::path absolutePath =
        path.is_absolute() ? path.lexically_normal() : std::filesystem::absolute(path).lexically_normal();
    return PathContainsSegment(absolutePath, "_Exec");
}

std::optional<std::filesystem::path> FindAncestorNamed(std::filesystem::path path, const std::string_view folderName)
{
    path = path.empty() ? std::filesystem::path {} : std::filesystem::absolute(path).lexically_normal();
    const std::string normalizedFolderName = Lowercase(folderName);

    while (!path.empty())
    {
        if (Lowercase(path.filename().string()) == normalizedFolderName)
        {
            return path;
        }

        if (!path.has_parent_path() || path == path.parent_path())
        {
            break;
        }

        path = path.parent_path();
    }

    return std::nullopt;
}

std::filesystem::path ResolveAssetRootForPath(const std::filesystem::path& path)
{
    if (const auto assetsRoot = FindAncestorNamed(path, "assets"); assetsRoot.has_value())
    {
        return assetsRoot->lexically_normal();
    }

    return DefaultProjectAssetFolder("assets");
}

std::filesystem::path CurrentPageImportTargetFolder(const std::filesystem::path& windowFile,
                                                    const editor::EditorFileLayout& files)
{
    if (!files.pageFiles.empty())
    {
        const std::filesystem::path firstFolder = files.pageFiles.front().parent_path();
        const bool allPageFoldersMatch = std::all_of(files.pageFiles.begin(),
                                                     files.pageFiles.end(),
                                                     [&firstFolder](const std::filesystem::path& pageFile)
                                                     {
                                                         return pageFile.parent_path().lexically_normal() ==
                                                                firstFolder.lexically_normal();
                                                     });
        if (allPageFoldersMatch && !firstFolder.empty())
        {
            return firstFolder.lexically_normal();
        }
    }

    if (const auto assetsRoot = FindAncestorNamed(windowFile.parent_path(), "assets"); assetsRoot.has_value())
    {
        return (*assetsRoot / "pages").lexically_normal();
    }

    if (Lowercase(windowFile.parent_path().filename().string()) == "windows" && windowFile.parent_path().has_parent_path())
    {
        return (windowFile.parent_path().parent_path() / "pages").lexically_normal();
    }

    return windowFile.empty() ? DefaultProjectAssetFolder("assets/pages") : windowFile.parent_path().lexically_normal();
}

const char* ImportDispositionLabel(const editor::ImportDisposition disposition) noexcept
{
    switch (disposition)
    {
    case editor::ImportDisposition::CopyNew:
        return "copy";
    case editor::ImportDisposition::KeepExisting:
        return "keep existing";
    case editor::ImportDisposition::RenameCopy:
        return "rename copy";
    }

    return "copy";
}

ImVec4 ImportDispositionColor(const editor::ImportDisposition disposition) noexcept
{
    switch (disposition)
    {
    case editor::ImportDisposition::CopyNew:
        return ImVec4(0.33f, 0.86f, 0.78f, 1.0f);
    case editor::ImportDisposition::KeepExisting:
        return ImVec4(0.66f, 0.78f, 0.95f, 1.0f);
    case editor::ImportDisposition::RenameCopy:
        return ImVec4(0.95f, 0.72f, 0.38f, 1.0f);
    }

    return ImVec4(0.33f, 0.86f, 0.78f, 1.0f);
}

void TryApplyEditorWindowIcon()
{
    std::string error;
    const std::filesystem::path iconFile = mfd::ResolveWindowBrandingIconFile();
    mfd::ApplyWindowIconFile(iconFile, &error);
}

int FindPageIndexByName(const mfd::LoadedWindowConfiguration& loaded, const std::string_view pageName)
{
    for (int index = 0; index < static_cast<int>(loaded.document.pages.size()); ++index)
    {
        const auto& page = loaded.document.pages[static_cast<std::size_t>(index)];
        if (page.name == pageName)
        {
            return index;
        }
    }

    return -1;
}

int FindPageReticleIndexById(const mfd::PageDefinition& page, const std::string_view reticleId)
{
    for (int index = 0; index < static_cast<int>(page.staticReticles.size()); ++index)
    {
        if (page.staticReticles[static_cast<std::size_t>(index)].id == reticleId)
        {
            return index;
        }
    }

    return -1;
}

template <std::size_t N>
void CopyTextBuffer(std::array<char, N>& destination, const std::string_view value)
{
    std::snprintf(destination.data(), destination.size(), "%.*s", static_cast<int>(value.size()), value.data());
}

Color ToRayColor(const mfd::ColorRgba& color)
{
    return Color {color.r, color.g, color.b, color.a};
}

std::uint8_t ToColorChannelByte(const float value) noexcept
{
    return static_cast<std::uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
}

void ApplyBilinearFilterToFont(const Font font) noexcept
{
    if (font.texture.id != 0)
    {
        SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
    }
}

ImVec4 ToImGuiColor(const mfd::ColorRgba& color)
{
    return ImVec4(
        static_cast<float>(color.r) / 255.0f,
        static_cast<float>(color.g) / 255.0f,
        static_cast<float>(color.b) / 255.0f,
        static_cast<float>(color.a) / 255.0f);
}

mfd::ColorRgba ToColorRgba(const ImVec4& color)
{
    return mfd::ColorRgba {
        ToColorChannelByte(color.x),
        ToColorChannelByte(color.y),
        ToColorChannelByte(color.z),
        ToColorChannelByte(color.w)};
}

void ShowHoveredRegionTooltip(const bool hovered, const char* text)
{
    if (!hovered || text == nullptr || text[0] == '\0')
    {
        return;
    }

    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 32.0f);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}

std::string PrimitiveTypeLabel(const mfd::PrimitiveType type)
{
    switch (type)
    {
    case mfd::PrimitiveType::Text:
        return "Text";
    case mfd::PrimitiveType::Time:
        return "Time";
    case mfd::PrimitiveType::Line:
        return "Line";
    case mfd::PrimitiveType::Circle:
        return "Circle";
    case mfd::PrimitiveType::Ring:
        return "Ring";
    case mfd::PrimitiveType::Rectangle:
        return "Rectangle";
    case mfd::PrimitiveType::Ellipse:
        return "Ellipse";
    case mfd::PrimitiveType::Square:
        return "Square";
    case mfd::PrimitiveType::Diamond:
        return "Diamond";
    case mfd::PrimitiveType::Triangle:
        return "Triangle";
    case mfd::PrimitiveType::Polyline:
        return "Polyline";
    case mfd::PrimitiveType::Bezier:
        return "Bezier";
    case mfd::PrimitiveType::Arc:
        return "Arc";
    case mfd::PrimitiveType::Image:
        return "Image";
    }

    return "Primitive";
}

const char* LineStyleLabel(const mfd::LineStyle lineStyle) noexcept
{
    switch (lineStyle)
    {
    case mfd::LineStyle::Dotted:
        return "Dotted";
    case mfd::LineStyle::Dashed:
        return "Dashed";
    case mfd::LineStyle::Solid:
    default:
        return "Solid";
    }
}

bool SupportsPrimitiveLineStyle(const mfd::PrimitiveType type) noexcept
{
    switch (type)
    {
    case mfd::PrimitiveType::Text:
    case mfd::PrimitiveType::Time:
    case mfd::PrimitiveType::Image:
        return false;
    default:
        return true;
    }
}

const char* ReticleClipModeLabel(const mfd::ReticleClipMode mode) noexcept
{
    switch (mode)
    {
    case mfd::ReticleClipMode::Inner:
        return "Inner clipping";
    case mfd::ReticleClipMode::Outer:
        return "Outer clipping";
    case mfd::ReticleClipMode::None:
    default:
        return "Disabled";
    }
}

struct ClipPrimitiveOption
{
    std::string primitiveId;
    std::string label;
};

std::vector<ClipPrimitiveOption> CollectClipPrimitiveOptions(const mfd::ReticleGroup& reticle)
{
    std::vector<ClipPrimitiveOption> options;
    std::unordered_set<std::string> seenPrimitiveIds;

    for (int index = 0; index < static_cast<int>(reticle.primitives.size()); ++index)
    {
        const auto& primitive = reticle.primitives[static_cast<std::size_t>(index)];
        if (primitive.id.empty() || !mfd::SupportsReticleClipPrimitive(primitive))
        {
            continue;
        }

        if (!seenPrimitiveIds.insert(primitive.id).second)
        {
            continue;
        }

        options.push_back(ClipPrimitiveOption {
            primitive.id,
            primitive.id + " (" + PrimitiveTypeLabel(primitive.type) + ")"});
    }

    return options;
}

bool IsPointInsidePolygon(const std::vector<ImVec2>& polygon, const ImVec2 point) noexcept
{
    if (polygon.size() < 3)
    {
        return false;
    }

    bool inside = false;
    std::size_t previous = polygon.size() - 1U;
    for (std::size_t current = 0; current < polygon.size(); ++current)
    {
        const ImVec2& a = polygon[current];
        const ImVec2& b = polygon[previous];

        const bool intersects =
            ((a.y > point.y) != (b.y > point.y)) &&
            (point.x < (b.x - a.x) * (point.y - a.y) / ((b.y - a.y) + 0.00001f) + a.x);
        if (intersects)
        {
            inside = !inside;
        }

        previous = current;
    }

    return inside;
}

float EstimatedTextHalfWidth(const std::string_view text, const float fontSize, const float letterSpacing)
{
    const std::size_t characterCount = std::max<std::size_t>(1, text.size());
    const float glyphWidth = fontSize * static_cast<float>(characterCount) * 0.30f;
    const float spacingWidth = std::max(0.0f, letterSpacing) *
                               static_cast<float>(characterCount > 0 ? characterCount - 1U : 0U) * 0.5f;
    return std::max(0.03f, glyphWidth + spacingWidth);
}

float EstimatedTextHalfWidth(const mfd::TextGeometry& text)
{
    return EstimatedTextHalfWidth(text.text, text.fontSize, text.letterSpacing);
}

float EstimatedTextHalfWidth(const mfd::TimeGeometry& time)
{
    return EstimatedTextHalfWidth(time.format.empty() ? std::string_view {"%H:%M:%S"} : std::string_view {time.format},
                                  time.fontSize,
                                  time.letterSpacing);
}

float EstimatedTextHalfHeight(const float fontSize)
{
    return std::max(0.02f, fontSize * 0.50f);
}

float EstimatedTextHalfHeight(const mfd::TextGeometry& text)
{
    return EstimatedTextHalfHeight(text.fontSize);
}

float EstimatedTextHalfHeight(const mfd::TimeGeometry& time)
{
    return EstimatedTextHalfHeight(time.fontSize);
}

std::vector<mfd::Vec2> ApproximateEllipsePoints(const float width, const float height, const int segments = 48)
{
    const int segmentCount = std::max(12, segments);
    const float halfWidth = width * 0.5f;
    const float halfHeight = height * 0.5f;
    std::vector<mfd::Vec2> points;
    points.reserve(static_cast<std::size_t>(segmentCount));

    for (int index = 0; index < segmentCount; ++index)
    {
        const float angle = static_cast<float>(index) / static_cast<float>(segmentCount) * 2.0f * PI;
        points.push_back({std::cos(angle) * halfWidth, std::sin(angle) * halfHeight});
    }

    return points;
}

std::vector<mfd::Vec2> ApproximateArcPoints(const float radius,
                                            const float startAngleDegrees,
                                            const float endAngleDegrees,
                                            const int segments = 48)
{
    const int segmentCount = std::max(2, segments);
    const float safeRadius = std::max(0.0f, std::abs(radius));
    const float startRadians = startAngleDegrees * PI / 180.0f;
    const float sweepRadians = (endAngleDegrees - startAngleDegrees) * PI / 180.0f;
    std::vector<mfd::Vec2> points;
    points.reserve(static_cast<std::size_t>(segmentCount) + 1U);

    for (int index = 0; index <= segmentCount; ++index)
    {
        const float factor = static_cast<float>(index) / static_cast<float>(segmentCount);
        const float angle = startRadians + sweepRadians * factor;
        points.push_back({std::cos(angle) * safeRadius, std::sin(angle) * safeRadius});
    }

    return points;
}

mfd::Vec2 InverseTransformPoint(const mfd::Vec2& point, const mfd::Transform2D& transform)
{
    const mfd::Vec2 translated = point - transform.position;
    const mfd::Vec2 rotated = mfd::Rotate(translated, -transform.rotationDegrees);

    return {
        std::abs(transform.scale.x) <= 0.0001f ? 0.0f : rotated.x / transform.scale.x,
        std::abs(transform.scale.y) <= 0.0001f ? 0.0f : rotated.y / transform.scale.y};
}

float Distance(const ImVec2 lhs, const ImVec2 rhs)
{
    const float dx = lhs.x - rhs.x;
    const float dy = lhs.y - rhs.y;
    return std::sqrt(dx * dx + dy * dy);
}

bool BlinkStateMatchesNormalizedName(const mfd::ReticleBlinkState& blink,
                                     const std::string_view normalizedBlinkTypeName)
{
    const std::string currentNormalizedName =
        blink.normalizedTypeName.empty() ? mfd::NormalizePageName(blink.typeName) : blink.normalizedTypeName;
    return currentNormalizedName == normalizedBlinkTypeName;
}

std::filesystem::path ConfiguredPathFolder(const std::filesystem::path& configuredPath)
{
    if (configuredPath.empty())
    {
        return {};
    }

    return configuredPath.has_extension() ? configuredPath.parent_path() : configuredPath;
}

bool ReticleIdExistsExact(const std::vector<mfd::ReticleGroup>& groups, const std::string_view id)
{
    return std::any_of(groups.begin(),
                       groups.end(),
                       [id](const mfd::ReticleGroup& reticle)
                       {
                           return reticle.id == id;
                       });
}

bool PageLayerIdExistsExact(const mfd::PageDefinition& page, const std::string_view id)
{
    return std::any_of(page.layers.begin(),
                       page.layers.end(),
                       [id](const mfd::PageLayerDefinition& layer)
                       {
                           return layer.id == id;
                       });
}

std::string NormalizeEditorIdentifier(const std::string_view value)
{
    return mfd::NormalizePageName(value);
}

std::string PageProblemId(const mfd::PageDefinition& page)
{
    return "page/" + NormalizeEditorIdentifier(page.normalizedName.empty() ? page.name : page.normalizedName);
}

std::string ReticleAssetProblemId(const std::string_view templateId)
{
    return "reticle/" + NormalizeEditorIdentifier(templateId);
}

std::string PageReticleProblemId(const mfd::PageDefinition& page, const mfd::ReticleGroup& reticle)
{
    return PageProblemId(page) + "/reticle/" + NormalizeEditorIdentifier(reticle.id);
}

void PushProblem(std::vector<std::string>& messages, const std::string_view entityId, const std::string_view message)
{
    if (entityId.empty())
    {
        messages.emplace_back(message);
        return;
    }

    messages.push_back(std::string(entityId) + ": " + std::string(message));
}

void AppendPrimitiveProblems(std::vector<std::string>& messages,
                             const std::vector<mfd::Primitive>& primitives,
                             const std::string_view ownerId)
{
    std::unordered_set<std::string> primitiveIds;
    for (const mfd::Primitive& primitive : primitives)
    {
        if (primitive.id.empty())
        {
            continue;
        }

        if (!primitiveIds.insert(NormalizeEditorIdentifier(primitive.id)).second)
        {
            PushProblem(messages, ownerId, "Primitive ids must stay unique inside one reticle.");
        }
    }
}

std::size_t PageLayerOrder(const mfd::PageDefinition& page, const std::string_view layerId)
{
    for (std::size_t index = 0; index < page.layers.size(); ++index)
    {
        if (mfd::PageNamesEqual(page.layers[index].id, layerId))
        {
            return index;
        }
    }

    return page.layers.size();
}

bool PageHasDynamicTemplateBinding(const mfd::PageDefinition& page, const std::string_view templateId)
{
    return mfd::FindDynamicReticleLayerBinding(page, templateId) != nullptr;
}

bool PageHasDynamicOrderConflict(const mfd::PageDefinition& page,
                                 const std::string_view layerId,
                                 const int orderInLayer,
                                 const int ignoredIndex = -1)
{
    for (int index = 0; index < static_cast<int>(page.dynamicReticleBindings.size()); ++index)
    {
        if (index == ignoredIndex)
        {
            continue;
        }

        const mfd::DynamicReticleLayerBinding& binding = page.dynamicReticleBindings[static_cast<std::size_t>(index)];
        if (mfd::PageNamesEqual(binding.layerId, layerId) && binding.orderInLayer == orderInLayer)
        {
            return true;
        }
    }

    return false;
}

int NextPageDynamicOrderInLayer(const mfd::PageDefinition& page,
                                const std::string_view layerId,
                                const int ignoredIndex = -1)
{
    int nextOrder = 0;
    while (PageHasDynamicOrderConflict(page, layerId, nextOrder, ignoredIndex))
    {
        ++nextOrder;
    }

    return nextOrder;
}

struct LogicalBounds
{
    mfd::Vec2 min {};
    mfd::Vec2 max {};
    mfd::Vec2 center {};
    bool valid = false;
};

struct PageMinimapState
{
    ImVec2 frameMin {};
    ImVec2 frameMax {};
    ImVec2 contentMin {};
    ImVec2 contentMax {};
    ImVec2 contentCenter {};
    mfd::Vec2 logicalMin {};
    mfd::Vec2 logicalMax {};
    mfd::Vec2 logicalCenter {};
    float pixelsPerLogicalUnit = 1.0f;
    bool valid = false;
};

struct PageReticleHit
{
    int reticleIndex = -1;
    float distance = std::numeric_limits<float>::max();
    float area = std::numeric_limits<float>::max();
    bool directHit = false;
    bool boundsHit = false;
    int drawPriority = 0;
};

struct PageClipPrimitiveHit
{
    int reticleIndex = -1;
    int primitiveIndex = -1;
    float primitiveDistance = std::numeric_limits<float>::max();
    float reticleDistance = std::numeric_limits<float>::max();
};

struct TutorialDynamicTemplateInfo
{
    std::string_view templateId;
    std::string_view preferredLayerId;
    std::string_view targetId;
    const char* label;
    const char* reason;
};

struct DynamicBindingDraftState
{
    std::string templateId;
    std::string layerId;
};

void IncludeLogicalPoint(LogicalBounds& bounds, const mfd::Vec2 point)
{
    if (!bounds.valid)
    {
        bounds.min = point;
        bounds.max = point;
        bounds.valid = true;
        return;
    }

    bounds.min.x = std::min(bounds.min.x, point.x);
    bounds.min.y = std::min(bounds.min.y, point.y);
    bounds.max.x = std::max(bounds.max.x, point.x);
    bounds.max.y = std::max(bounds.max.y, point.y);
}

void FinalizeLogicalBounds(LogicalBounds& bounds)
{
    if (!bounds.valid)
    {
        return;
    }

    bounds.center = {
        (bounds.min.x + bounds.max.x) * 0.5f,
        (bounds.min.y + bounds.max.y) * 0.5f};
}

void IncludeLogicalBounds(LogicalBounds& bounds, const LogicalBounds& other)
{
    if (!other.valid)
    {
        return;
    }

    IncludeLogicalPoint(bounds, other.min);
    IncludeLogicalPoint(bounds, other.max);
}

LogicalBounds ComputePrimitiveLocalBounds(const mfd::Primitive& primitive)
{
    LogicalBounds bounds;
    if (!primitive.style.visible)
    {
        return bounds;
    }

    auto includeTransformedPoint = [&](const mfd::Vec2 localPoint)
    {
        IncludeLogicalPoint(bounds, mfd::ApplyTransform(localPoint, primitive.transform));
    };

    if (const auto* text = std::get_if<mfd::TextGeometry>(&primitive.geometry))
    {
        const float halfWidth = EstimatedTextHalfWidth(*text);
        const float halfHeight = EstimatedTextHalfHeight(*text);
        includeTransformedPoint({-halfWidth, -halfHeight});
        includeTransformedPoint({halfWidth, -halfHeight});
        includeTransformedPoint({halfWidth, halfHeight});
        includeTransformedPoint({-halfWidth, halfHeight});
    }
    else if (const auto* time = std::get_if<mfd::TimeGeometry>(&primitive.geometry))
    {
        const float halfWidth = EstimatedTextHalfWidth(*time);
        const float halfHeight = EstimatedTextHalfHeight(*time);
        includeTransformedPoint({-halfWidth, -halfHeight});
        includeTransformedPoint({halfWidth, -halfHeight});
        includeTransformedPoint({halfWidth, halfHeight});
        includeTransformedPoint({-halfWidth, halfHeight});
    }
    else if (const auto* line = std::get_if<mfd::LineGeometry>(&primitive.geometry))
    {
        includeTransformedPoint(line->start);
        includeTransformedPoint(line->end);
    }
    else if (const auto* circle = std::get_if<mfd::CircleGeometry>(&primitive.geometry))
    {
        includeTransformedPoint({-circle->radius, -circle->radius});
        includeTransformedPoint({circle->radius, -circle->radius});
        includeTransformedPoint({circle->radius, circle->radius});
        includeTransformedPoint({-circle->radius, circle->radius});
    }
    else if (const auto* ring = std::get_if<mfd::RingGeometry>(&primitive.geometry))
    {
        includeTransformedPoint({-ring->outerRadius, -ring->outerRadius});
        includeTransformedPoint({ring->outerRadius, -ring->outerRadius});
        includeTransformedPoint({ring->outerRadius, ring->outerRadius});
        includeTransformedPoint({-ring->outerRadius, ring->outerRadius});
    }
    else if (const auto* rectangle = std::get_if<mfd::RectangleGeometry>(&primitive.geometry))
    {
        includeTransformedPoint({-rectangle->width * 0.5f, -rectangle->height * 0.5f});
        includeTransformedPoint({rectangle->width * 0.5f, -rectangle->height * 0.5f});
        includeTransformedPoint({rectangle->width * 0.5f, rectangle->height * 0.5f});
        includeTransformedPoint({-rectangle->width * 0.5f, rectangle->height * 0.5f});
    }
    else if (const auto* ellipse = std::get_if<mfd::EllipseGeometry>(&primitive.geometry))
    {
        includeTransformedPoint({-ellipse->width * 0.5f, -ellipse->height * 0.5f});
        includeTransformedPoint({ellipse->width * 0.5f, -ellipse->height * 0.5f});
        includeTransformedPoint({ellipse->width * 0.5f, ellipse->height * 0.5f});
        includeTransformedPoint({-ellipse->width * 0.5f, ellipse->height * 0.5f});
    }
    else if (const auto* square = std::get_if<mfd::SquareGeometry>(&primitive.geometry))
    {
        includeTransformedPoint({-square->width * 0.5f, -square->height * 0.5f});
        includeTransformedPoint({square->width * 0.5f, -square->height * 0.5f});
        includeTransformedPoint({square->width * 0.5f, square->height * 0.5f});
        includeTransformedPoint({-square->width * 0.5f, square->height * 0.5f});
    }
    else if (const auto* diamond = std::get_if<mfd::DiamondGeometry>(&primitive.geometry))
    {
        includeTransformedPoint({0.0f, diamond->height * 0.5f});
        includeTransformedPoint({diamond->width * 0.5f, 0.0f});
        includeTransformedPoint({0.0f, -diamond->height * 0.5f});
        includeTransformedPoint({-diamond->width * 0.5f, 0.0f});
    }
    else if (const auto* triangle = std::get_if<mfd::TriangleGeometry>(&primitive.geometry))
    {
        includeTransformedPoint(triangle->points[0]);
        includeTransformedPoint(triangle->points[1]);
        includeTransformedPoint(triangle->points[2]);
    }
    else if (const auto* polyline = std::get_if<mfd::PolylineGeometry>(&primitive.geometry))
    {
        for (const auto& point : polyline->points)
        {
            includeTransformedPoint(point);
        }
    }
    else if (const auto* bezier = std::get_if<mfd::BezierGeometry>(&primitive.geometry))
    {
        for (const auto& point : bezier->controlPoints)
        {
            includeTransformedPoint(point);
        }
    }
    else if (const auto* arc = std::get_if<mfd::ArcGeometry>(&primitive.geometry))
    {
        for (const auto& point :
             ApproximateArcPoints(arc->radius, arc->startAngleDegrees, arc->endAngleDegrees, arc->segments))
        {
            includeTransformedPoint(point);
        }

        if (primitive.style.filled)
        {
            includeTransformedPoint({});
        }
    }
    else if (const auto* image = std::get_if<mfd::ImageGeometry>(&primitive.geometry))
    {
        includeTransformedPoint({-image->width * 0.5f, -image->height * 0.5f});
        includeTransformedPoint({image->width * 0.5f, -image->height * 0.5f});
        includeTransformedPoint({image->width * 0.5f, image->height * 0.5f});
        includeTransformedPoint({-image->width * 0.5f, image->height * 0.5f});
    }

    FinalizeLogicalBounds(bounds);
    return bounds;
}

LogicalBounds ComputeReticleLocalBounds(const mfd::ReticleGroup& reticle)
{
    LogicalBounds bounds;

    for (const auto& primitive : reticle.primitives)
    {
        const LogicalBounds primitiveBounds = ComputePrimitiveLocalBounds(primitive);
        if (!primitiveBounds.valid)
        {
            continue;
        }

        IncludeLogicalPoint(bounds, primitiveBounds.min);
        IncludeLogicalPoint(bounds, primitiveBounds.max);
    }

    FinalizeLogicalBounds(bounds);
    return bounds;
}

mfd::Vec2 ReticleVisualCenterLocal(const mfd::ReticleGroup& reticle)
{
    const LogicalBounds bounds = ComputeReticleLocalBounds(reticle);
    return bounds.valid ? bounds.center : mfd::Vec2 {};
}

LogicalBounds ComputeReticleWorldBounds(const mfd::ReticleGroup& reticle)
{
    const LogicalBounds localBounds = ComputeReticleLocalBounds(reticle);
    if (!localBounds.valid)
    {
        return {};
    }

    LogicalBounds worldBounds;
    const std::array<mfd::Vec2, 4> corners {{
        {localBounds.min.x, localBounds.min.y},
        {localBounds.max.x, localBounds.min.y},
        {localBounds.max.x, localBounds.max.y},
        {localBounds.min.x, localBounds.max.y},
    }};
    for (const mfd::Vec2& corner : corners)
    {
        IncludeLogicalPoint(worldBounds, mfd::ApplyTransform(corner, reticle.transform));
    }

    FinalizeLogicalBounds(worldBounds);
    return worldBounds;
}

mfd::PageViewState MakeViewFittingBounds(const LogicalBounds& bounds,
                                         const int width,
                                         const int height,
                                         const float padding = 0.82f) noexcept
{
    mfd::PageViewState view {};
    if (!bounds.valid || width <= 0 || height <= 0)
    {
        return view;
    }

    const float minDimension = static_cast<float>(std::min(width, height));
    if (minDimension <= 0.0f)
    {
        return view;
    }

    const float visibleHalfWidth = static_cast<float>(width) / minDimension;
    const float visibleHalfHeight = static_cast<float>(height) / minDimension;
    const float halfExtentX = std::max(0.05f, (bounds.max.x - bounds.min.x) * 0.5f);
    const float halfExtentY = std::max(0.05f, (bounds.max.y - bounds.min.y) * 0.5f);
    const float fitZoom =
        std::min(visibleHalfWidth / halfExtentX, visibleHalfHeight / halfExtentY) * std::clamp(padding, 0.1f, 1.0f);

    view.center = bounds.center;
    view.zoom = std::clamp(fitZoom, 0.1f, 20.0f);
    return view;
}

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

constexpr std::array<mfd::PrimitiveType, 14> kPrimitiveTypes {
    mfd::PrimitiveType::Text,
    mfd::PrimitiveType::Time,
    mfd::PrimitiveType::Line,
    mfd::PrimitiveType::Circle,
    mfd::PrimitiveType::Ring,
    mfd::PrimitiveType::Rectangle,
    mfd::PrimitiveType::Ellipse,
    mfd::PrimitiveType::Square,
    mfd::PrimitiveType::Diamond,
    mfd::PrimitiveType::Triangle,
    mfd::PrimitiveType::Polyline,
    mfd::PrimitiveType::Bezier,
    mfd::PrimitiveType::Arc,
    mfd::PrimitiveType::Image};

constexpr std::size_t kInvalidBlinkTypeIndex = std::numeric_limits<std::size_t>::max();

int DefaultPageIndex(const std::vector<mfd::PageDefinition>& pages) noexcept
{
    for (int index = 0; index < static_cast<int>(pages.size()); ++index)
    {
        if (pages[static_cast<std::size_t>(index)].defaultPage)
        {
            return index;
        }
    }

    return 0;
}

int SuggestReplacementPageIndex(const std::vector<mfd::PageDefinition>& pages, const int removedPageIndex) noexcept
{
    for (int index = 0; index < static_cast<int>(pages.size()); ++index)
    {
        if (index != removedPageIndex)
        {
            return index;
        }
    }

    return -1;
}

std::size_t FindBlinkTypeIndex(const mfd::PageDefinition& page, const std::string_view blinkTypeName)
{
    const std::string normalizedBlinkTypeName = mfd::NormalizePageName(blinkTypeName);
    if (normalizedBlinkTypeName.empty())
    {
        return kInvalidBlinkTypeIndex;
    }

    for (std::size_t index = 0; index < page.blinkTypes.size(); ++index)
    {
        if (page.blinkTypes[index].normalizedName == normalizedBlinkTypeName)
        {
            return index;
        }
    }

    return kInvalidBlinkTypeIndex;
}

std::size_t EffectiveDefaultBlinkTypeIndex(const mfd::PageDefinition& page)
{
    if (page.blinkTypes.empty())
    {
        return kInvalidBlinkTypeIndex;
    }

    if (!page.normalizedDefaultBlinkTypeName.empty())
    {
        for (std::size_t index = 0; index < page.blinkTypes.size(); ++index)
        {
            if (page.blinkTypes[index].normalizedName == page.normalizedDefaultBlinkTypeName)
            {
                return index;
            }
        }
    }

    return 0;
}

std::string MakeUniqueBlinkTypeName(const mfd::PageDefinition& page, std::string_view baseName)
{
    std::string candidate = baseName.empty() ? std::string {"blink"} : std::string(baseName);
    int suffix = 2;

    while (FindBlinkTypeIndex(page, candidate) != kInvalidBlinkTypeIndex)
    {
        candidate = std::string(baseName.empty() ? "blink" : baseName) + "_" + std::to_string(suffix++);
    }

    return candidate;
}

void RenameBlinkReference(mfd::ReticleBlinkState& blink,
                          const std::string_view previousNormalizedName,
                          const std::string& nextName,
                          const std::string& nextNormalizedName)
{
    if (previousNormalizedName.empty())
    {
        return;
    }

    const std::string currentNormalizedName =
        blink.normalizedTypeName.empty() ? mfd::NormalizePageName(blink.typeName) : blink.normalizedTypeName;
    if (currentNormalizedName != previousNormalizedName)
    {
        return;
    }

    blink.typeName = nextName;
    blink.normalizedTypeName = nextNormalizedName;
}

void ClearBlinkReference(mfd::ReticleBlinkState& blink, const std::string_view removedNormalizedName)
{
    if (removedNormalizedName.empty())
    {
        return;
    }

    const std::string currentNormalizedName =
        blink.normalizedTypeName.empty() ? mfd::NormalizePageName(blink.typeName) : blink.normalizedTypeName;
    if (currentNormalizedName == removedNormalizedName)
    {
        blink = {};
    }
}

std::size_t CountBlinkReferences(const mfd::PageDefinition& page, const std::string_view normalizedBlinkTypeName)
{
    if (normalizedBlinkTypeName.empty())
    {
        return 0;
    }

    std::size_t count = 0;
    for (const auto& reticle : page.staticReticles)
    {
        if (BlinkStateMatchesNormalizedName(reticle.blink, normalizedBlinkTypeName))
        {
            ++count;
        }
    }

    if (page.strobe.has_value() &&
        BlinkStateMatchesNormalizedName(page.strobe->reticle.blink, normalizedBlinkTypeName))
    {
        ++count;
    }

    return count;
}

void RenameBlinkReferences(mfd::PageDefinition& page,
                           const std::string_view previousNormalizedName,
                           const std::string& nextName)
{
    if (previousNormalizedName.empty())
    {
        return;
    }

    const std::string nextNormalizedName = mfd::NormalizePageName(nextName);
    if (page.normalizedDefaultBlinkTypeName == previousNormalizedName)
    {
        page.defaultBlinkTypeName = nextName;
        page.normalizedDefaultBlinkTypeName = nextNormalizedName;
    }

    for (auto& reticle : page.staticReticles)
    {
        RenameBlinkReference(reticle.blink, previousNormalizedName, nextName, nextNormalizedName);
    }

    if (page.strobe.has_value())
    {
        RenameBlinkReference(page.strobe->reticle.blink, previousNormalizedName, nextName, nextNormalizedName);
    }
}

void ClearBlinkReferencesForRemovedType(mfd::PageDefinition& page, const std::string_view removedNormalizedName)
{
    if (removedNormalizedName.empty())
    {
        return;
    }

    if (page.normalizedDefaultBlinkTypeName == removedNormalizedName)
    {
        page.defaultBlinkTypeName.clear();
        page.normalizedDefaultBlinkTypeName.clear();
    }

    for (auto& reticle : page.staticReticles)
    {
        ClearBlinkReference(reticle.blink, removedNormalizedName);
    }

    if (page.strobe.has_value())
    {
        ClearBlinkReference(page.strobe->reticle.blink, removedNormalizedName);
    }
}

void RefreshBlinkBindingForEditor(const mfd::PageDefinition& page, mfd::ReticleBlinkState& blink)
{
    blink.normalizedTypeName = blink.typeName.empty() ? std::string {} : mfd::NormalizePageName(blink.typeName);

    if (!blink.typeName.empty())
    {
        const std::size_t blinkTypeIndex = FindBlinkTypeIndex(page, blink.normalizedTypeName);
        blink.durationMs = blinkTypeIndex == kInvalidBlinkTypeIndex ? 0U : page.blinkTypes[blinkTypeIndex].durationMs;
        return;
    }

    if (!blink.enabled)
    {
        blink.durationMs = 0;
        return;
    }

    const std::size_t defaultBlinkTypeIndex = EffectiveDefaultBlinkTypeIndex(page);
    blink.durationMs =
        defaultBlinkTypeIndex == kInvalidBlinkTypeIndex ? 0U : page.blinkTypes[defaultBlinkTypeIndex].durationMs;
}

void RefreshPageBlinkStateForEditor(mfd::PageDefinition& page)
{
    for (auto& blinkType : page.blinkTypes)
    {
        blinkType.normalizedName = mfd::NormalizePageName(blinkType.name);
        blinkType.durationMs = std::max<std::uint32_t>(1U, blinkType.durationMs);
    }

    page.normalizedDefaultBlinkTypeName =
        page.defaultBlinkTypeName.empty() ? std::string {} : mfd::NormalizePageName(page.defaultBlinkTypeName);

    for (auto& reticle : page.staticReticles)
    {
        RefreshBlinkBindingForEditor(page, reticle.blink);
    }

    if (page.strobe.has_value())
    {
        RefreshBlinkBindingForEditor(page, page.strobe->reticle.blink);
    }
}

const mfd::EditorLayerDefinition* FindEditorLayer(const mfd::PageDefinition& page, const std::string_view layerId)
{
    if (layerId.empty())
    {
        return nullptr;
    }

    const auto iterator = std::find_if(page.editor.layers.begin(),
                                       page.editor.layers.end(),
                                       [layerId](const mfd::EditorLayerDefinition& layer)
                                       {
                                           return layer.id == layerId;
                                       });
    return iterator == page.editor.layers.end() ? nullptr : &(*iterator);
}

bool IsReticleVisibleInEditor(const mfd::PageDefinition& page, const mfd::ReticleGroup& reticle)
{
    if (const mfd::EditorLayerDefinition* layer = FindEditorLayer(page, reticle.layerId); layer != nullptr)
    {
        return layer->visible;
    }

    return true;
}

template <typename ViewportStateT>
LogicalBounds ComputeViewportLogicalBounds(const ViewportStateT& viewport)
{
    LogicalBounds bounds;
    if (!viewport.valid)
    {
        return bounds;
    }

    const std::array<ImVec2, 4> corners {{
        viewport.origin,
        ImVec2(viewport.origin.x + viewport.size.x, viewport.origin.y),
        ImVec2(viewport.origin.x + viewport.size.x, viewport.origin.y + viewport.size.y),
        ImVec2(viewport.origin.x, viewport.origin.y + viewport.size.y),
    }};
    for (const ImVec2& corner : corners)
    {
        IncludeLogicalPoint(bounds, viewport.ToLogical(corner));
    }

    FinalizeLogicalBounds(bounds);
    return bounds;
}

template <typename ViewportStateT>
PageMinimapState ComputePageMinimapState(const mfd::PageDefinition& page, const ViewportStateT& viewport)
{
    PageMinimapState state;
    if (!viewport.valid)
    {
        return state;
    }

    LogicalBounds logicalBounds;
    for (const auto& reticle : page.staticReticles)
    {
        if (!IsReticleVisibleInEditor(page, reticle))
        {
            continue;
        }

        IncludeLogicalBounds(logicalBounds, ComputeReticleWorldBounds(reticle));
    }

    if (page.strobe.has_value())
    {
        IncludeLogicalBounds(logicalBounds, ComputeReticleWorldBounds(page.strobe->reticle));
    }

    IncludeLogicalBounds(logicalBounds, ComputeViewportLogicalBounds(viewport));
    if (!logicalBounds.valid)
    {
        IncludeLogicalPoint(logicalBounds, mfd::Vec2 {-1.0f, -1.0f});
        IncludeLogicalPoint(logicalBounds, mfd::Vec2 {1.0f, 1.0f});
    }

    FinalizeLogicalBounds(logicalBounds);

    const float width = std::max(logicalBounds.max.x - logicalBounds.min.x, 0.001f);
    const float height = std::max(logicalBounds.max.y - logicalBounds.min.y, 0.001f);
    const float logicalPadding = std::max({0.20f, width * 0.12f, height * 0.12f});
    logicalBounds.min.x -= logicalPadding;
    logicalBounds.min.y -= logicalPadding;
    logicalBounds.max.x += logicalPadding;
    logicalBounds.max.y += logicalPadding;
    FinalizeLogicalBounds(logicalBounds);

    const float paddedWidth = std::max(logicalBounds.max.x - logicalBounds.min.x, 0.001f);
    const float paddedHeight = std::max(logicalBounds.max.y - logicalBounds.min.y, 0.001f);
    const ImVec2 frameSize(
        std::clamp(viewport.size.x * 0.24f, 150.0f, 240.0f),
        std::clamp(viewport.size.y * 0.24f, 120.0f, 210.0f));
    constexpr float kFrameMargin = 16.0f;
    constexpr float kInnerPadding = 12.0f;

    state.frameMin = ImVec2(
        viewport.origin.x + viewport.size.x - frameSize.x - kFrameMargin,
        viewport.origin.y + viewport.size.y - frameSize.y - kFrameMargin);
    state.frameMax = ImVec2(state.frameMin.x + frameSize.x, state.frameMin.y + frameSize.y);
    state.contentCenter = ImVec2(
        (state.frameMin.x + state.frameMax.x) * 0.5f,
        (state.frameMin.y + state.frameMax.y) * 0.5f);

    const float usableWidth = std::max(8.0f, frameSize.x - kInnerPadding * 2.0f);
    const float usableHeight = std::max(8.0f, frameSize.y - kInnerPadding * 2.0f);
    state.pixelsPerLogicalUnit = std::min(usableWidth / paddedWidth, usableHeight / paddedHeight);

    const ImVec2 contentSize(paddedWidth * state.pixelsPerLogicalUnit, paddedHeight * state.pixelsPerLogicalUnit);
    state.contentMin = ImVec2(
        state.contentCenter.x - contentSize.x * 0.5f,
        state.contentCenter.y - contentSize.y * 0.5f);
    state.contentMax = ImVec2(
        state.contentCenter.x + contentSize.x * 0.5f,
        state.contentCenter.y + contentSize.y * 0.5f);
    state.logicalMin = logicalBounds.min;
    state.logicalMax = logicalBounds.max;
    state.logicalCenter = logicalBounds.center;
    state.valid = true;
    return state;
}

bool IsPointInsideRect(const ImVec2 point, const ImVec2 min, const ImVec2 max)
{
    return point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y;
}

struct ViewportToolbarLayout
{
    ImVec2 toolbarMin {};
    ImVec2 toolbarMax {};
    ImVec2 buttonPos {};
    ImVec2 buttonSize {};
    ImVec2 textPos {};
    std::array<char, 96> infoLabel {};
};

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

ViewportToolbarLayout ComputeViewportToolbarLayout(const ImVec2 viewportOrigin,
                                                   const float zoom,
                                                   const std::optional<mfd::Vec2>& mouseLogical)
{
    ViewportToolbarLayout layout;

    const ImGuiStyle& style = ImGui::GetStyle();
    const std::string infoLabel = FormatViewportToolbarInfoLabel(zoom, mouseLogical);
    std::snprintf(layout.infoLabel.data(), layout.infoLabel.size(), "%s", infoLabel.c_str());

    const ImVec2 buttonLabelSize = ImGui::CalcTextSize("?");
    const ImVec2 textSize = ImGui::CalcTextSize(layout.infoLabel.data());
    layout.buttonSize = ImVec2(
        buttonLabelSize.x + style.FramePadding.x * 2.0f,
        buttonLabelSize.y + style.FramePadding.y * 2.0f);
    layout.buttonPos = ImVec2(viewportOrigin.x + 12.0f, viewportOrigin.y + 12.0f);
    layout.textPos = ImVec2(layout.buttonPos.x + layout.buttonSize.x + style.ItemSpacing.x,
                            layout.buttonPos.y + style.FramePadding.y);
    layout.toolbarMin = layout.buttonPos;
    layout.toolbarMax = ImVec2(
        layout.textPos.x + textSize.x,
        layout.buttonPos.y + std::max(layout.buttonSize.y, textSize.y + style.FramePadding.y * 2.0f));
    return layout;
}

void DrawViewportHelpPopupContent(const bool libraryPreview)
{
    if (libraryPreview)
    {
        ImGui::TextDisabled("Reticle studio");
        ImGui::Separator();
        ImGui::BulletText("Mouse wheel: zoom the studio camera.");
        ImGui::BulletText("Right-drag: pan the studio camera.");
        ImGui::BulletText("Click a primitive: focus it in the studio and inspector.");
        ImGui::BulletText("Left-drag the handles: edit the selected primitive geometry.");
    }
    else
    {
        ImGui::TextDisabled("Page preview");
        ImGui::Separator();
        ImGui::BulletText("Ctrl+click: add or remove one reticle from the selection.");
        ImGui::BulletText("Esc: clear the current page-reticle selection.");
        ImGui::BulletText("Drag a selected reticle: move the whole selected group.");
        ImGui::BulletText("Blue handle: rotate the selected reticle.");
        ImGui::BulletText("Corner handles: scale the selected reticle.");
        ImGui::BulletText("Mouse wheel: zoom the page camera.");
        ImGui::BulletText("Right-drag: pan the page camera.");
        ImGui::BulletText("Right-click: open selection and clipping actions.");
        ImGui::BulletText("Left-drag the minimap viewport: navigate the page.");
    }

    ImGui::Separator();
    ImGui::TextDisabled("Global shortcuts");
    ImGui::BulletText("Save: Ctrl+S");
    ImGui::BulletText("Undo: Ctrl+Z");
    ImGui::BulletText("Copy / Cut / Paste selected page reticles: Ctrl+C / Ctrl+X / Ctrl+V");
    ImGui::BulletText("Delete current selection: Del");
}

void DrawViewportToolbar(const ImVec2 viewportOrigin,
                         const float zoom,
                         const std::optional<mfd::Vec2>& mouseLogical,
                         const char* buttonId,
                         const char* popupId,
                         const bool libraryPreview)
{
    const ViewportToolbarLayout layout = ComputeViewportToolbarLayout(viewportOrigin, zoom, mouseLogical);
    ImGui::SetCursorScreenPos(layout.buttonPos);
    if (ImGui::Button(buttonId, layout.buttonSize))
    {
        ImGui::OpenPopup(popupId);
    }
    ShowItemTooltip("Open a compact summary of the controls available in this view.");

    ImGui::SetCursorScreenPos(layout.textPos);
    ImGui::TextDisabled("%s", layout.infoLabel.data());

    ImGui::SetNextWindowPos(ImVec2(layout.buttonPos.x, layout.buttonPos.y + layout.buttonSize.y + 6.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopup(popupId))
    {
        DrawViewportHelpPopupContent(libraryPreview);
        ImGui::EndPopup();
    }
}

ImVec2 ToMinimapScreen(const PageMinimapState& minimap, const mfd::Vec2 logical)
{
    return ImVec2(
        minimap.contentCenter.x + (logical.x - minimap.logicalCenter.x) * minimap.pixelsPerLogicalUnit,
        minimap.contentCenter.y - (logical.y - minimap.logicalCenter.y) * minimap.pixelsPerLogicalUnit);
}

mfd::Vec2 ToMinimapLogical(const PageMinimapState& minimap, const ImVec2 screen)
{
    const float clampedX = std::clamp(screen.x, minimap.contentMin.x, minimap.contentMax.x);
    const float clampedY = std::clamp(screen.y, minimap.contentMin.y, minimap.contentMax.y);
    return {
        minimap.logicalCenter.x + (clampedX - minimap.contentCenter.x) / minimap.pixelsPerLogicalUnit,
        minimap.logicalCenter.y - (clampedY - minimap.contentCenter.y) / minimap.pixelsPerLogicalUnit};
}

std::size_t CountEditorLayerAssignments(const mfd::PageDefinition& page, const std::string_view layerId)
{
    if (layerId.empty())
    {
        return 0;
    }

    return static_cast<std::size_t>(std::count_if(
        page.staticReticles.begin(),
        page.staticReticles.end(),
        [layerId](const mfd::ReticleGroup& reticle)
        {
            return reticle.layerId == layerId;
         }));
}

std::size_t CountDynamicLayerBindings(const mfd::PageDefinition& page, const std::string_view layerId)
{
    if (layerId.empty())
    {
        return 0;
    }

    return static_cast<std::size_t>(std::count_if(
        page.dynamicReticleBindings.begin(),
        page.dynamicReticleBindings.end(),
        [layerId](const mfd::DynamicReticleLayerBinding& binding)
        {
            return mfd::PageNamesEqual(binding.layerId, layerId);
        }));
}

std::string SummarizeDynamicLayerBindings(const mfd::PageDefinition& page, const std::string_view layerId)
{
    if (layerId.empty())
    {
        return {};
    }

    std::string summary;
    for (const mfd::DynamicReticleLayerBinding& binding : page.dynamicReticleBindings)
    {
        if (!mfd::PageNamesEqual(binding.layerId, layerId))
        {
            continue;
        }

        if (!summary.empty())
        {
            summary += ", ";
        }
        summary += binding.templateId;
    }

    return summary;
}

void RenameEditorLayerReferences(mfd::PageDefinition& page,
                                 const std::string_view previousLayerId,
                                 const std::string& nextLayerId)
{
    if (previousLayerId.empty() || previousLayerId == nextLayerId)
    {
        return;
    }

    for (auto& reticle : page.staticReticles)
    {
        if (reticle.layerId == previousLayerId)
        {
            reticle.layerId = nextLayerId;
        }
    }

    for (auto& binding : page.dynamicReticleBindings)
    {
        if (binding.layerId == previousLayerId)
        {
            binding.layerId = nextLayerId;
        }
    }
}

std::size_t ClearEditorLayerReferences(mfd::PageDefinition& page, const std::string_view removedLayerId)
{
    if (removedLayerId.empty())
    {
        return 0;
    }

    std::size_t clearedCount = 0;
    for (auto& reticle : page.staticReticles)
    {
        if (reticle.layerId == removedLayerId)
        {
            reticle.layerId.clear();
            ++clearedCount;
        }
    }

    for (auto& binding : page.dynamicReticleBindings)
    {
        if (binding.layerId == removedLayerId)
        {
            binding.layerId.clear();
            ++clearedCount;
        }
    }

    return clearedCount;
}

std::string DefaultEditorLayerId(const mfd::PageDefinition& page)
{
    const auto visibleLayer = std::find_if(page.editor.layers.begin(),
                                           page.editor.layers.end(),
                                           [](const mfd::EditorLayerDefinition& layer)
                                           {
                                               return layer.visible && !layer.id.empty();
                                           });
    if (visibleLayer != page.editor.layers.end())
    {
        return visibleLayer->id;
    }

    const auto firstLayer = std::find_if(page.editor.layers.begin(),
                                         page.editor.layers.end(),
                                         [](const mfd::EditorLayerDefinition& layer)
                                         {
                                             return !layer.id.empty();
                                         });
    return firstLayer == page.editor.layers.end() ? std::string {} : firstLayer->id;
}

void BootstrapEditorLayersForPage(mfd::PageDefinition& page)
{
    if (page.layers.empty())
    {
        page.layers.push_back(mfd::PageLayerDefinition {std::string(mfd::kDefaultPageLayerId)});
    }

    std::vector<mfd::EditorLayerDefinition> synchronizedStates;
    synchronizedStates.reserve(page.layers.size());
    for (const mfd::PageLayerDefinition& runtimeLayer : page.layers)
    {
        const auto iterator = std::find_if(page.editor.layers.begin(),
                                           page.editor.layers.end(),
                                           [&runtimeLayer](const mfd::EditorLayerDefinition& layer)
                                           {
                                               return mfd::PageNamesEqual(layer.id, runtimeLayer.id);
                                           });
        synchronizedStates.push_back(mfd::EditorLayerDefinition {
            runtimeLayer.id,
            iterator == page.editor.layers.end() ? true : iterator->visible});
    }
    page.editor.layers = std::move(synchronizedStates);

    const std::string fallbackLayerId = page.layers.front().id;
    for (auto& reticle : page.staticReticles)
    {
        if (reticle.layerId.empty() || mfd::FindPageLayerDefinition(page, reticle.layerId) == nullptr)
        {
            reticle.layerId = fallbackLayerId;
        }
    }

    for (auto& binding : page.dynamicReticleBindings)
    {
        if (binding.layerId.empty() || mfd::FindPageLayerDefinition(page, binding.layerId) == nullptr)
        {
            binding.layerId = fallbackLayerId;
        }
    }
}

bool IsRaylibControlChordPressed(const std::initializer_list<int> keys)
{
    const bool controlDown = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    if (!controlDown)
    {
        return false;
    }

    if (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT) ||
        IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER))
    {
        return false;
    }

    for (const int key : keys)
    {
        if (IsKeyPressed(key))
        {
            return true;
        }
    }

    return false;
}
} // namespace

EditorApplication::EditorApplication()
{
    sidebarWidth_ = kSidebarWidth;
    inspectorWidth_ = kInspectorWidth;
    tutorial_ = std::make_unique<EditorTutorialController>(*this);
    CopyTextBuffer(newPageDraft_.name, "NewPage");
    CopyTextBuffer(newPageDraft_.title, "New Page");
    CopyTextBuffer(newPageDraft_.fileName, DefaultProjectAssetFolder("assets/pages/new_page.json").string());
    CopyTextBuffer(newWindowDraft_.windowFile, DefaultProjectAssetFolder("assets/windows/new_window.json").string());
    CopyTextBuffer(newWindowDraft_.title, "New MFD Window");
    CopyTextBuffer(newWindowDraft_.reticleLibraryFolder, DefaultProjectAssetFolder("assets/reticles").string());
    CopyTextBuffer(newWindowDraft_.commandAddress, "127.0.0.1");
    CopyTextBuffer(newWindowDraft_.feedbackAddress, "127.0.0.1");
    CopyTextBuffer(newWindowDraft_.firstPageName, "Page1");
    CopyTextBuffer(newWindowDraft_.firstPageTitle, "Page 1");
    CopyTextBuffer(newWindowDraft_.firstPageFile, DefaultProjectAssetFolder("assets/pages/page1.json").string());
    CopyTextBuffer(newLibraryReticleDraft_.id, "new_reticle");
    CopyTextBuffer(duplicateLibraryReticleDraft_.id, "reticle_copy");
    ResetPagePreviewView();
    ResetLibraryPreviewView();
    RebuildStatus("Open one window asset or create assets to begin authoring.", false);
    tutorial_->LoadProgress();
}

EditorApplication::~EditorApplication()
{
    ReleasePreviewGpuResources();
}

float EditorApplication::ViewportState::LogicalScale() const noexcept
{
    return 0.5f * std::min(size.x, size.y);
}

ImVec2 EditorApplication::ViewportState::ToScreen(const mfd::Vec2 logical) const noexcept
{
    const mfd::Vec2 viewed = mfd::ApplyPageView(logical, view);
    return ImVec2(
        origin.x + size.x * 0.5f + viewed.x * LogicalScale(),
        origin.y + size.y * 0.5f - viewed.y * LogicalScale());
}

mfd::Vec2 EditorApplication::ViewportState::ToLogical(const ImVec2 screen) const noexcept
{
    const float scale = LogicalScale();
    if (!valid || scale <= 0.0f)
    {
        return {};
    }

    const float viewedX = (screen.x - origin.x - size.x * 0.5f) / scale;
    const float viewedY = -(screen.y - origin.y - size.y * 0.5f) / scale;
    const float zoom = mfd::SanitizeZoom(view.zoom);

    return {
        viewedX / zoom + view.center.x,
        viewedY / zoom + view.center.y};
}

int EditorApplication::Run()
{
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1720, 980, "MFDStudio");
    SetExitKey(KEY_NULL);
    TryApplyEditorWindowIcon();
    SetWindowMinSize(1320, 760);
    SetTargetFPS(60);

    rlImGuiSetup(true);
    ApplyEditorTheme();

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(Color {8, 13, 18, 255});

        bool imguiBegun = false;
        try
        {
            rlImGuiBegin();
            imguiBegun = true;
            HandleShortcuts();
            DrawMenuBar();
            DrawRootLayout();
            DrawPopups();
        }
        catch (const std::exception& exception)
        {
            lastRuntimeError_ = exception.what();
            RebuildStatus(lastRuntimeError_, true);
        }
        catch (...)
        {
            lastRuntimeError_ = "Unknown exception inside mfd_editor";
            RebuildStatus(lastRuntimeError_, true);
        }

        if (imguiBegun)
        {
            rlImGuiEnd();
        }

        EndDrawing();
    }

    rlImGuiShutdown();
    ReleasePreviewGpuResources();
    CloseWindow();
    return 0;
}

bool EditorApplication::LoadWindowConfiguration(const std::filesystem::path& path)
{
    try
    {
        loaded_ = loader_.LoadWindowConfiguration(path);
        for (auto& page : loaded_.document.pages)
        {
            BootstrapEditorLayersForPage(page);
        }
        files_.pageFiles = loaded_.window.pageFiles;
        files_.removedPageFiles.clear();
        files_.removedTemplateFiles.clear();
        std::string error;
        if (!editor::DiscoverReticleTemplateFiles(loaded_.window.reticleLibraryFolder, files_, &error))
        {
            throw std::runtime_error(error);
        }

        ApplyPreviewFontFile(loaded_.window.fontFile);
        selection_ = {};
        layerFocusState_ = {};
        SelectPage(DefaultPageIndex(loaded_.document.pages));
        ResetLibraryPreviewView();
        undoStack_.clear();
        InvalidateReticleUsageHighlightCache();
        windowFile_ = path;
        lastRuntimeError_.clear();
        RebuildStatus("Editor loaded '" + loaded_.window.title + "'.", false);
        return true;
    }
    catch (const std::exception& exception)
    {
        RebuildStatus(exception.what(), true);
        return false;
    }
}

bool EditorApplication::SaveAll()
{
    if (!HasOpenWindow())
    {
        RebuildStatus("No window asset is open yet. Create or open one before saving.", true);
        return false;
    }

    std::string error;
    if (!editor::SaveEditorDocument(loaded_, files_, &error))
    {
        RebuildStatus("Save failed: " + error, true);
        return false;
    }

    RebuildStatus("Window, pages and library saved.", false);
    return true;
}

void EditorApplication::Undo()
{
    if (undoStack_.empty())
    {
        RebuildStatus("Nothing to undo.", true);
        return;
    }

    UndoSnapshot snapshot = std::move(undoStack_.back());
    undoStack_.pop_back();
    loaded_ = std::move(snapshot.loaded);
    files_ = std::move(snapshot.files);
    selection_ = std::move(snapshot.selection);
    pagePreviewView_ = snapshot.pagePreviewView;
    pagePreviewView_.zoom = mfd::SanitizeZoom(pagePreviewView_.zoom);
    libraryPreviewView_ = snapshot.libraryPreviewView;
    libraryPreviewView_.zoom = mfd::SanitizeZoom(libraryPreviewView_.zoom);

    if (selection_.pageIndex >= static_cast<int>(loaded_.document.pages.size()))
    {
        SelectPage(loaded_.document.pages.empty() ? 0 : static_cast<int>(loaded_.document.pages.size()) - 1);
    }

    SanitizeLayerFocusForActivePage();
    SanitizePageReticleSelectionForCurrentFocus();
    RebuildStatus("Undo applied.", false);
    InvalidateReticleUsageHighlightCache();
}

void EditorApplication::PushUndoSnapshot()
{
    if (undoStack_.size() >= 64)
    {
        undoStack_.erase(undoStack_.begin());
    }

    undoStack_.push_back(UndoSnapshot {loaded_, files_, selection_, pagePreviewView_, libraryPreviewView_});
    InvalidateReticleUsageHighlightCache();
}

void EditorApplication::HandleShortcuts()
{
    HandleDroppedFiles();

    const ImGuiIO& io = ImGui::GetIO();

    if (!io.WantTextInput && CanToggleFullscreenPagePreview() && ImGui::IsKeyPressed(ImGuiKey_F11))
    {
        ToggleFullscreenPagePreview();
    }

    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_RouteGlobal | ImGuiInputFlags_RouteOverActive) ||
        IsRaylibControlChordPressed({KEY_S}))
    {
        SaveAll();
    }

    const bool undoShortcutTriggered =
        ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z, ImGuiInputFlags_RouteGlobal | ImGuiInputFlags_RouteOverActive) ||
        IsRaylibControlChordPressed({KEY_Z, KEY_W});
    if (undoShortcutTriggered)
    {
        Undo();
    }

    if (!io.WantTextInput &&
        (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_C, ImGuiInputFlags_RouteGlobal) ||
         IsRaylibControlChordPressed({KEY_C})))
    {
        CopySelectedPageReticles();
    }

    if (!io.WantTextInput &&
        (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_X, ImGuiInputFlags_RouteGlobal) ||
         IsRaylibControlChordPressed({KEY_X})))
    {
        CutSelectedPageReticles();
    }

    if (!io.WantTextInput &&
        (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_V, ImGuiInputFlags_RouteGlobal) ||
         IsRaylibControlChordPressed({KEY_V})))
    {
        PasteCopiedPageReticles();
    }

    if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete))
    {
        DeleteSelection();
    }

    if (!io.WantTextInput &&
        !ImGui::IsPopupOpen((const char*)nullptr, ImGuiPopupFlags_AnyPopupId) &&
        ImGui::IsKeyPressed(ImGuiKey_Escape))
    {
        if (fullscreenPreviewController_.IsActive())
        {
            ToggleFullscreenPagePreview();
            return;
        }

        if (const mfd::PageDefinition* page = ActivePage();
            page != nullptr && layerFocusController_.IsFocusActive(*page, layerFocusState_))
        {
            ClearLayerFocus(true);
            return;
        }

        if (selection_.kind == SelectionKind::PageReticle && !SelectedPageReticleIndices().empty())
        {
            SelectPage(selection_.pageIndex);
            RebuildStatus("Page reticle selection cleared.", false);
        }
    }
}

void EditorApplication::DeleteSelection()
{
    if (selection_.kind == SelectionKind::Page)
    {
        OpenPageManagementPopup(PageManagementAction::DeleteAsset, selection_.pageIndex);
        return;
    }

    if (selection_.kind == SelectionKind::PageReticle)
    {
        mfd::PageDefinition* page = ActivePage();
        const std::vector<int> selectedIndices = SelectedPageReticleIndices();
        if (page == nullptr || selectedIndices.empty())
        {
            RebuildStatus("No page reticle selected to delete.", true);
            return;
        }

        PushUndoSnapshot();
        std::vector<int> descendingIndices = selectedIndices;
        std::sort(descendingIndices.begin(), descendingIndices.end(), std::greater<int>());

        std::vector<std::string> removedReticleIds;
        removedReticleIds.reserve(descendingIndices.size());
        for (const int reticleIndex : descendingIndices)
        {
            if (reticleIndex < 0 || reticleIndex >= static_cast<int>(page->staticReticles.size()))
            {
                continue;
            }

            removedReticleIds.push_back(page->staticReticles[static_cast<std::size_t>(reticleIndex)].id);
            page->staticReticles.erase(page->staticReticles.begin() + reticleIndex);
        }

        SelectPage(selection_.pageIndex);

        if (removedReticleIds.size() == 1U)
        {
            RebuildStatus("Reticle '" + removedReticleIds.front() + "' removed from page '" + page->name + "'.", false);
        }
        else
        {
            RebuildStatus(
                std::to_string(removedReticleIds.size()) + " reticles removed from page '" + page->name + "'.",
                false);
        }
        return;
    }

    if (selection_.kind == SelectionKind::LibraryReticle || selection_.kind == SelectionKind::LibraryPrimitive)
    {
        DeleteSelectedLibraryReticle();
        return;
    }

    RebuildStatus("Select a page, a page reticle or a library reticle to delete it.", true);
}

void EditorApplication::HandleDroppedFiles()
{
    if (!IsFileDropped())
    {
        return;
    }

    const FilePathList droppedFiles = LoadDroppedFiles();
    std::optional<std::filesystem::path> importedPageFile;
    for (unsigned int index = 0; index < droppedFiles.count; ++index)
    {
        const std::filesystem::path candidate = std::filesystem::path(droppedFiles.paths[index]).lexically_normal();
        if (Lowercase(candidate.extension().string()) == ".json")
        {
            importedPageFile = candidate;
            break;
        }
    }

    if (!importedPageFile.has_value())
    {
        RebuildStatus("Drop one page JSON file to open the import workflow.", true);
        UnloadDroppedFiles(droppedFiles);
        return;
    }

    if (!HasOpenWindow())
    {
        RebuildStatus("Open or create one window before importing a page asset.", true);
        UnloadDroppedFiles(droppedFiles);
        return;
    }

    OpenPageImportPopup(*importedPageFile);
    if (droppedFiles.count > 1U)
    {
        RebuildStatus("Opened the import workflow for the first dropped JSON file.", false);
    }
    UnloadDroppedFiles(droppedFiles);
}

void EditorApplication::OpenPageManagementPopup(const PageManagementAction action, const int pageIndex)
{
    if (loaded_.document.pages.empty() ||
        pageIndex < 0 ||
        pageIndex >= static_cast<int>(loaded_.document.pages.size()))
    {
        RebuildStatus("No page selected.", true);
        return;
    }

    pageManagementPopup_.action = action;
    pageManagementPopup_.openRequested = true;
    pageManagementPopup_.pageIndex = pageIndex;
    pageManagementPopup_.replacementPageIndex = SuggestReplacementPageIndex(loaded_.document.pages, pageIndex);
    pageManagementPopup_.allowOutsideAssetsRoot = false;
    pageManagementPopup_.confirmDelete = false;
}

void EditorApplication::OpenPageImportPopup(std::filesystem::path sourcePageFile)
{
    if (!HasOpenWindow())
    {
        RebuildStatus("Open or create one window before importing a page asset.", true);
        return;
    }

    pageImportPopup_.sourcePageFile = std::move(sourcePageFile);
    pageImportPopup_.openRequested = true;
}

void EditorApplication::OpenPageRenamePopup(const int pageIndex)
{
    if (loaded_.document.pages.empty() ||
        pageIndex < 0 ||
        pageIndex >= static_cast<int>(loaded_.document.pages.size()))
    {
        RebuildStatus("No page selected.", true);
        return;
    }

    pageRenamePopup_.pageIndex = pageIndex;
    pageRenamePopup_.openRequested = true;
    CopyTextBuffer(pageRenamePopup_.newName, loaded_.document.pages[static_cast<std::size_t>(pageIndex)].name);
}

editor::PageImportRequest EditorApplication::BuildPageImportRequest(const std::filesystem::path& sourcePageFile) const
{
    return editor::PageImportRequest {
        sourcePageFile,
        CurrentPageImportTargetFolder(windowFile_, files_),
        loaded_.window.reticleLibraryFolder};
}

editor::RenamePageRequest EditorApplication::BuildPageRenameRequest(const int pageIndex,
                                                                    const std::string_view newPageName) const
{
    const std::filesystem::path pageFile =
        pageIndex >= 0 && pageIndex < static_cast<int>(files_.pageFiles.size())
            ? files_.pageFiles[static_cast<std::size_t>(pageIndex)]
            : loaded_.window.sourceFile;
    return editor::RenamePageRequest {
        pageIndex,
        std::string(newPageName),
        ResolveAssetRootForPath(pageFile)};
}

void EditorApplication::OpenReticleRenamePopup(std::string templateId)
{
    if (templateId.empty())
    {
        RebuildStatus("No library reticle selected.", true);
        return;
    }

    const auto iterator = loaded_.document.reticleLibrary.find(templateId);
    if (iterator == loaded_.document.reticleLibrary.end())
    {
        RebuildStatus("The selected library reticle is no longer available.", true);
        return;
    }

    reticleRenamePopup_.currentTemplateId = std::move(templateId);
    reticleRenamePopup_.openRequested = true;
    reticleRenamePopup_.renameTemplateFile = true;
    CopyTextBuffer(reticleRenamePopup_.newName, iterator->second.id.empty() ? iterator->first : iterator->second.id);
}

void EditorApplication::OpenReticleExtractionPopup()
{
    const std::vector<int> selectedIndices = SelectedPageReticleIndices();
    if (selectedIndices.empty())
    {
        RebuildStatus("Select one or more page reticles before extracting them as a reusable reticle.", true);
        return;
    }

    const mfd::PageDefinition* page = ActivePage();
    if (page == nullptr)
    {
        RebuildStatus("Select one page before extracting a reusable reticle.", true);
        return;
    }

    std::string suggestedTemplateId = "extracted_reticle";
    const int firstReticleIndex = selectedIndices.front();
    if (firstReticleIndex >= 0 && firstReticleIndex < static_cast<int>(page->staticReticles.size()))
    {
        const mfd::ReticleGroup& firstReticle = page->staticReticles[static_cast<std::size_t>(firstReticleIndex)];
        if (!firstReticle.sourceTemplateId.empty())
        {
            suggestedTemplateId = firstReticle.sourceTemplateId + "_extract";
        }
        else if (!firstReticle.id.empty())
        {
            suggestedTemplateId = firstReticle.id + "_extract";
        }
    }

    CopyTextBuffer(reticleExtractionPopup_.templateId, suggestedTemplateId);
    if (loaded_.window.reticleLibraryFolder.empty())
    {
        CopyTextBuffer(reticleExtractionPopup_.templateFile, "");
    }
    else
    {
        CopyTextBuffer(reticleExtractionPopup_.templateFile,
                       editor::DefaultTemplateFilePath(loaded_.window.reticleLibraryFolder, suggestedTemplateId).string());
    }

    reticleExtractionPopup_.openRequested = true;
}

editor::RenameReticleRequest EditorApplication::BuildReticleRenameRequest(const std::string_view oldTemplateId,
                                                                          const std::string_view newTemplateId,
                                                                          const bool renameTemplateFile) const
{
    std::filesystem::path templateFile = loaded_.window.reticleLibraryFolder;
    if (const auto iterator = files_.templateFiles.find(std::string(oldTemplateId)); iterator != files_.templateFiles.end())
    {
        templateFile = iterator->second;
    }

    return editor::RenameReticleRequest {
        std::string(oldTemplateId),
        std::string(newTemplateId),
        ResolveAssetRootForPath(templateFile),
        renameTemplateFile};
}

editor::ReticleExtractionRequest EditorApplication::BuildReticleExtractionRequest() const
{
    std::filesystem::path requestedTemplateFile;
    if (reticleExtractionPopup_.templateFile.front() != '\0')
    {
        requestedTemplateFile = std::filesystem::path(reticleExtractionPopup_.templateFile.data()).lexically_normal();
    }

    return editor::ReticleExtractionRequest {
        selection_.pageIndex,
        SelectedPageReticleIndices(),
        reticleExtractionPopup_.templateId.data(),
        requestedTemplateFile};
}

editor::FullscreenPreviewLayoutState EditorApplication::CaptureFullscreenPreviewLayoutState() const
{
    editor::FullscreenPreviewLayoutState state;
    state.sidebarVisible = sidebarVisible_;
    state.inspectorVisible = inspectorVisible_;
    state.pageContextVisible = pagePreviewViewOptions_.showPageContext;
    state.layerInspectorVisible = pagePreviewViewOptions_.showLayerInspector;
    state.minimapVisible = pagePreviewViewOptions_.showMinimap;
    state.problemsVisible = pagePreviewViewOptions_.showProblemsPanel;
    state.sidebarWidth = sidebarWidth_;
    state.inspectorWidth = inspectorWidth_;
    state.pageContextWidth = libraryStudioPageWidth_;
    return state;
}

void EditorApplication::ApplyFullscreenPreviewLayoutState(const editor::FullscreenPreviewLayoutState& state)
{
    sidebarVisible_ = state.sidebarVisible;
    inspectorVisible_ = state.inspectorVisible;
    pagePreviewViewOptions_.showPageContext = state.pageContextVisible;
    pagePreviewViewOptions_.showLayerInspector = state.layerInspectorVisible;
    pagePreviewViewOptions_.showMinimap = state.minimapVisible;
    pagePreviewViewOptions_.showProblemsPanel = state.problemsVisible;
    sidebarWidth_ = state.sidebarWidth > 0.0f ? state.sidebarWidth : sidebarWidth_;
    inspectorWidth_ = state.inspectorWidth > 0.0f ? state.inspectorWidth : inspectorWidth_;
    libraryStudioPageWidth_ = state.pageContextWidth;
}

void EditorApplication::ToggleFullscreenPagePreview()
{
    if (!CanToggleFullscreenPagePreview())
    {
        return;
    }

    const editor::FullscreenPreviewTransition transition =
        fullscreenPreviewController_.Toggle(CaptureFullscreenPreviewLayoutState());
    if (!transition.changed)
    {
        return;
    }

    ApplyFullscreenPreviewLayoutState(transition.state);
    RebuildStatus(fullscreenPreviewController_.IsActive() ? "Fullscreen preview enabled." : "Fullscreen preview disabled.",
                  false);
}

void EditorApplication::OpenDesignExportPopup()
{
    const std::filesystem::path defaultFolder =
        windowFile_.empty() ? DefaultProjectAssetFolder("MFDStudioDesignExport")
                            : (windowFile_.parent_path() / std::filesystem::path("MFDStudioDesignExport"));
    CopyTextBuffer(designExportPopup_.outputFolder, defaultFolder.lexically_normal().string());
    designExportPopup_.exportCompleted = false;
    designExportPopup_.exportedFolder.clear();
    designExportPopup_.warnings.clear();
    designExportPopup_.openRequested = true;
}

editor::DesignExportRequest EditorApplication::BuildDesignExportRequest() const
{
    editor::DesignExportRequest request;
    request.outputFolder = std::filesystem::path(designExportPopup_.outputFolder.data()).lexically_normal();
    request.windowFile = windowFile_;
    request.loaded = HasOpenWindow() ? &loaded_ : nullptr;
    request.files = HasOpenWindow() ? &files_ : nullptr;
    request.exportMarkdownIcd = designExportPopup_.exportMarkdownIcd;
    request.exportExplodedViews = designExportPopup_.exportExplodedViews;
    request.includeCanvasCoordinates = designExportPopup_.includeCanvasCoordinates;
    request.includeCppSnippets = designExportPopup_.includeCppSnippets;
    request.includeStrobe = designExportPopup_.includeStrobe;
    request.includeBlink = designExportPopup_.includeBlink;
    request.includePrimitiveIds = designExportPopup_.includePrimitiveIds;
    request.includeMappingHash = designExportPopup_.includeMappingHash;
    return request;
}

bool EditorApplication::ExecuteDesignExportPlan(const editor::DesignExportPlan& plan)
{
    if (!plan.canExecute)
    {
        RebuildStatus(plan.error.empty() ? "The design export cannot execute." : plan.error, true);
        return false;
    }

    const editor::DesignExportResult result = designExportService_.Execute(plan);
    designExportPopup_.warnings = result.warnings;
    designExportPopup_.exportCompleted = true;
    designExportPopup_.exportedFolder = result.outputFolder;

    const bool hasErrors = !result.warnings.empty() &&
                           !std::filesystem::exists(plan.readmeFile) &&
                           !std::filesystem::exists(plan.windowIcdFile);
    if (hasErrors)
    {
        RebuildStatus("Design export failed. Review the popup warnings.", true);
        return false;
    }

    RebuildStatus("Design export created in '" + result.outputFolder.string() + "'.", false);
    return true;
}

bool EditorApplication::ExecutePageRemovePlan(const editor::PageRemovePlan& plan)
{
    if (!plan.canExecute)
    {
        RebuildStatus(plan.error.empty() ? "The page cannot be removed from the current window." : plan.error, true);
        return false;
    }

    PushUndoSnapshot();
    std::string error;
    if (!pageManagementService_.Execute(plan, loaded_, files_, &error))
    {
        RebuildStatus("Remove page failed: " + error, true);
        return false;
    }

    if (loaded_.document.pages.empty())
    {
        selection_ = {};
    }
    else
    {
        selection_.pageIndex = plan.nextSelectedPageIndex;
        selection_.pageReticleIndex = -1;
        selection_.pageReticleIndices.clear();
        SelectPage(plan.nextSelectedPageIndex);
    }

    if (plan.replacementPageName.empty())
    {
        RebuildStatus("Page '" + plan.pageName + "' removed from the current window.", false);
    }
    else
    {
        RebuildStatus("Page '" + plan.pageName + "' removed. Default page switched to '" +
                          plan.replacementPageName + "'.",
                      false);
    }

    InvalidateReticleUsageHighlightCache();
    return true;
}

bool EditorApplication::ExecutePageImportPlan(const editor::PageImportPlan& plan)
{
    if (!plan.canExecute)
    {
        RebuildStatus(plan.error.empty() ? "The selected page cannot be imported." : plan.error, true);
        return false;
    }

    PushUndoSnapshot();

    editor::PageImportResult result;
    std::string error;
    if (!pageImportService_.Execute(plan, loaded_, files_, &result, &error))
    {
        RebuildStatus(error.empty() ? "Importing the selected page failed." : error, true);
        return false;
    }

    SelectPage(result.importedPageIndex);
    std::string status = "Page '" + result.pageName + "' imported";
    if (!result.importedTemplateIds.empty())
    {
        status += " with " + std::to_string(result.importedTemplateIds.size()) + " reticle template";
        if (result.importedTemplateIds.size() != 1U)
        {
            status += "s";
        }
    }
    status += ". Use File > Save to persist the staged JSON assets.";
    RebuildStatus(status, false);
    InvalidateReticleUsageHighlightCache();
    return true;
}

bool EditorApplication::ExecutePageRenamePlan(const editor::RenamePagePlan& plan)
{
    if (!plan.canExecute)
    {
        RebuildStatus(plan.error.empty() ? "The selected page cannot be renamed globally." : plan.error, true);
        return false;
    }

    editor::RenamePageResult result;
    std::string error;
    if (!pageRenameService_.Execute(plan, loaded_, files_, &result, &error))
    {
        RebuildStatus(error.empty() ? "Renaming the selected page globally failed." : error, true);
        return false;
    }

    std::string status = "Page '" + plan.oldPageName + "' renamed to '" + plan.newPageName + "'";
    if (!plan.references.empty())
    {
        status += " across " + std::to_string(plan.references.size()) + " window reference";
        if (plan.references.size() != 1U)
        {
            status += "s";
        }
    }
    if (result.updatedWindowCount > 0U)
    {
        status += " with " + std::to_string(result.updatedWindowCount) + " window JSON update";
        if (result.updatedWindowCount != 1U)
        {
            status += "s";
        }
    }
    status += ". Regenerate the generated client API if this page is exposed there.";
    RebuildStatus(status, false);
    InvalidateReticleUsageHighlightCache();
    return true;
}

bool EditorApplication::ExecuteReticleRenamePlan(const editor::RenameReticlePlan& plan)
{
    if (!plan.canExecute)
    {
        RebuildStatus(plan.error.empty() ? "The selected reticle template cannot be renamed globally." : plan.error, true);
        return false;
    }

    editor::RenameReticleResult result;
    std::string error;
    if (!reticleRenameService_.Execute(plan, loaded_, files_, &result, &error))
    {
        RebuildStatus(error.empty() ? "Renaming the selected reticle template globally failed." : error, true);
        return false;
    }

    if (mfd::PageNamesEqual(selection_.libraryReticleId, plan.oldReticleName))
    {
        selection_.libraryReticleId = plan.newReticleName;
    }
    if (mfd::PageNamesEqual(selection_.libraryBrowserReticleId, plan.oldReticleName))
    {
        selection_.libraryBrowserReticleId = plan.newReticleName;
    }

    std::string status = "Reticle template '" + plan.oldReticleName + "' renamed to '" + plan.newReticleName + "'";
    if (!plan.references.empty())
    {
        status += " across " + std::to_string(plan.references.size()) + " page reference";
        if (plan.references.size() != 1U)
        {
            status += "s";
        }
    }
    if (result.updatedPageCount > 0U)
    {
        status += " with " + std::to_string(result.updatedPageCount) + " page JSON update";
        if (result.updatedPageCount != 1U)
        {
            status += "s";
        }
    }
    if (result.renamedTemplateFile)
    {
        status += " and one template file move";
    }
    status += ". Regenerate the generated client API if this template is exposed there.";
    RebuildStatus(status, false);
    InvalidateReticleUsageHighlightCache();
    return true;
}

bool EditorApplication::ExecuteReticleExtractionPlan(const editor::ReticleExtractionPlan& plan)
{
    if (!plan.canExecute)
    {
        RebuildStatus(plan.error.empty() ? "The current page-reticle selection cannot be extracted yet." : plan.error, true);
        return false;
    }

    PushUndoSnapshot();

    editor::ReticleExtractionResult result;
    std::string error;
    if (!reticleExtractionService_.Execute(plan, loaded_, files_, &result, &error))
    {
        RebuildStatus(error.empty() ? "Extracting the selected page reticles failed." : error, true);
        return false;
    }

    SelectPageReticle(plan.pageIndex, result.insertedReticleIndex);
    InvalidateReticleUsageHighlightCache();

    std::string status = "Extracted " + std::to_string(result.extractedPrimitiveCount) + " primitive";
    if (result.extractedPrimitiveCount != 1U)
    {
        status += "s";
    }
    status += " into reticle template '" + result.templateId + "'. Use File > Save to persist the staged JSON asset.";
    RebuildStatus(status, false);
    return true;
}

bool EditorApplication::ExecutePageDeletePlan(const editor::PageDeletePlan& plan)
{
    if (!plan.canExecute)
    {
        RebuildStatus(plan.error.empty() ? "The page asset cannot be deleted." : plan.error, true);
        return false;
    }

    PushUndoSnapshot();
    std::string error;
    if (!pageManagementService_.Execute(plan, loaded_, files_, &error))
    {
        RebuildStatus("Delete page failed: " + error, true);
        return false;
    }

    if (loaded_.document.pages.empty())
    {
        selection_ = {};
    }
    else
    {
        selection_.pageIndex = plan.nextSelectedPageIndex;
        selection_.pageReticleIndex = -1;
        selection_.pageReticleIndices.clear();
        SelectPage(plan.nextSelectedPageIndex);
    }

    if (plan.replacementPageName.empty())
    {
        RebuildStatus("Page '" + plan.pageName + "' removed and marked for deletion on the next save.", false);
    }
    else
    {
        RebuildStatus("Page '" + plan.pageName + "' removed and marked for deletion. Default page switched to '" +
                          plan.replacementPageName + "'.",
                      false);
    }

    return true;
}

mfd::ColorRgba ScaleAlpha(const mfd::ColorRgba color, const float factor)
{
    const float clampedFactor = std::clamp(factor, 0.0f, 1.0f);
    return mfd::ColorRgba {
        color.r,
        color.g,
        color.b,
        static_cast<std::uint8_t>(std::clamp(static_cast<int>(std::lround(static_cast<float>(color.a) * clampedFactor)),
                                             0,
                                             255))};
}

mfd::ReticleGroup MakeDimmedReticlePreviewCopy(const mfd::ReticleGroup& source, const float alphaFactor)
{
    mfd::ReticleGroup copy = source;
    if (copy.overrides.color.has_value())
    {
        copy.overrides.color = ScaleAlpha(*copy.overrides.color, alphaFactor);
    }
    if (copy.overrides.fillColor.has_value())
    {
        copy.overrides.fillColor = ScaleAlpha(*copy.overrides.fillColor, alphaFactor);
    }

    for (mfd::Primitive& primitive : copy.primitives)
    {
        primitive.style.color = ScaleAlpha(primitive.style.color, alphaFactor);
        primitive.style.fillColor = ScaleAlpha(primitive.style.fillColor, alphaFactor);
    }

    return copy;
}

void EditorApplication::DeleteSelectedLibraryReticle()
{
    mfd::ReticleGroup* reticle = SelectedLibraryReticle();
    if (reticle == nullptr)
    {
        RebuildStatus("No library reticle selected to delete.", true);
        return;
    }

    const std::string reticleId = reticle->id;
    for (const auto& page : loaded_.document.pages)
    {
        for (const auto& pageReticle : page.staticReticles)
        {
            if (pageReticle.sourceTemplateId == reticleId)
            {
                RebuildStatus("Cannot delete library reticle '" + reticleId +
                                  "' because page '" + page.name + "' still uses it.",
                              true);
                return;
            }
        }

        if (page.strobe.has_value() && page.strobe->reticle.sourceTemplateId == reticleId)
        {
            RebuildStatus("Cannot delete library reticle '" + reticleId +
                              "' because strobe on page '" + page.name + "' still uses it.",
                          true);
            return;
        }

        if (std::any_of(page.dynamicReticleBindings.begin(),
                        page.dynamicReticleBindings.end(),
                        [&reticleId](const mfd::DynamicReticleLayerBinding& binding)
                        {
                            return mfd::PageNamesEqual(binding.templateId, reticleId);
                        }))
        {
            RebuildStatus("Cannot delete library reticle '" + reticleId +
                              "' because page '" + page.name +
                              "' exposes it as one generated dynamic template.",
                          true);
            return;
        }
    }

    PushUndoSnapshot();

    if (const auto fileIt = files_.templateFiles.find(reticleId); fileIt != files_.templateFiles.end())
    {
        files_.removedTemplateFiles.push_back(fileIt->second.lexically_normal());
        files_.templateFiles.erase(fileIt);
    }

    loaded_.document.reticleLibrary.erase(reticleId);

    if (loaded_.document.reticleLibrary.empty())
    {
        SelectPage(std::clamp(selection_.pageIndex,
                              0,
                              std::max(0, static_cast<int>(loaded_.document.pages.size()) - 1)));
        RebuildStatus("Library reticle '" + reticleId + "' deleted.", false);
        return;
    }

    std::vector<std::string> remainingTemplateIds;
    remainingTemplateIds.reserve(loaded_.document.reticleLibrary.size());
    for (const auto& entry : loaded_.document.reticleLibrary)
    {
        remainingTemplateIds.push_back(entry.first);
    }
    std::sort(remainingTemplateIds.begin(), remainingTemplateIds.end());
    SelectLibraryReticle(remainingTemplateIds.front());
    RebuildStatus("Library reticle '" + reticleId + "' deleted.", false);
}

void EditorApplication::DrawMenuBar()
{
    using editor::tutorial::TutorialStepId;
    const bool hasOpenWindow = HasOpenWindow();

    if (!ImGui::BeginMainMenuBar())
    {
        return;
    }

    const bool fileMenuOpen = ImGui::BeginMenu("File");
    tutorial_->DrawHalo("menu_file", "Click File", "Open the top-level document actions used by this tutorial step.");
    if (ImGui::IsItemClicked() && tutorial_->MatchesTarget("menu_file"))
    {
        tutorial_->AdvancePhase();
    }
    if (fileMenuOpen)
    {
        const bool newWindowRequested = ImGui::MenuItem("New window from scratch");
        ShowItemTooltip("Create a brand-new window JSON and optional first page directly from the editor.");
        tutorial_->DrawHalo(
            "menu_file_new_window",
            "Click New window from scratch",
            "Open the creation dialog prefilled with the tutorial window settings.");
        if (newWindowRequested)
        {
            if (tutorial_->MatchesTarget("menu_file_new_window"))
            {
                tutorial_->AdvancePhase();
            }
            OpenNewWindowPopup();
        }

        const bool openWindowRequested = ImGui::MenuItem("Open window asset...");
        ShowItemTooltip("Browse to one authored window JSON through the native file explorer.");
        if (openWindowRequested)
        {
            OpenWindowAssetFromFileExplorer();
        }

        const bool exportMenuOpen = ImGui::BeginMenu("Export", hasOpenWindow);
        ShowItemTooltip("Open export workflows for designer-facing deliverables.");
        tutorial_->DrawHalo(
            "menu_file_export",
            "Open Export",
            "Open the export workflows exposed by the editor before reviewing the design-export popup.");
        if (ImGui::IsItemClicked() && tutorial_->MatchesTarget("menu_file_export"))
        {
            tutorial_->AdvancePhase();
        }
        if (exportMenuOpen)
        {
            const bool exportDesignRequested = ImGui::MenuItem("Export design...");
            ShowItemTooltip("Generate Markdown ICD files and exploded designer views for the current window.");
            tutorial_->DrawHalo(
                "menu_file_export_design",
                "Click Export design...",
                "Open the design export popup and review its options without writing anything yet.");
            if (exportDesignRequested)
            {
                if (tutorial_->MatchesTarget("menu_file_export_design"))
                {
                    tutorial_->AdvancePhase();
                }
                OpenDesignExportPopup();
            }
            ImGui::EndMenu();
        }

        ImGui::Separator();
        const bool saveRequested = ImGui::MenuItem("Save", "Ctrl+S", false, hasOpenWindow);
        ShowItemTooltip("Write the window file, page files and reticle template files back to disk.");
        tutorial_->DrawHalo(
            "menu_file_save",
            "Click Save",
            "Persist the authored tutorial assets before moving to the code review steps.");
        if (saveRequested)
        {
            const bool tutorialSaveMatched = tutorial_->MatchesTarget("menu_file_save");
            if (SaveAll() && tutorialSaveMatched)
            {
                tutorial_->CompleteStep();
            }
        }

        const bool canReloadCurrent = hasOpenWindow && std::filesystem::exists(windowFile_);
        const bool reloadRequested = ImGui::MenuItem("Reload current", nullptr, false, canReloadCurrent);
        ShowItemTooltip("Reload the current window asset from disk and discard unsaved editor changes.");
        if (reloadRequested)
        {
            LoadWindowConfiguration(windowFile_);
        }
        ImGui::EndMenu();
    }
    else if (tutorial_->ShouldResetFileMenuPhaseOnClose())
    {
        tutorial_->ResetPhase();
    }

    if (ImGui::BeginMenu("Edit"))
    {
        const bool undoRequested = ImGui::MenuItem("Undo", "Ctrl+Z", false, !undoStack_.empty());
        ShowItemTooltip("Restore the previous editor snapshot.");
        if (undoRequested)
        {
            Undo();
        }

        const bool hasPageReticleSelection = !SelectedPageReticleIndices().empty();
        const bool copyReticlesRequested =
            ImGui::MenuItem("Copy selected page reticles", "Ctrl+C", false, hasPageReticleSelection);
        ShowItemTooltip("Copy the selected page reticle instances so they can be pasted on the active page.");
        if (copyReticlesRequested)
        {
            CopySelectedPageReticles();
        }

        const bool cutReticlesRequested =
            ImGui::MenuItem("Cut selected page reticles", "Ctrl+X", false, hasPageReticleSelection);
        ShowItemTooltip("Copy the selected page reticle instances into the clipboard, then remove them from the page.");
        if (cutReticlesRequested)
        {
            CutSelectedPageReticles();
        }

        const bool canPastePageReticles = ActivePage() != nullptr && !pageReticleClipboard_.empty();
        const bool pasteReticlesRequested =
            ImGui::MenuItem("Paste page reticles", "Ctrl+V", false, canPastePageReticles);
        ShowItemTooltip("Paste copied page reticles onto the active page.");
        if (pasteReticlesRequested)
        {
            PasteCopiedPageReticles();
        }

        const bool deleteReticlesRequested =
            ImGui::MenuItem("Delete selected page reticles", "Del", false, hasPageReticleSelection);
        ShowItemTooltip("Remove the selected page reticle instances from the active page.");
        if (deleteReticlesRequested)
        {
            DeleteSelection();
        }
        ImGui::EndMenu();
    }

    const bool pageMenuOpen = ImGui::BeginMenu("Page", hasOpenWindow);
    tutorial_->DrawHalo("menu_page", "Click Page", "Open the page-authoring actions used by the current tutorial step.");
    if (ImGui::IsItemClicked() && tutorial_->MatchesTarget("menu_page"))
    {
        tutorial_->AdvancePhase();
    }
    if (pageMenuOpen)
    {
        const bool newPageRequested = ImGui::MenuItem("New page");
        ShowItemTooltip("Create a new page and its backing JSON file.");
        tutorial_->DrawHalo(
            "menu_page_new",
            "Click New page",
            "Open the page dialog with the tutorial page values already prepared for you.");
        if (newPageRequested)
        {
            if (tutorial_->MatchesTarget("menu_page_new"))
            {
                tutorial_->AdvancePhase();
            }
            OpenNewPagePopup();
        }

        const bool importPageRequested = ImGui::MenuItem("Import page...");
        ShowItemTooltip("Import one external page JSON and stage its reticle dependencies into the current window.");
        tutorial_->DrawHalo(
            "menu_page_import",
            "Click Import page...",
            "Open the import workflow so you know where shared page ingestion starts. You can cancel the native file picker.");
        if (importPageRequested)
        {
            if (tutorial_->MatchesTarget("menu_page_import"))
            {
                tutorial_->CompleteStep();
            }
            OpenPageAssetImportFromFileExplorer();
        }

        const bool renamePageRequested = ImGui::MenuItem("Rename current page globally...", nullptr, false, ActivePage() != nullptr);
        ShowItemTooltip("Rename the current page asset safely across the current asset tree and update shared window references.");
        tutorial_->DrawHalo(
            "menu_page_rename",
            "Click Rename current page globally...",
            "Open the safe page-rename popup, review the scanned references, then close it without executing the rename.");
        if (renamePageRequested)
        {
            if (tutorial_->MatchesTarget("menu_page_rename"))
            {
                tutorial_->AdvancePhase();
            }
            OpenPageRenamePopup(selection_.pageIndex);
        }

        const bool removePageRequested = ImGui::MenuItem("Remove current page from window", nullptr, false, ActivePage() != nullptr);
        ShowItemTooltip("Detach the current page from the window while keeping its JSON file.");
        if (removePageRequested)
        {
            OpenPageManagementPopup(PageManagementAction::RemoveFromWindow, selection_.pageIndex);
        }

        const bool deletePageRequested = ImGui::MenuItem("Delete current page asset...", "Del", false, ActivePage() != nullptr);
        ShowItemTooltip("Open the confirmation flow that removes the page and marks its JSON file for deletion on the next save.");
        if (deletePageRequested)
        {
            OpenPageManagementPopup(PageManagementAction::DeleteAsset, selection_.pageIndex);
        }
        ImGui::EndMenu();
    }
    else if (tutorial_->ShouldResetPageMenuPhaseOnClose())
    {
        tutorial_->ResetPhase();
    }

    const bool reticleMenuOpen = ImGui::BeginMenu("Reticle", hasOpenWindow);
    tutorial_->DrawHalo("menu_reticle", "Click Reticle", "Open the reticle-template actions used by the tutorial.");
    if (ImGui::IsItemClicked() && tutorial_->MatchesTarget("menu_reticle"))
    {
        tutorial_->AdvancePhase();
    }
    if (reticleMenuOpen)
    {
        const bool newLibraryReticleRequested = ImGui::MenuItem("New library reticle from primitive");
        ShowItemTooltip("Create a new shared reticle template.");
        tutorial_->DrawHalo(
            "menu_reticle_new",
            "Click New library reticle from primitive",
            "Open the reticle dialog and create the tutorial template shown in this step.");
        if (newLibraryReticleRequested)
        {
            if (tutorial_->MatchesTarget("menu_reticle_new"))
            {
                tutorial_->AdvancePhase();
            }
            OpenNewLibraryReticlePopup();
        }

        const bool hasFocusedLibraryReticle =
            (selection_.kind == SelectionKind::LibraryReticle || selection_.kind == SelectionKind::LibraryPrimitive) &&
            SelectedLibraryReticle() != nullptr;

        const bool duplicateReticleRequested =
            ImGui::MenuItem("Duplicate selected library reticle", nullptr, false, hasFocusedLibraryReticle);
        ShowItemTooltip("Duplicate the focused library reticle under a new template id.");
        if (duplicateReticleRequested)
        {
            OpenDuplicateLibraryReticlePopup();
        }

        const bool renameReticleRequested =
            ImGui::MenuItem("Rename selected library reticle globally...", nullptr, false, hasFocusedLibraryReticle);
        ShowItemTooltip("Rename the focused library reticle template safely across the current asset tree and every page that references it.");
        tutorial_->DrawHalo(
            "menu_reticle_rename",
            "Click Rename selected library reticle globally...",
            "Open the safe reticle-rename popup, review the shared references, then close it without executing the rename.");
        if (renameReticleRequested)
        {
            if (tutorial_->MatchesTarget("menu_reticle_rename"))
            {
                tutorial_->AdvancePhase();
            }
            OpenReticleRenamePopup(selection_.libraryReticleId);
        }

        const bool deleteReticleRequested =
            ImGui::MenuItem("Delete selected library reticle", "Del", false, hasFocusedLibraryReticle);
        ShowItemTooltip("Delete the focused library reticle template from the shared library.");
        if (deleteReticleRequested)
        {
            DeleteSelectedLibraryReticle();
        }
        ImGui::EndMenu();
    }
    else if (tutorial_->ShouldResetReticleMenuPhaseOnClose())
    {
        tutorial_->ResetPhase();
    }

    if (ImGui::BeginMenu("Help"))
    {
        const bool tutorialRequested = ImGui::MenuItem("Tutorial", nullptr, tutorial_->IsCoachVisible());
        ShowItemTooltip("Open the guided discovery mode for the editor and tutorial assets.");
        if (tutorialRequested)
        {
            tutorial_->OpenFlow();
        }
        ImGui::EndMenu();
    }

    ImGui::TextDisabled("|");
    const char* titleLabel = hasOpenWindow && !loaded_.window.title.empty() ? loaded_.window.title.c_str() : "No asset open";
    ImGui::Text("%s", titleLabel);
    if (hasOpenWindow)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("[%s]", windowFile_.filename().string().c_str());
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", statusMessage_.c_str());

    ImGui::EndMainMenuBar();
}

void EditorApplication::DrawRootLayout()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin(
        "MFD Editor Root",
        nullptr,
        ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar(2);

    const float totalWidth = ImGui::GetContentRegionAvail().x;
    const float totalHeight = ImGui::GetContentRegionAvail().y;
    const float splitterCount = static_cast<float>((sidebarVisible_ ? 1 : 0) + (inspectorVisible_ ? 1 : 0));
    const float reservedInspectorWidth = inspectorVisible_ ? inspectorWidth_ : 0.0f;

    if (sidebarVisible_)
    {
        const float maxSidebarWidth = std::max(
            kMinSidebarWidth, totalWidth - reservedInspectorWidth - kMinWorkspaceWidth - splitterCount * editor::ui::kPaneSplitterWidth);
        sidebarWidth_ = std::floor(std::clamp(sidebarWidth_, kMinSidebarWidth, maxSidebarWidth));
    }

    if (inspectorVisible_)
    {
        const float maxInspectorWidth = std::max(
            kMinInspectorWidth, totalWidth - (sidebarVisible_ ? sidebarWidth_ : 0.0f) - kMinWorkspaceWidth -
                                   splitterCount * editor::ui::kPaneSplitterWidth);
        inspectorWidth_ = std::floor(std::clamp(inspectorWidth_, kMinInspectorWidth, maxInspectorWidth));
    }

    if (sidebarVisible_)
    {
        ImGui::BeginChild("Sidebar", ImVec2(sidebarWidth_, 0.0f), true);
        DrawSidebar();
        ImGui::EndChild();

        ImGui::SameLine();
        if (DrawVerticalSplitter("##SidebarSplitter", totalHeight))
        {
            const float nextSidebarWidth = sidebarWidth_ + ImGui::GetIO().MouseDelta.x;
            const float nextMaxSidebarWidth =
                std::max(kMinSidebarWidth,
                         totalWidth - (inspectorVisible_ ? inspectorWidth_ : 0.0f) - kMinWorkspaceWidth -
                             splitterCount * editor::ui::kPaneSplitterWidth);
            sidebarWidth_ = std::floor(std::clamp(nextSidebarWidth, kMinSidebarWidth, nextMaxSidebarWidth));
        }

        ImGui::SameLine();
    }

    const float workspaceWidth = std::floor(std::max(kMinWorkspaceWidth,
                                                     totalWidth - (sidebarVisible_ ? sidebarWidth_ : 0.0f) -
                                                         (inspectorVisible_ ? inspectorWidth_ : 0.0f) -
                                                         splitterCount * editor::ui::kPaneSplitterWidth));
    ImGui::BeginChild("Workspace", ImVec2(workspaceWidth, 0.0f), true);
    DrawWorkspace();
    ImGui::EndChild();

    if (inspectorVisible_)
    {
        ImGui::SameLine();
        if (DrawVerticalSplitter("##InspectorSplitter", totalHeight))
        {
            const float nextInspectorWidth = inspectorWidth_ - ImGui::GetIO().MouseDelta.x;
            const float nextMaxInspectorWidth =
                std::max(kMinInspectorWidth,
                         totalWidth - (sidebarVisible_ ? sidebarWidth_ : 0.0f) - kMinWorkspaceWidth -
                             splitterCount * editor::ui::kPaneSplitterWidth);
            inspectorWidth_ = std::floor(std::clamp(nextInspectorWidth, kMinInspectorWidth, nextMaxInspectorWidth));
        }

        ImGui::SameLine();
        const float inspectorPanelWidth = std::max(0.0f, std::floor(ImGui::GetContentRegionAvail().x));
        ImGui::BeginChild("Inspector", ImVec2(inspectorPanelWidth, 0.0f), true);
        DrawInspector();
        ImGui::EndChild();
    }

    ImGui::End();
}

bool EditorApplication::IsLibraryStudioWorkspaceVisible() const
{
    using editor::tutorial::TutorialStepId;

    const bool forcePagePreviewTutorialWorkspace =
        tutorial_->IsStep(static_cast<int>(TutorialStepId::ShowLayerInspector)) ||
        tutorial_->IsStep(static_cast<int>(TutorialStepId::ShowMinimap)) ||
        tutorial_->IsStep(static_cast<int>(TutorialStepId::ShowReticleUsageHighlights)) ||
        tutorial_->IsStep(static_cast<int>(TutorialStepId::ShowProblemsPanel)) ||
        tutorial_->IsStep(static_cast<int>(TutorialStepId::ToggleFullscreenPreview)) ||
        tutorial_->IsStep(static_cast<int>(TutorialStepId::InspectReticleRenameWorkflow)) ||
        tutorial_->IsStep(static_cast<int>(TutorialStepId::InspectDesignExportWorkflow));
    return !forcePagePreviewTutorialWorkspace &&
           (selection_.kind == SelectionKind::LibraryReticle || selection_.kind == SelectionKind::LibraryPrimitive);
}

bool EditorApplication::CanToggleFullscreenPagePreview() const
{
    return fullscreenPreviewController_.IsActive() || (HasOpenWindow() && !IsLibraryStudioWorkspaceVisible());
}

void EditorApplication::DrawSidebar()
{
    ImGui::TextColored(ImVec4(0.33f, 0.86f, 0.78f, 1.0f), "MFD Editor");
    ImGui::TextDisabled("Work directly in the page visualization.");
    ImGui::Separator();

    if (!HasOpenWindow())
    {
        ImGui::TextWrapped("No authored window is open yet.");
        ImGui::Spacing();
        ImGui::TextDisabled("Use File > Open window asset... or File > New window from scratch.");

        if (!lastRuntimeError_.empty())
        {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "Runtime: %s", lastRuntimeError_.c_str());
        }
        return;
    }

    DrawPageTree();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    DrawLibraryTree();

    if (!lastRuntimeError_.empty())
    {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "Runtime: %s", lastRuntimeError_.c_str());
    }
}

void EditorApplication::DrawWorkspace()
{
    if (!HasOpenWindow())
    {
        tutorial_->DrawCoach();
        DrawEmptyWorkspacePlaceholder();
        return;
    }

    const std::vector<std::string> pagePreviewProblems = BuildPagePreviewProblemMessages();
    const bool hasPagePreviewProblems = !pagePreviewProblems.empty();
    const bool libraryStudioVisible = IsLibraryStudioWorkspaceVisible();
    const bool fullscreenPreviewActive = fullscreenPreviewController_.IsActive();

    if (fullscreenPreviewActive)
    {
        ImGui::TextColored(ImVec4(0.72f, 0.86f, 0.95f, 1.0f), "Page preview");
        DrawPagePreviewHeaderControls("##FullscreenPagePreviewViewMenu", hasPagePreviewProblems);
        ImGui::TextDisabled("Fullscreen preview keeps the page canvas interactive. Press F11 or Esc to restore the editor layout.");
        tutorial_->DrawCoach();
        ImGui::Separator();

        DrawPagePreviewWorkspace(
            pagePreviewProblems,
            "FullscreenPagePreviewPanel", "FullscreenPageLayersPanel", "FullscreenPageProblemsPanel", true, true);
        return;
    }

    if (libraryStudioVisible)
    {
        const float totalWidth = ImGui::GetContentRegionAvail().x;
        const float totalHeight = ImGui::GetContentRegionAvail().y;

        if (!pagePreviewViewOptions_.showPageContext)
        {
            DrawReticleStudioPanel();
            return;
        }

        if (libraryStudioPageWidth_ <= 0.0f)
        {
            libraryStudioPageWidth_ = std::max(kMinPageContextWidth, totalWidth * 0.56f);
        }

        const float maxPageWidth = std::max(kMinPageContextWidth,
                                            totalWidth - kMinReticleStudioWidth - editor::ui::kPaneSplitterWidth);
        libraryStudioPageWidth_ = std::floor(std::clamp(libraryStudioPageWidth_, kMinPageContextWidth, maxPageWidth));
        const float pageWidth = libraryStudioPageWidth_;
        const float studioWidth = std::max(kMinReticleStudioWidth,
                                           std::floor(totalWidth - pageWidth - editor::ui::kPaneSplitterWidth));

        ImGui::BeginChild("PageContextPanel", ImVec2(pageWidth, 0.0f), true);
        ImGui::TextColored(ImVec4(0.72f, 0.86f, 0.95f, 1.0f), "Page context");
        DrawPagePreviewHeaderControls("##PageContextViewMenu", hasPagePreviewProblems, false);
        ImGui::TextDisabled("Keep drag & drop and page composition visible while editing the library reticle.");
        tutorial_->DrawCoach();
        ImGui::Separator();

        DrawPagePreviewWorkspace(pagePreviewProblems,
                                 "PageContextPreviewPanel",
                                 "PageContextLayersPanel",
                                 "PageContextProblemsPanel",
                                 selection_.kind == SelectionKind::PageReticle,
                                 selection_.kind == SelectionKind::PageReticle);
        ImGui::EndChild();

        ImGui::SameLine();
        if (DrawVerticalSplitter("##WorkspaceSplitter", totalHeight))
        {
            const float nextPageWidth = libraryStudioPageWidth_ + ImGui::GetIO().MouseDelta.x;
            const float nextMaxPageWidth = std::max(kMinPageContextWidth,
                                                    totalWidth - kMinReticleStudioWidth - editor::ui::kPaneSplitterWidth);
            libraryStudioPageWidth_ = std::floor(std::clamp(nextPageWidth, kMinPageContextWidth, nextMaxPageWidth));
        }

        ImGui::SameLine();
        DrawReticleStudioPanel(studioWidth);
        return;
    }

    ImGui::TextColored(ImVec4(0.72f, 0.86f, 0.95f, 1.0f), "Page preview");
    DrawPagePreviewHeaderControls("##MainPagePreviewViewMenu", hasPagePreviewProblems);
    ImGui::TextDisabled("Use View to toggle preview-only overlays without touching authored JSON assets.");
    tutorial_->DrawCoach();
    ImGui::Separator();

    DrawPagePreviewWorkspace(
        pagePreviewProblems,
        "MainPagePreviewPanel", "MainPageLayersPanel", "MainPageProblemsPanel", true, true);
}

void EditorApplication::DrawPagePreviewWorkspace(const std::vector<std::string>& pagePreviewProblems,
                                                 const char* previewChildId,
                                                 const char* layersChildId,
                                                 const char* problemsChildId,
                                                 const bool drawPreviewOverlays,
                                                 const bool handlePreviewInteraction)
{
    const ImVec2 available = ImGui::GetContentRegionAvail();
    editor::WorkspaceLayoutRequest layoutRequest;
    layoutRequest.width = std::max(0.0f, std::floor(available.x));
    layoutRequest.height = std::max(0.0f, std::floor(available.y));
    layoutRequest.spacing = ImGui::GetStyle().ItemSpacing.y;
    layoutRequest.showLeadingPanel = pagePreviewViewOptions_.showLayerInspector;
    layoutRequest.leadingPanelWidth = kLayerInspectorDockWidth;
    layoutRequest.minLeadingPanelWidth = 196.0f;
    layoutRequest.showBottomPanel = pagePreviewViewOptions_.showProblemsPanel;
    layoutRequest.bottomPanelHeight = kPreviewProblemsDockHeight;
    layoutRequest.minBottomPanelHeight = 88.0f;
    layoutRequest.minCenterWidth = 220.0f;
    layoutRequest.minCenterHeight = 168.0f;
    const editor::WorkspaceLayoutResult layout = editor::ComputeWorkspaceLayout(layoutRequest);
    const mfd::PageDefinition* activePage = ActivePage();

    if (layout.leadingPanel.IsVisible())
    {
        ImGui::BeginChild(layersChildId, ImVec2(layout.leadingPanel.width, layout.leadingPanel.height), true);
        if (activePage != nullptr)
        {
            DrawLayerInspectorPanel(*activePage);
        }
        else
        {
            ImGui::TextColored(ImVec4(0.85f, 0.91f, 0.96f, 1.0f), "Layer Inspector");
            ImGui::TextDisabled("Open one page to inspect editor layers.");
        }
        ImGui::EndChild();
        ImGui::SameLine();
    }

    ImGui::BeginGroup();
    if (layout.previewPanel.IsVisible())
    {
        ImGui::BeginChild(previewChildId, ImVec2(layout.previewPanel.width, layout.previewPanel.height), true);

        ViewportState pageViewport;
        pageViewport.origin = ImGui::GetCursorScreenPos();
        pageViewport.size = ImGui::GetContentRegionAvail();
        pageViewport.valid = pageViewport.size.x > 8.0f && pageViewport.size.y > 8.0f;

        if (activePage != nullptr)
        {
            pageViewport.view = pagePreviewView_;
        }

        if (pageViewport.valid && activePage != nullptr)
        {
            DrawPagePreview(pageViewport);
            if (drawPreviewOverlays)
            {
                DrawPreviewOverlays(pageViewport);
            }
            if (handlePreviewInteraction)
            {
                HandlePreviewInteraction(pageViewport);
            }
            DrawPageReticleContextMenu();
        }
        else
        {
            ImGui::TextDisabled("No active page to preview.");
        }

        ImGui::EndChild();
    }

    if (layout.bottomPanel.IsVisible())
    {
        ImGui::BeginChild(problemsChildId, ImVec2(layout.bottomPanel.width, layout.bottomPanel.height), true);
        DrawProblemsPanel(pagePreviewProblems);
        ImGui::EndChild();
    }
    ImGui::EndGroup();
}

void EditorApplication::DrawReticleStudioPanel(const float width)
{
    const float panelWidth = width > 0.0f ? width : std::max(0.0f, std::floor(ImGui::GetContentRegionAvail().x));
    ImGui::BeginChild("ReticleStudioPanel", ImVec2(panelWidth, 0.0f), true);
    ImGui::TextColored(ImVec4(0.72f, 0.86f, 0.95f, 1.0f), "Reticle studio");

    const ImGuiStyle& style = ImGui::GetStyle();
    const float buttonWidth = ImGui::CalcTextSize("View").x + style.FramePadding.x * 2.0f;
    ImGui::SameLine();
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x - buttonWidth));
    if (ImGui::Button("View##ReticleStudioDisplay"))
    {
        ImGui::OpenPopup(kReticleStudioDisplayPopupId);
    }

    if (ImGui::BeginPopup(kReticleStudioDisplayPopupId))
    {
        ImGui::Checkbox("Show page context", &pagePreviewViewOptions_.showPageContext);
        ImGui::Checkbox("Show primitive names", &libraryStudioShowPrimitiveLabels_);
        ImGui::Checkbox("Show gizmos", &libraryStudioShowGizmos_);
        ImGui::EndPopup();
    }

    ImGui::TextDisabled("Click a primitive to focus it, drag the handles to edit its geometry.");
    ImGui::Separator();

    ViewportState studioViewport;
    studioViewport.origin = ImGui::GetCursorScreenPos();
    studioViewport.size = ImGui::GetContentRegionAvail();
    studioViewport.valid = studioViewport.size.x > 8.0f && studioViewport.size.y > 8.0f;
    studioViewport.view = libraryPreviewView_;

    if (studioViewport.valid)
    {
        DrawLibraryPreview(studioViewport);
        DrawLibraryPreviewOverlays(studioViewport);
        HandleLibraryPreviewInteraction(studioViewport);
    }
    ImGui::EndChild();
}

void EditorApplication::DrawEmptyWorkspacePlaceholder()
{
    const ImVec2 available = ImGui::GetContentRegionAvail();
    if (available.x <= 8.0f || available.y <= 8.0f)
    {
        return;
    }

    const char* headline = "Open or create assets";
    const char* description =
        "Start with a new authored window or browse to an existing window JSON.\n"
        "Nothing is loaded automatically when the editor starts, but you can launch the guided tutorial.";

    const ImVec2 headlineSize = ImGui::CalcTextSize(headline);
    const ImVec2 descriptionSize = ImGui::CalcTextSize(description);
    const float buttonRowWidth = 420.0f;
    const float totalHeight = headlineSize.y + descriptionSize.y + 126.0f;
    const ImVec2 start(
        std::max(0.0f, (available.x - std::max(std::max(headlineSize.x, descriptionSize.x), buttonRowWidth)) * 0.5f),
        std::max(0.0f, (available.y - totalHeight) * 0.5f));

    ImGui::SetCursorPos(start);
    ImGui::TextColored(ImVec4(0.72f, 0.86f, 0.95f, 1.0f), "%s", headline);
    ImGui::SetCursorPosX(start.x);
    ImGui::TextDisabled("%s", description);
    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::SetCursorPosX(start.x);
    if (AccentButton("Open window asset..."))
    {
        OpenWindowAssetFromFileExplorer();
    }

    ImGui::SetCursorPosX(start.x);
    if (ImGui::Button("New window from scratch", ImVec2(220.0f, 0.0f)))
    {
        OpenNewWindowPopup();
    }

    ImGui::SetCursorPosX(start.x);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.54f, 0.61f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.66f, 0.73f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.12f, 0.44f, 0.52f, 1.00f));
    if (ImGui::Button("Launch the tutorial", ImVec2(220.0f, 0.0f)))
    {
        tutorial_->OpenFlow();
    }
    ImGui::PopStyleColor(3);
}

void EditorApplication::DrawInspector()
{
    if (!HasOpenWindow())
    {
        ImGui::TextDisabled("No asset is open.");
        ImGui::TextWrapped("Open one existing window asset or create a new window to edit pages, reticles and strobe settings.");
        return;
    }

    switch (selection_.kind)
    {
    case SelectionKind::Page:
        DrawPageInspector();
        break;
    case SelectionKind::PageReticle:
        DrawPageReticleInspector();
        break;
    case SelectionKind::LibraryReticle:
        DrawLibraryReticleInspector();
        break;
    case SelectionKind::LibraryPrimitive:
        DrawLibraryReticleInspector();
        ImGui::Separator();
        DrawLibraryPrimitiveInspector();
        break;
    }
}

void EditorApplication::DrawPageTree()
{
    ImGui::TextColored(ImVec4(0.72f, 0.86f, 0.95f, 1.0f), "Pages");
    ShowItemTooltip("Browse authored pages and select a page or one of its page reticles.");
    const editor::ReticleUsageHighlightResult* usageHighlight =
        pagePreviewViewOptions_.highlightReticleUsages ? ResolveReticleUsageHighlight() : nullptr;

    for (int pageIndex = 0; pageIndex < static_cast<int>(loaded_.document.pages.size()); ++pageIndex)
    {
        const auto& page = loaded_.document.pages[static_cast<std::size_t>(pageIndex)];
        ImGuiTreeNodeFlags flags =
            ImGuiTreeNodeFlags_OpenOnArrow |
            ImGuiTreeNodeFlags_OpenOnDoubleClick |
            ImGuiTreeNodeFlags_SpanAvailWidth;

        if (selection_.kind == SelectionKind::Page && selection_.pageIndex == pageIndex)
        {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        const std::string pageLabel = page.title.empty() ? page.name : page.title;
        const bool pageUsesSelectedReticle =
            usageHighlight != nullptr &&
            std::any_of(usageHighlight->pages.begin(),
                        usageHighlight->pages.end(),
                        [pageIndex](const editor::ReticleUsageHighlightPage& usagePage)
                        {
                            return usagePage.currentPageIndex == pageIndex;
                        });
        if (pageUsesSelectedReticle)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.78f, 0.38f, 1.0f));
        }
        const bool open = ImGui::TreeNodeEx((pageLabel + "##page_" + std::to_string(pageIndex)).c_str(), flags);
        if (pageUsesSelectedReticle)
        {
            ImGui::PopStyleColor();
        }
        ShowItemTooltip("Click to focus the page inspector. Use the arrow or double-click to expand its reticles.");
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        {
            SelectPage(pageIndex);
        }

        if (ImGui::BeginPopupContextItem(("PageContextMenu##" + std::to_string(pageIndex)).c_str()))
        {
            if (ImGui::MenuItem("Rename page globally..."))
            {
                SelectPage(pageIndex);
                OpenPageRenamePopup(pageIndex);
            }

            if (ImGui::MenuItem("Remove page from window"))
            {
                SelectPage(pageIndex);
                OpenPageManagementPopup(PageManagementAction::RemoveFromWindow, pageIndex);
            }

            if (ImGui::MenuItem("Delete page asset..."))
            {
                SelectPage(pageIndex);
                OpenPageManagementPopup(PageManagementAction::DeleteAsset, pageIndex);
            }

            ImGui::EndPopup();
        }

        if (open)
        {
            ImGui::TextDisabled("file: %s", pageIndex < static_cast<int>(files_.pageFiles.size())
                                              ? files_.pageFiles[static_cast<std::size_t>(pageIndex)].filename().string().c_str()
                                              : "<missing>");

            for (int reticleIndex = 0; reticleIndex < static_cast<int>(page.staticReticles.size()); ++reticleIndex)
            {
                const auto& reticle = page.staticReticles[static_cast<std::size_t>(reticleIndex)];
                const bool reticleSelectable = IsPageReticleSelectableInCurrentFocus(page, reticle);
                ImGuiTreeNodeFlags leafFlags =
                    ImGuiTreeNodeFlags_Leaf |
                    ImGuiTreeNodeFlags_NoTreePushOnOpen |
                    ImGuiTreeNodeFlags_SpanAvailWidth;
                if (HasSelectedPageReticle(pageIndex, reticleIndex))
                {
                    leafFlags |= ImGuiTreeNodeFlags_Selected;
                }

                const std::string reticleLabel =
                    (reticle.id.empty() ? "reticle" : reticle.id) + "##reticle_" +
                    std::to_string(pageIndex) + "_" + std::to_string(reticleIndex);
                ImGui::BeginDisabled(!reticleSelectable);
                ImGui::TreeNodeEx(reticleLabel.c_str(), leafFlags);
                DrawReticleHoverPreviewTooltip(
                    reticle,
                    std::string("Page reticle: ") + (reticle.id.empty() ? "reticle" : reticle.id),
                    ToRayColor(page.backgroundColor));
                if (reticleSelectable && ImGui::IsItemClicked())
                {
                    if (ImGui::GetIO().KeyCtrl)
                    {
                        TogglePageReticleSelection(pageIndex, reticleIndex);
                    }
                    else
                    {
                        SelectPageReticle(pageIndex, reticleIndex);
                    }
                }

                if (!reticle.layerId.empty())
                {
                    ImGui::SameLine();
                    const bool layerVisible = IsReticleVisibleInEditor(page, reticle);
                    ImGui::TextDisabled("[%s%s]",
                                        reticle.layerId.c_str(),
                                        layerVisible ? "" : " hidden");
                }
                ImGui::EndDisabled();
            }

            ImGui::TreePop();
        }
    }
}

void EditorApplication::DrawLibraryTree()
{
    ImGui::TextColored(ImVec4(0.72f, 0.86f, 0.95f, 1.0f), "Reticle library");
    ShowItemTooltip("Shared reticle templates that can be edited in the studio or dropped onto pages.");

    std::vector<std::string> templateIds;
    templateIds.reserve(loaded_.document.reticleLibrary.size());
    for (const auto& entry : loaded_.document.reticleLibrary)
    {
        templateIds.push_back(entry.first);
    }
    std::sort(templateIds.begin(), templateIds.end());

    for (const auto& templateId : templateIds)
    {
        const bool selected =
            selection_.libraryBrowserReticleId == templateId ||
            (((selection_.kind == SelectionKind::LibraryReticle || selection_.kind == SelectionKind::LibraryPrimitive) &&
              selection_.libraryReticleId == templateId) &&
             selection_.libraryBrowserReticleId.empty());
        ImGuiTreeNodeFlags flags =
            ImGuiTreeNodeFlags_Leaf |
            ImGuiTreeNodeFlags_NoTreePushOnOpen |
            ImGuiTreeNodeFlags_SpanAvailWidth;
        if (selected)
        {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        ImGui::TreeNodeEx((templateId + "##library").c_str(), flags);
        const auto libraryIt = loaded_.document.reticleLibrary.find(templateId);
        if (libraryIt != loaded_.document.reticleLibrary.end())
        {
            DrawReticleHoverPreviewTooltip(
                libraryIt->second,
                std::string("Library reticle: ") + templateId,
                Color {10, 18, 24, 255});
        }
        if (ImGui::IsItemClicked())
        {
            selection_.libraryBrowserReticleId = templateId;
            if (selection_.kind != SelectionKind::LibraryReticle && selection_.kind != SelectionKind::LibraryPrimitive)
            {
                selection_.libraryReticleId = templateId;
            }
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                SelectLibraryReticle(templateId);
            }
        }

        if (ImGui::BeginDragDropSource())
        {
            ImGui::SetDragDropPayload("MFD_LIBRARY_RETICLE", templateId.c_str(), templateId.size() + 1);
            ImGui::Text("Drop '%s' on the page", templateId.c_str());
            ImGui::EndDragDropSource();
        }

        if (ImGui::BeginPopupContextItem(("LibraryReticleContextMenu##" + templateId).c_str()))
        {
            if (ImGui::MenuItem("Rename reticle globally..."))
            {
                SelectLibraryReticle(templateId);
                OpenReticleRenamePopup(templateId);
            }

            if (ImGui::MenuItem("Delete library reticle"))
            {
                SelectLibraryReticle(templateId);
                DeleteSelectedLibraryReticle();
            }

            ImGui::EndPopup();
        }
    }

    if (AccentButton("New reticle"))
    {
        OpenNewLibraryReticlePopup();
    }
    ShowItemTooltip("Create a new library reticle seeded with one primitive.");
}

void EditorApplication::OpenNewPagePopup()
{
    SeedNewPageAssetDraftPath();
    showNewPagePopup_ = true;
}

void EditorApplication::OpenNewWindowPopup()
{
    SeedNewWindowAssetDraftPaths();
    showNewWindowPopup_ = true;
}

bool EditorApplication::OpenWindowAssetFromFileExplorer()
{
    const std::filesystem::path initialFolder =
        HasOpenWindow() && windowFile_.has_parent_path() ? windowFile_.parent_path() : DefaultProjectAssetFolder("assets/windows");
    std::string error;
    const std::optional<std::filesystem::path> selectedFile = editor::OpenWindowAssetFileDialog(initialFolder, &error);
    if (!selectedFile.has_value())
    {
        if (!error.empty())
        {
            RebuildStatus(error, true);
        }
        return false;
    }

    return LoadWindowConfiguration(*selectedFile);
}

bool EditorApplication::OpenPageAssetImportFromFileExplorer()
{
    if (!HasOpenWindow())
    {
        RebuildStatus("Open or create one window before importing a page asset.", true);
        return false;
    }

    const std::filesystem::path initialFolder = CurrentPageImportTargetFolder(windowFile_, files_);
    std::string error;
    const std::optional<std::filesystem::path> selectedFile = editor::OpenPageAssetFileDialog(initialFolder, &error);
    if (!selectedFile.has_value())
    {
        if (!error.empty())
        {
            RebuildStatus(error, true);
        }
        return false;
    }

    OpenPageImportPopup(*selectedFile);
    return true;
}

void EditorApplication::OpenAssetFolderPicker(const AssetFolderPickerTarget target)
{
    assetFolderPickerTarget_ = target;

    switch (target)
    {
    case AssetFolderPickerTarget::WindowFile:
        assetFolderPickerCurrentFolder_ =
            ConfiguredPathFolder(std::filesystem::path(newWindowDraft_.windowFile.data()).lexically_normal());
        if (assetFolderPickerCurrentFolder_.empty())
        {
            assetFolderPickerCurrentFolder_ = DefaultProjectAssetFolder("assets/windows");
        }
        break;

    case AssetFolderPickerTarget::ReticleLibraryFolder:
        assetFolderPickerCurrentFolder_ =
            ConfiguredPathFolder(std::filesystem::path(newWindowDraft_.reticleLibraryFolder.data()).lexically_normal());
        if (assetFolderPickerCurrentFolder_.empty())
        {
            assetFolderPickerCurrentFolder_ = DefaultProjectAssetFolder("assets/reticles");
        }
        break;

    case AssetFolderPickerTarget::FirstPageFile:
        assetFolderPickerCurrentFolder_ =
            ConfiguredPathFolder(std::filesystem::path(newWindowDraft_.firstPageFile.data()).lexically_normal());
        if (assetFolderPickerCurrentFolder_.empty())
        {
            assetFolderPickerCurrentFolder_ = DefaultProjectAssetFolder("assets/pages");
        }
        break;

    case AssetFolderPickerTarget::NewPageFile:
        assetFolderPickerCurrentFolder_ =
            ConfiguredPathFolder(std::filesystem::path(newPageDraft_.fileName.data()).lexically_normal());
        if (assetFolderPickerCurrentFolder_.empty())
        {
            assetFolderPickerCurrentFolder_ = DefaultProjectAssetFolder("assets/pages");
        }
        break;

    case AssetFolderPickerTarget::None:
    default:
        assetFolderPickerCurrentFolder_.clear();
        break;
    }

    if (assetFolderPickerCurrentFolder_.is_relative())
    {
        assetFolderPickerCurrentFolder_ = std::filesystem::absolute(assetFolderPickerCurrentFolder_).lexically_normal();
    }

    showAssetFolderPickerPopup_ = true;
}

void EditorApplication::OpenNewLibraryReticlePopup()
{
    showNewLibraryReticlePopup_ = true;
}

void EditorApplication::OpenDuplicateLibraryReticlePopup()
{
    const mfd::ReticleGroup* reticle = SelectedLibraryReticle();
    if (reticle == nullptr)
    {
        return;
    }

    CopyTextBuffer(duplicateLibraryReticleDraft_.id, reticle->id + "_copy");
    showDuplicateLibraryReticlePopup_ = true;
}

void EditorApplication::RebuildStatus(std::string message, const bool isError)
{
    statusMessage_ = std::move(message);
    statusIsError_ = isError;
}

void EditorApplication::EnsurePreviewTexture(const int width, const int height)
{
    if (previewTextureReady_ &&
        previewTexture_.texture.width == width &&
        previewTexture_.texture.height == height)
    {
        return;
    }

    ReleasePreviewTexture();
    previewTextureStencilReady_ = false;
    previewTexture_ = mfd::LoadRenderTextureWithStencil(width, height, &previewTextureStencilReady_);
    previewTextureReady_ = previewTexture_.texture.id != 0;
}

void EditorApplication::ReleasePreviewTexture()
{
    const bool windowReady = IsWindowReady();
    if (previewTextureReady_)
    {
        if (windowReady)
        {
            UnloadRenderTexture(previewTexture_);
        }
        previewTexture_ = {};
    }

    previewTextureReady_ = false;
    previewTextureStencilReady_ = false;
}

void EditorApplication::EnsureTooltipPreviewTexture(const int width, const int height)
{
    if (tooltipPreviewTextureReady_ &&
        tooltipPreviewTexture_.texture.width == width &&
        tooltipPreviewTexture_.texture.height == height)
    {
        return;
    }

    ReleaseTooltipPreviewTexture();
    tooltipPreviewTextureStencilReady_ = false;
    tooltipPreviewTexture_ =
        mfd::LoadRenderTextureWithStencil(width, height, &tooltipPreviewTextureStencilReady_);
    tooltipPreviewTextureReady_ = tooltipPreviewTexture_.texture.id != 0;
}

void EditorApplication::ReleaseTooltipPreviewTexture()
{
    const bool windowReady = IsWindowReady();
    if (tooltipPreviewTextureReady_)
    {
        if (windowReady)
        {
            UnloadRenderTexture(tooltipPreviewTexture_);
        }
        tooltipPreviewTexture_ = {};
    }

    tooltipPreviewTextureReady_ = false;
    tooltipPreviewTextureStencilReady_ = false;
}

void EditorApplication::ReleaseLayerPreviewTextures() noexcept
{
    const bool windowReady = IsWindowReady();
    for (LayerPreviewTextureSlot& slot : layerPreviewTextures_)
    {
        if (slot.ready)
        {
            if (windowReady)
            {
                UnloadRenderTexture(slot.texture);
            }
            slot.texture = {};
        }

        slot.ready = false;
        slot.stencilReady = false;
        slot.width = 0;
        slot.height = 0;
    }

    layerPreviewTextures_.clear();
}

void EditorApplication::ReleasePreviewGpuResources() noexcept
{
    previewBezierCache_.Clear();
    previewImageCache_.Clear();
    previewTextLayoutCache_.Clear();
    ReleaseLayerPreviewTextures();
    ReleaseTooltipPreviewTexture();
    ReleasePreviewTexture();
    ReleasePreviewFont();
}

const RenderTexture2D* EditorApplication::RenderLayerPreviewThumbnail(const std::size_t thumbnailIndex,
                                                                      const mfd::PageDefinition& page,
                                                                      const editor::LayerFocusStripEntry& entry,
                                                                      int width,
                                                                      int height)
{
    width = std::max(width, 1);
    height = std::max(height, 1);

    if (thumbnailIndex >= layerPreviewTextures_.size())
    {
        layerPreviewTextures_.resize(thumbnailIndex + 1U);
    }

    LayerPreviewTextureSlot& slot = layerPreviewTextures_[thumbnailIndex];
    if (!slot.ready || slot.width != width || slot.height != height)
    {
        if (slot.ready)
        {
            UnloadRenderTexture(slot.texture);
            slot.texture = {};
            slot.ready = false;
        }

        slot.stencilReady = false;
        slot.texture = mfd::LoadRenderTextureWithStencil(width, height, &slot.stencilReady);
        slot.ready = slot.texture.texture.id != 0;
        slot.width = width;
        slot.height = height;
    }

    if (!slot.ready)
    {
        return nullptr;
    }

    LogicalBounds bounds;
    auto includeReticleBounds = [&](const mfd::ReticleGroup& reticle)
    {
        IncludeLogicalBounds(bounds, ComputeReticleWorldBounds(reticle));
    };

    auto isEntryMatch = [&](const mfd::ReticleGroup& reticle) -> bool
    {
        if (entry.fullView)
        {
            return IsReticleVisibleInEditor(page, reticle);
        }

        return reticle.layerId == entry.layerId;
    };

    for (const mfd::ReticleGroup& reticle : page.staticReticles)
    {
        if (isEntryMatch(reticle))
        {
            includeReticleBounds(reticle);
        }
    }

    mfd::PageViewState previewView = bounds.valid ? MakeViewFittingBounds(bounds, width, height) : mfd::PageViewState {};
    previewView.zoom = mfd::SanitizeZoom(previewView.zoom);
    const Color background = ToRayColor(page.backgroundColor);
    const bool drawDimmedLayer = !entry.fullView && !entry.visible;

    BeginTextureMode(slot.texture);
    ClearBackground(background);
    {
        EnsurePreviewFont();
        ApplyBilinearFilterToFont(PreviewTextFont() == nullptr ? GetFontDefault() : *PreviewTextFont());
        mfd::Canvas2D canvas(width,
                             height,
                             previewView,
                             PreviewTextFont(),
                             background,
                             slot.stencilReady,
                             &previewBezierCache_,
                             &previewImageCache_,
                             &previewTextLayoutCache_);

        for (const mfd::ReticleGroup& reticle : page.staticReticles)
        {
            if (!isEntryMatch(reticle))
            {
                continue;
            }

            if (drawDimmedLayer)
            {
                canvas.DrawReticle(MakeDimmedReticlePreviewCopy(reticle, 0.38f));
            }
            else
            {
                canvas.DrawReticle(reticle);
            }
        }
    }
    EndTextureMode();

    return &slot.texture;
}

void EditorApplication::ApplyPreviewFontFile(std::filesystem::path fontFile)
{
    if (!fontFile.empty())
    {
        fontFile = fontFile.lexically_normal();
    }

    if (previewFontFile_ == fontFile)
    {
        return;
    }

    ReleasePreviewFont();
    previewFontFile_ = std::move(fontFile);
    previewFontLoadAttempted_ = false;
}

void EditorApplication::EnsurePreviewFont()
{
    if (previewFontReady_ || previewFontLoadAttempted_ || previewFontFile_.empty() || !IsWindowReady())
    {
        return;
    }

    previewFontLoadAttempted_ = true;
    const std::string path = previewFontFile_.string();
    Font loadedFont = LoadFont(path.c_str());
    if (loadedFont.texture.id == 0)
    {
        return;
    }

    ApplyBilinearFilterToFont(loadedFont);
    previewFont_ = loadedFont;
    previewFontReady_ = true;
}

void EditorApplication::ReleasePreviewFont() noexcept
{
    const bool windowReady = IsWindowReady();
    if (previewFontReady_)
    {
        if (windowReady)
        {
            UnloadFont(previewFont_);
        }
        previewFont_ = {};
        previewFontReady_ = false;
    }
}

const Font* EditorApplication::PreviewTextFont() const noexcept
{
    return previewFontReady_ ? &previewFont_ : nullptr;
}

void EditorApplication::ResetPagePreviewView() noexcept
{
    pagePreviewView_ = {};
    if (const mfd::PageDefinition* page = ActivePage(); page != nullptr)
    {
        pagePreviewView_ = page->view;
        pagePreviewView_.zoom = mfd::SanitizeZoom(pagePreviewView_.zoom);
    }
}

void EditorApplication::ResetLibraryPreviewView() noexcept
{
    libraryPreviewView_ = {};
    libraryPreviewView_.zoom = 1.0f;
}

std::vector<std::string> EditorApplication::BuildPagePreviewProblemMessages() const
{
    std::vector<std::string> messages;
    if (!HasOpenWindow())
    {
        return messages;
    }

    if (!loaded_.document.pages.empty() && files_.pageFiles.size() != loaded_.document.pages.size())
    {
        PushProblem(messages, "window", "Page file layout count must match the number of authored pages.");
    }

    std::unordered_set<std::string> pageNames;
    for (const mfd::PageDefinition& page : loaded_.document.pages)
    {
        const std::string pageId = PageProblemId(page);
        const std::string normalizedPageId =
            page.normalizedName.empty() ? NormalizeEditorIdentifier(page.name) : page.normalizedName;

        if (page.name.empty())
        {
            PushProblem(messages, pageId, "Page name cannot be empty.");
        }

        if (!pageNames.insert(normalizedPageId).second)
        {
            PushProblem(messages, pageId, "Page ids must stay unique.");
        }

        std::unordered_set<std::string> runtimeLayerIds;
        for (const mfd::PageLayerDefinition& layer : page.layers)
        {
            if (layer.id.empty())
            {
                PushProblem(messages, pageId, "Page layer ids cannot be empty.");
                continue;
            }

            if (!runtimeLayerIds.insert(NormalizeEditorIdentifier(layer.id)).second)
            {
                PushProblem(messages, pageId, "Page layer ids must stay unique inside one page.");
            }
        }

        std::unordered_set<std::string> editorLayerIds;
        for (const mfd::EditorLayerDefinition& layer : page.editor.layers)
        {
            if (layer.id.empty())
            {
                PushProblem(messages, pageId, "Editor layer ids cannot be empty.");
                continue;
            }

            const std::string normalizedLayerId = NormalizeEditorIdentifier(layer.id);
            if (!editorLayerIds.insert(normalizedLayerId).second)
            {
                PushProblem(messages, pageId, "Editor layer ids must stay unique inside one page.");
            }

            if (runtimeLayerIds.find(normalizedLayerId) == runtimeLayerIds.end())
            {
                PushProblem(messages, pageId, "Editor layer state must reference one runtime page layer.");
            }
        }

        std::unordered_set<std::string> blinkTypeNames;
        for (const mfd::PageBlinkDefinition& blinkType : page.blinkTypes)
        {
            const std::string normalizedBlinkName =
                blinkType.normalizedName.empty() ? NormalizeEditorIdentifier(blinkType.name) : blinkType.normalizedName;
            if (normalizedBlinkName.empty())
            {
                PushProblem(messages, pageId, "Blink type names cannot be empty.");
                continue;
            }

            if (!blinkTypeNames.insert(normalizedBlinkName).second)
            {
                PushProblem(messages, pageId, "Blink type names must stay unique inside one page.");
            }
        }

        if (!page.defaultBlinkTypeName.empty() &&
            mfd::FindPageBlinkDefinition(page, page.defaultBlinkTypeName) == nullptr)
        {
            PushProblem(messages, pageId, "The page default blink type must resolve inside the page blink catalog.");
        }

        std::unordered_set<std::string> dynamicTemplateIds;
        std::unordered_map<std::string, std::unordered_set<int>> dynamicBindingOrdersByLayer;
        for (const mfd::DynamicReticleLayerBinding& binding : page.dynamicReticleBindings)
        {
            if (binding.templateId.empty())
            {
                PushProblem(messages, pageId, "Page dynamic reticle bindings must define a template id.");
                continue;
            }

            const std::string normalizedTemplateId = NormalizeEditorIdentifier(binding.templateId);
            if (!dynamicTemplateIds.insert(normalizedTemplateId).second)
            {
                PushProblem(messages, pageId, "Page dynamic reticle binding template ids must stay unique inside one page.");
            }

            if (loaded_.document.reticleLibrary.find(binding.templateId) == loaded_.document.reticleLibrary.end())
            {
                PushProblem(messages, pageId, "Page dynamic reticle binding template ids must resolve inside the loaded reticle library.");
            }

            const std::string normalizedLayerId = NormalizeEditorIdentifier(binding.layerId);
            if (normalizedLayerId.empty())
            {
                PushProblem(messages, pageId, "Page dynamic reticle bindings must define a layer id.");
            }
            else if (runtimeLayerIds.find(normalizedLayerId) == runtimeLayerIds.end())
            {
                PushProblem(messages, pageId, "Page dynamic reticle bindings must reference one runtime page layer.");
            }

            if (!dynamicBindingOrdersByLayer[normalizedLayerId].insert(binding.orderInLayer).second)
            {
                PushProblem(messages, pageId, "Page dynamic reticle binding orderInLayer values must stay unique inside one layer.");
            }
        }

        std::unordered_set<std::string> reticleIds;
        for (const mfd::ReticleGroup& reticle : page.staticReticles)
        {
            const std::string reticleId = PageReticleProblemId(page, reticle);
            if (reticle.id.empty())
            {
                PushProblem(messages, pageId, "Page reticle ids cannot be empty.");
            }

            if (!reticleIds.insert(NormalizeEditorIdentifier(reticle.id)).second)
            {
                PushProblem(messages, reticleId, "Page reticle ids must stay unique inside one page.");
            }

            if (!reticle.sourceTemplateId.empty() &&
                loaded_.document.reticleLibrary.find(reticle.sourceTemplateId) == loaded_.document.reticleLibrary.end())
            {
                PushProblem(messages, reticleId, "Page reticle source template must resolve inside the loaded reticle library.");
            }

            const std::string normalizedLayerId = NormalizeEditorIdentifier(reticle.layerId);
            if (normalizedLayerId.empty())
            {
                PushProblem(messages, reticleId, "Page reticles must define a runtime layer id.");
            }
            else if (runtimeLayerIds.find(normalizedLayerId) == runtimeLayerIds.end())
            {
                PushProblem(messages, reticleId, "Page reticles must reference an existing runtime page layer.");
            }

            AppendPrimitiveProblems(messages, reticle.primitives, reticleId);
            if (reticle.clipping.mode != mfd::ReticleClipMode::None && mfd::ResolveClipPrimitive(reticle) == nullptr)
            {
                PushProblem(messages, reticleId, "Reticle clipping must reference an existing supported primitive.");
            }

            if (reticle.blink.enabled &&
                !reticle.blink.typeName.empty() &&
                mfd::FindPageBlinkDefinition(page, reticle.blink.typeName) == nullptr)
            {
                PushProblem(messages, reticleId, "Page reticle blink bindings must reference one page-local blink type.");
            }
        }

        if (page.strobe.has_value())
        {
            if (page.strobe->reticle.blink.enabled &&
                !page.strobe->reticle.blink.typeName.empty() &&
                mfd::FindPageBlinkDefinition(page, page.strobe->reticle.blink.typeName) == nullptr)
            {
                PushProblem(messages, pageId, "Page strobe blink bindings must reference one page-local blink type.");
            }

            if (page.strobe->reticle.clipping.mode != mfd::ReticleClipMode::None &&
                mfd::ResolveClipPrimitive(page.strobe->reticle) == nullptr)
            {
                PushProblem(messages, pageId, "Page strobe clipping must reference an existing supported primitive.");
            }
        }
    }

    for (const auto& [templateId, reticle] : loaded_.document.reticleLibrary)
    {
        const std::string reticleId = ReticleAssetProblemId(templateId);
        if (NormalizeEditorIdentifier(templateId) != NormalizeEditorIdentifier(reticle.id))
        {
            PushProblem(messages, reticleId, "Reticle-library map key and reticle id must stay aligned.");
        }

        if (files_.templateFiles.find(templateId) == files_.templateFiles.end())
        {
            PushProblem(messages, reticleId, "Missing template file path for reticle asset.");
        }

        AppendPrimitiveProblems(messages, reticle.primitives, reticleId);
        if (reticle.clipping.mode != mfd::ReticleClipMode::None && mfd::ResolveClipPrimitive(reticle) == nullptr)
        {
            PushProblem(messages, reticleId, "Reticle clipping must reference an existing supported primitive.");
        }
    }

    return messages;
}

void EditorApplication::DrawPagePreview(const ViewportState& viewport)
{
    using editor::tutorial::TutorialStepId;

    const mfd::PageDefinition* page = ActivePage();
    if (page == nullptr)
    {
        return;
    }

    EnsurePreviewTexture(static_cast<int>(viewport.size.x), static_cast<int>(viewport.size.y));
    if (!previewTextureReady_)
    {
        return;
    }

    BeginTextureMode(previewTexture_);
    ClearBackground(ToRayColor(page->backgroundColor));
    {
        EnsurePreviewFont();
        ApplyBilinearFilterToFont(PreviewTextFont() == nullptr ? GetFontDefault() : *PreviewTextFont());
        mfd::Canvas2D canvas(
            previewTexture_.texture.width,
            previewTexture_.texture.height,
            viewport.view,
            PreviewTextFont(),
            ToRayColor(page->backgroundColor),
            previewTextureStencilReady_,
            &previewBezierCache_,
            &previewImageCache_,
            &previewTextLayoutCache_);
        const auto drawReticlePass = [&](const bool drawOnTop)
        {
            for (const auto& reticle : page->staticReticles)
            {
                if (!IsReticleVisibleInEditor(*page, reticle) || reticle.drawOnTop != drawOnTop)
                {
                    continue;
                }

                if (ShouldDimPageReticleInCurrentFocus(*page, reticle))
                {
                    canvas.DrawReticle(MakeDimmedReticlePreviewCopy(reticle, 0.30f));
                }
                else
                {
                    canvas.DrawReticle(reticle);
                }
            }
        };
        drawReticlePass(false);
        drawReticlePass(true);

        if (page->strobe.has_value())
        {
            if (ShouldDimPageReticleInCurrentFocus(*page, page->strobe->reticle))
            {
                canvas.DrawReticle(MakeDimmedReticlePreviewCopy(page->strobe->reticle, 0.30f));
            }
            else
            {
                canvas.DrawReticle(page->strobe->reticle);
            }
        }
    }
    EndTextureMode();

    ImGui::Image(
        (ImTextureID)(uintptr_t)previewTexture_.texture.id,
        viewport.size,
        ImVec2(0.0f, 1.0f),
        ImVec2(1.0f, 0.0f));

    ImGui::SetCursorScreenPos(viewport.origin);
    ImGui::SetNextItemAllowOverlap();
    ImGui::InvisibleButton("PagePreviewInput", viewport.size);

    const ImVec2 mouse = ImGui::GetMousePos();
    const ImVec2 viewportMax(viewport.origin.x + viewport.size.x, viewport.origin.y + viewport.size.y);
    std::optional<mfd::Vec2> mouseLogical;
    if (IsPointInsideRect(mouse, viewport.origin, viewportMax))
    {
        mouseLogical = viewport.ToLogical(mouse);
    }

    tutorial_->DrawHalo(
        "page_preview_clip_source",
        "Right-click the circle reticle",
        "Open the clipping context menu on the tutorial mask so you can keep only the inside region.");

        if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MFD_LIBRARY_RETICLE"))
        {
            const char* templateId = static_cast<const char*>(payload->Data);
            if (tutorial_->ShouldUseHighlightedAddToPageButton())
            {
                RebuildStatus("Tutorial: use the highlighted Add to active page button for this step.", true);
            }
            else
            {
                CreatePageReticleInstanceFromTemplate(templateId, viewport.ToLogical(ImGui::GetMousePos()));
            }
        }
        ImGui::EndDragDropTarget();
    }

    DrawViewportToolbar(
        viewport.origin,
        mfd::SanitizeZoom(pagePreviewView_.zoom),
        mouseLogical,
        "?##PagePreviewHelp",
        kPagePreviewHelpPopupId,
        false);
}

void EditorApplication::DrawLibraryPreview(const ViewportState& viewport)
{
    const mfd::ReticleGroup* reticle = SelectedLibraryReticle();
    if (reticle == nullptr)
    {
        return;
    }

    EnsurePreviewTexture(static_cast<int>(viewport.size.x), static_cast<int>(viewport.size.y));
    if (!previewTextureReady_)
    {
        return;
    }

    BeginTextureMode(previewTexture_);
    ClearBackground(Color {10, 18, 24, 255});
    {
        EnsurePreviewFont();
        ApplyBilinearFilterToFont(PreviewTextFont() == nullptr ? GetFontDefault() : *PreviewTextFont());
        mfd::Canvas2D canvas(
            previewTexture_.texture.width,
            previewTexture_.texture.height,
            viewport.view,
            PreviewTextFont(),
            Color {10, 18, 24, 255},
            previewTextureStencilReady_,
            &previewBezierCache_,
            &previewImageCache_,
            &previewTextLayoutCache_);
        canvas.DrawReticle(*reticle);
    }
    EndTextureMode();

    ImGui::Image(
        (ImTextureID)(uintptr_t)previewTexture_.texture.id,
        viewport.size,
        ImVec2(0.0f, 1.0f),
        ImVec2(1.0f, 0.0f));

    ImGui::SetCursorScreenPos(viewport.origin);
    ImGui::SetNextItemAllowOverlap();
    ImGui::InvisibleButton("LibraryPreviewInput", viewport.size);

    const ImVec2 mouse = ImGui::GetMousePos();
    const ImVec2 viewportMax(viewport.origin.x + viewport.size.x, viewport.origin.y + viewport.size.y);
    std::optional<mfd::Vec2> mouseLogical;
    if (IsPointInsideRect(mouse, viewport.origin, viewportMax))
    {
        mouseLogical = viewport.ToLogical(mouse);
    }

    DrawViewportToolbar(
        viewport.origin,
        mfd::SanitizeZoom(libraryPreviewView_.zoom),
        mouseLogical,
        "?##LibraryPreviewHelp",
        kLibraryPreviewHelpPopupId,
        true);
}

void EditorApplication::DrawPagePreviewHeaderControls(const char* buttonId,
                                                      const bool showProblemsIndicator,
                                                      const bool allowFullscreenToggle)
{
    using editor::tutorial::TutorialStepId;

    const ImGuiStyle& style = ImGui::GetStyle();
    const std::string label =
        showProblemsIndicator && !pagePreviewViewOptions_.showProblemsPanel ? "View !" : "View";
    const std::string buttonLabel = label + (buttonId == nullptr ? "##PagePreviewViewMenu" : buttonId);
    const float buttonWidth = ImGui::CalcTextSize(label.c_str()).x + style.FramePadding.x * 2.0f;
    const float fullscreenButtonWidth =
        allowFullscreenToggle ? ImGui::CalcTextSize("[]").x + style.FramePadding.x * 2.0f : 0.0f;
    const float controlsWidth =
        buttonWidth + (allowFullscreenToggle ? style.ItemSpacing.x + fullscreenButtonWidth : 0.0f);

    ImGui::SameLine();
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x - controlsWidth));
    if (ImGui::Button(buttonLabel.c_str()))
    {
        if (tutorial_->MatchesTarget("page_preview_view_menu"))
        {
            tutorial_->AdvancePhase();
        }
        ImGui::OpenPopup(kPagePreviewDisplayPopupId);
    }
    ShowItemTooltip("Toggle page-preview overlays and editor-only helper panels.");
    tutorial_->DrawHalo(
        "page_preview_view_menu",
        "Click View",
        "Open the page-preview helper menu so the coach can walk through the editor-only overlays one by one.");

    if (allowFullscreenToggle)
    {
        ImGui::SameLine();
        if (ImGui::Button("[]##PagePreviewFullscreenToggle"))
        {
            ToggleFullscreenPagePreview();
            if (tutorial_->MatchesTarget("page_preview_fullscreen"))
            {
                if (tutorial_->IsStepPhase(static_cast<int>(TutorialStepId::ToggleFullscreenPreview), 0))
                {
                    tutorial_->AdvancePhase();
                }
                else
                {
                    tutorial_->CompleteStep();
                }
            }
        }
        ShowItemTooltip(fullscreenPreviewController_.IsActive() ? "Exit fullscreen preview" : "Fullscreen page preview");
        tutorial_->DrawHalo(
            "page_preview_fullscreen",
            "Toggle fullscreen preview",
            tutorial_->IsStepPhase(static_cast<int>(TutorialStepId::ToggleFullscreenPreview), 0)
                ? "Enter fullscreen preview to focus on the page canvas."
                : "Leave fullscreen preview to restore the normal editor layout.");
    }

    if (ImGui::BeginPopup(kPagePreviewDisplayPopupId))
    {
        const bool layerInspectorChanged = ImGui::Checkbox("Layer Inspector", &pagePreviewViewOptions_.showLayerInspector);
        if (layerInspectorChanged && !pagePreviewViewOptions_.showLayerInspector)
        {
            ClearLayerFocus(false);
            ReleaseLayerPreviewTextures();
        }
        if (layerInspectorChanged && pagePreviewViewOptions_.showLayerInspector &&
            tutorial_->MatchesTarget("page_preview_view_layer_inspector"))
        {
            tutorial_->CompleteStep();
        }
        tutorial_->DrawHalo(
            "page_preview_view_layer_inspector",
            "Enable Layer Inspector",
            "Turn on the layer strip to inspect editor-only layers and their thumbnails.");

        const bool minimapChanged = ImGui::Checkbox("Minimap", &pagePreviewViewOptions_.showMinimap);
        if (minimapChanged && pagePreviewViewOptions_.showMinimap &&
            tutorial_->MatchesTarget("page_preview_view_minimap"))
        {
            tutorial_->CompleteStep();
        }
        tutorial_->DrawHalo(
            "page_preview_view_minimap",
            "Enable Minimap",
            "Turn on the minimap so page navigation stays readable while zooming and panning.");

        const bool problemsChanged = ImGui::Checkbox("Problems", &pagePreviewViewOptions_.showProblemsPanel);
        if (problemsChanged && pagePreviewViewOptions_.showProblemsPanel &&
            tutorial_->MatchesTarget("page_preview_view_problems"))
        {
            tutorial_->CompleteStep();
        }
        tutorial_->DrawHalo(
            "page_preview_view_problems",
            "Enable Problems",
            "Dock the validation panel under the preview so diagnostics stay visible while editing.");

        const bool highlightChanged = ImGui::Checkbox("Highlight reticle usages", &pagePreviewViewOptions_.highlightReticleUsages);
        if (highlightChanged && pagePreviewViewOptions_.highlightReticleUsages &&
            tutorial_->MatchesTarget("page_preview_view_highlight_usages"))
        {
            tutorial_->CompleteStep();
        }
        tutorial_->DrawHalo(
            "page_preview_view_highlight_usages",
            "Enable Highlight reticle usages",
            "Turn on template usage highlighting for the currently selected shared reticle.");
        ImGui::Separator();
        ImGui::Checkbox("Reticle names", &pagePreviewViewOptions_.showReticleNames);
        ImGui::Checkbox("Gizmos", &pagePreviewViewOptions_.showGizmos);
        const bool pageContextChanged = ImGui::Checkbox("Page context", &pagePreviewViewOptions_.showPageContext);
        if (pageContextChanged && pagePreviewViewOptions_.showPageContext &&
            tutorial_->MatchesTarget("page_preview_view_page_context"))
        {
            tutorial_->CompleteStep();
        }
        tutorial_->DrawHalo(
            "page_preview_view_page_context",
            "Enable Page context",
            "Turn on the page-context split so the active page stays visible while you inspect the rest of the workspace.");
        if (showProblemsIndicator && !pagePreviewViewOptions_.showProblemsPanel)
        {
            ImGui::Separator();
            ImGui::TextDisabled("Validation issues are available. Enable Problems to inspect them below the preview.");
        }
        ImGui::EndPopup();
    }
    else if (tutorial_->ShouldResetPagePreviewViewPhaseOnClose())
    {
        tutorial_->ResetPhase();
    }
}

void EditorApplication::DrawLibraryPreviewOverlays(const ViewportState& viewport)
{
    const mfd::ReticleGroup* reticle = SelectedLibraryReticle();
    if (reticle == nullptr)
    {
        return;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const bool hasSelectedPrimitive = selection_.kind == SelectionKind::LibraryPrimitive &&
                                      selection_.libraryReticleId == reticle->id &&
                                      selection_.primitiveIndex >= 0 &&
                                      selection_.primitiveIndex < static_cast<int>(reticle->primitives.size());

    auto toScreenPoint = [&](const mfd::Primitive& primitive, const mfd::Vec2 localPoint)
    {
        return viewport.ToScreen(mfd::ApplyTransform(mfd::ApplyTransform(localPoint, primitive.transform), reticle->transform));
    };

    auto drawHandle = [&](const ImVec2 point, const ImU32 color, const float radius = 6.0f)
    {
        drawList->AddCircleFilled(point, radius, color, 16);
        drawList->AddCircle(point, radius, IM_COL32(10, 18, 24, 255), 16, 1.5f);
    };

    for (int primitiveIndex = 0; primitiveIndex < static_cast<int>(reticle->primitives.size()); ++primitiveIndex)
    {
        const mfd::Primitive& primitive = reticle->primitives[static_cast<std::size_t>(primitiveIndex)];
        const ReticleScreenBounds bounds = ComputePrimitiveScreenBounds(*reticle, primitive, viewport);
        if (!bounds.valid)
        {
            continue;
        }

        const bool selected = hasSelectedPrimitive && selection_.primitiveIndex == primitiveIndex;
        if (libraryStudioShowGizmos_)
        {
            const ImU32 borderColor = selected ? IM_COL32(255, 212, 110, 255) : IM_COL32(104, 185, 205, 160);
            const ImU32 fillColor = selected ? IM_COL32(255, 212, 110, 32) : IM_COL32(104, 185, 205, 18);
            drawList->AddRectFilled(bounds.min, bounds.max, fillColor, 6.0f);
            drawList->AddRect(bounds.min, bounds.max, borderColor, 6.0f, 0, selected ? 2.2f : 1.3f);
        }

        if (libraryStudioShowPrimitiveLabels_)
        {
            const std::string label =
                std::to_string(primitiveIndex + 1) + ". " +
                (primitive.id.empty() ? PrimitiveTypeLabel(primitive.type) : primitive.id);
            const ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
            const ImVec2 tagMin(bounds.min.x + 6.0f, bounds.min.y + 6.0f);
            const ImVec2 tagMax(tagMin.x + textSize.x + 12.0f, tagMin.y + textSize.y + 6.0f);
            drawList->AddRectFilled(tagMin, tagMax, selected ? IM_COL32(255, 212, 110, 220) : IM_COL32(33, 49, 59, 210), 4.0f);
            drawList->AddText(ImVec2(tagMin.x + 6.0f, tagMin.y + 3.0f),
                              selected ? IM_COL32(12, 20, 26, 255) : IM_COL32(220, 235, 240, 255),
                              label.c_str());
        }

        if (!selected || !libraryStudioShowGizmos_)
        {
            continue;
        }

        const ImVec2 primitiveCenter = toScreenPoint(primitive, {});
        drawHandle(primitiveCenter, IM_COL32(94, 224, 174, 255), 7.0f);

        if (const auto* line = std::get_if<mfd::LineGeometry>(&primitive.geometry))
        {
            drawHandle(toScreenPoint(primitive, line->start), IM_COL32(255, 140, 92, 255));
            drawHandle(toScreenPoint(primitive, line->end), IM_COL32(255, 140, 92, 255));
            continue;
        }

        if (const auto* circle = std::get_if<mfd::CircleGeometry>(&primitive.geometry))
        {
            drawList->AddCircle(primitiveCenter,
                                std::max(8.0f, Distance(primitiveCenter, toScreenPoint(primitive, {circle->radius, 0.0f}))),
                                IM_COL32(255, 212, 110, 140),
                                48,
                                1.2f);
            drawHandle(toScreenPoint(primitive, {circle->radius, 0.0f}), IM_COL32(110, 180, 250, 255), 7.0f);
            continue;
        }

        if (const auto* ring = std::get_if<mfd::RingGeometry>(&primitive.geometry))
        {
            drawList->AddCircle(primitiveCenter,
                                std::max(8.0f, Distance(primitiveCenter, toScreenPoint(primitive, {ring->outerRadius, 0.0f}))),
                                IM_COL32(255, 212, 110, 140),
                                48,
                                1.2f);
            drawList->AddCircle(primitiveCenter,
                                std::max(4.0f, Distance(primitiveCenter, toScreenPoint(primitive, {ring->innerRadius, 0.0f}))),
                                IM_COL32(255, 212, 110, 90),
                                48,
                                1.0f);
            drawHandle(toScreenPoint(primitive, {ring->innerRadius, 0.0f}), IM_COL32(110, 180, 250, 255), 7.0f);
            drawHandle(toScreenPoint(primitive, {ring->outerRadius, 0.0f}), IM_COL32(110, 180, 250, 255), 7.0f);
            continue;
        }

        if (const auto* rectangle = std::get_if<mfd::RectangleGeometry>(&primitive.geometry))
        {
            drawHandle(toScreenPoint(primitive, {-rectangle->width * 0.5f, -rectangle->height * 0.5f}), IM_COL32(255, 140, 92, 255));
            drawHandle(toScreenPoint(primitive, {rectangle->width * 0.5f, -rectangle->height * 0.5f}), IM_COL32(255, 140, 92, 255));
            drawHandle(toScreenPoint(primitive, {rectangle->width * 0.5f, rectangle->height * 0.5f}), IM_COL32(255, 140, 92, 255));
            drawHandle(toScreenPoint(primitive, {-rectangle->width * 0.5f, rectangle->height * 0.5f}), IM_COL32(255, 140, 92, 255));
            continue;
        }

        if (const auto* ellipse = std::get_if<mfd::EllipseGeometry>(&primitive.geometry))
        {
            drawList->AddCircle(primitiveCenter,
                                std::max(8.0f, Distance(primitiveCenter, toScreenPoint(primitive, {ellipse->width * 0.5f, 0.0f}))),
                                IM_COL32(255, 212, 110, 90),
                                48,
                                1.0f);
            drawHandle(toScreenPoint(primitive, {ellipse->width * 0.5f, 0.0f}), IM_COL32(110, 180, 250, 255), 7.0f);
            drawHandle(toScreenPoint(primitive, {0.0f, ellipse->height * 0.5f}), IM_COL32(110, 180, 250, 255), 7.0f);
            continue;
        }

        if (const auto* square = std::get_if<mfd::SquareGeometry>(&primitive.geometry))
        {
            drawHandle(toScreenPoint(primitive, {-square->width * 0.5f, -square->height * 0.5f}), IM_COL32(255, 140, 92, 255));
            drawHandle(toScreenPoint(primitive, {square->width * 0.5f, -square->height * 0.5f}), IM_COL32(255, 140, 92, 255));
            drawHandle(toScreenPoint(primitive, {square->width * 0.5f, square->height * 0.5f}), IM_COL32(255, 140, 92, 255));
            drawHandle(toScreenPoint(primitive, {-square->width * 0.5f, square->height * 0.5f}), IM_COL32(255, 140, 92, 255));
            continue;
        }

        if (const auto* diamond = std::get_if<mfd::DiamondGeometry>(&primitive.geometry))
        {
            drawHandle(toScreenPoint(primitive, {0.0f, diamond->height * 0.5f}), IM_COL32(255, 140, 92, 255));
            drawHandle(toScreenPoint(primitive, {diamond->width * 0.5f, 0.0f}), IM_COL32(255, 140, 92, 255));
            drawHandle(toScreenPoint(primitive, {0.0f, -diamond->height * 0.5f}), IM_COL32(255, 140, 92, 255));
            drawHandle(toScreenPoint(primitive, {-diamond->width * 0.5f, 0.0f}), IM_COL32(255, 140, 92, 255));
            continue;
        }

        if (const auto* triangle = std::get_if<mfd::TriangleGeometry>(&primitive.geometry))
        {
            drawHandle(toScreenPoint(primitive, triangle->points[0]), IM_COL32(255, 140, 92, 255));
            drawHandle(toScreenPoint(primitive, triangle->points[1]), IM_COL32(255, 140, 92, 255));
            drawHandle(toScreenPoint(primitive, triangle->points[2]), IM_COL32(255, 140, 92, 255));
            continue;
        }

        if (const auto* polyline = std::get_if<mfd::PolylineGeometry>(&primitive.geometry))
        {
            for (const auto& point : polyline->points)
            {
                drawHandle(toScreenPoint(primitive, point), IM_COL32(255, 140, 92, 255));
            }
            continue;
        }

        if (const auto* bezier = std::get_if<mfd::BezierGeometry>(&primitive.geometry))
        {
            for (const auto& point : bezier->controlPoints)
            {
                drawHandle(toScreenPoint(primitive, point), IM_COL32(255, 140, 92, 255));
            }
            continue;
        }

        if (const auto* arc = std::get_if<mfd::ArcGeometry>(&primitive.geometry))
        {
            const std::vector<mfd::Vec2> arcPoints =
                ApproximateArcPoints(arc->radius, arc->startAngleDegrees, arc->endAngleDegrees, arc->segments);
            for (std::size_t index = 0; index + 1U < arcPoints.size(); ++index)
            {
                drawList->AddLine(toScreenPoint(primitive, arcPoints[index]),
                                  toScreenPoint(primitive, arcPoints[index + 1U]),
                                  IM_COL32(255, 212, 110, 110),
                                  1.5f);
            }

            const float middleAngleDegrees = (arc->startAngleDegrees + arc->endAngleDegrees) * 0.5f;
            const float middleAngleRadians = middleAngleDegrees * PI / 180.0f;
            const mfd::Vec2 middlePoint {
                std::cos(middleAngleRadians) * std::abs(arc->radius),
                std::sin(middleAngleRadians) * std::abs(arc->radius)};

            if (!arcPoints.empty())
            {
                drawHandle(toScreenPoint(primitive, arcPoints.front()), IM_COL32(255, 140, 92, 255));
                drawHandle(toScreenPoint(primitive, arcPoints.back()), IM_COL32(255, 140, 92, 255));
            }
            drawHandle(toScreenPoint(primitive, middlePoint), IM_COL32(110, 180, 250, 255), 7.0f);
        }
    }
}

void EditorApplication::DrawPreviewOverlays(const ViewportState& viewport)
{
    const mfd::PageDefinition* page = ActivePage();
    if (page == nullptr)
    {
        return;
    }

    if (pagePreviewViewOptions_.showMinimap)
    {
        DrawPagePreviewMinimap(viewport, *page);
    }

    if (pagePreviewViewOptions_.showReticleNames)
    {
        DrawPagePreviewReticleNames(viewport, *page);
    }

    if (pagePreviewViewOptions_.highlightReticleUsages)
    {
        DrawReticleUsageHighlightPlaceholder(viewport);
    }

    if (pagePreviewViewOptions_.showGizmos)
    {
        DrawPagePreviewGizmos(viewport, *page);
    }
}

void EditorApplication::DrawPagePreviewMinimap(const ViewportState& viewport, const mfd::PageDefinition& page)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const PageMinimapState minimap = ComputePageMinimapState(page, viewport);
    if (!minimap.valid)
    {
        return;
    }

    const ImVec2 mouse = ImGui::GetMousePos();
    const bool mouseInsideMinimap = IsPointInsideRect(mouse, minimap.contentMin, minimap.contentMax);

    drawList->AddRectFilled(minimap.frameMin, minimap.frameMax, IM_COL32(7, 15, 23, 224), 8.0f);
    drawList->AddRect(minimap.frameMin, minimap.frameMax, IM_COL32(68, 118, 152, 255), 8.0f, 0, 1.5f);
    drawList->AddRectFilled(minimap.contentMin, minimap.contentMax, IM_COL32(12, 24, 34, 220), 6.0f);

    if (minimap.logicalMin.x <= 0.0f && minimap.logicalMax.x >= 0.0f)
    {
        const ImVec2 axisBottom = ToMinimapScreen(minimap, mfd::Vec2 {0.0f, minimap.logicalMin.y});
        const ImVec2 axisTop = ToMinimapScreen(minimap, mfd::Vec2 {0.0f, minimap.logicalMax.y});
        drawList->AddLine(axisBottom, axisTop, IM_COL32(52, 79, 96, 255), 1.0f);
    }

    if (minimap.logicalMin.y <= 0.0f && minimap.logicalMax.y >= 0.0f)
    {
        const ImVec2 axisLeft = ToMinimapScreen(minimap, mfd::Vec2 {minimap.logicalMin.x, 0.0f});
        const ImVec2 axisRight = ToMinimapScreen(minimap, mfd::Vec2 {minimap.logicalMax.x, 0.0f});
        drawList->AddLine(axisLeft, axisRight, IM_COL32(52, 79, 96, 255), 1.0f);
    }

    const std::vector<int> selectedIndices = SelectedPageReticleIndices();
    for (int reticleIndex = 0; reticleIndex < static_cast<int>(page.staticReticles.size()); ++reticleIndex)
    {
        const mfd::ReticleGroup& reticle = page.staticReticles[static_cast<std::size_t>(reticleIndex)];
        if (!IsReticleVisibleInEditor(page, reticle))
        {
            continue;
        }

        const LogicalBounds worldBounds = ComputeReticleWorldBounds(reticle);
        if (!worldBounds.valid)
        {
            continue;
        }

        const ImVec2 rectPointA = ToMinimapScreen(minimap, worldBounds.min);
        const ImVec2 rectPointB = ToMinimapScreen(minimap, worldBounds.max);
        const ImVec2 rectMin(std::min(rectPointA.x, rectPointB.x), std::min(rectPointA.y, rectPointB.y));
        const ImVec2 rectMax(std::max(rectPointA.x, rectPointB.x), std::max(rectPointA.y, rectPointB.y));
        const bool selected = std::find(selectedIndices.begin(), selectedIndices.end(), reticleIndex) != selectedIndices.end();

        if (rectMax.x - rectMin.x < 4.0f || rectMax.y - rectMin.y < 4.0f)
        {
            const ImVec2 center = ToMinimapScreen(minimap, worldBounds.center);
            drawList->AddCircleFilled(center,
                                      selected ? 3.5f : 2.5f,
                                      selected ? IM_COL32(84, 219, 201, 255) : IM_COL32(174, 200, 214, 230),
                                      12);
            continue;
        }

        drawList->AddRectFilled(rectMin,
                                rectMax,
                                selected ? IM_COL32(84, 219, 201, 70) : IM_COL32(174, 200, 214, 34),
                                2.0f);
        drawList->AddRect(rectMin,
                          rectMax,
                          selected ? IM_COL32(84, 219, 201, 255) : IM_COL32(174, 200, 214, 190),
                          2.0f,
                          0,
                          selected ? 1.8f : 1.0f);
    }

    const LogicalBounds viewBounds = ComputeViewportLogicalBounds(viewport);
    if (viewBounds.valid)
    {
        const ImVec2 viewA = ToMinimapScreen(minimap, viewBounds.min);
        const ImVec2 viewB = ToMinimapScreen(minimap, viewBounds.max);
        const ImVec2 viewMin(std::min(viewA.x, viewB.x), std::min(viewA.y, viewB.y));
        const ImVec2 viewMax(std::max(viewA.x, viewB.x), std::max(viewA.y, viewB.y));
        drawList->AddRectFilled(viewMin, viewMax, IM_COL32(110, 180, 250, 38), 4.0f);
        drawList->AddRect(viewMin, viewMax, IM_COL32(110, 180, 250, 255), 4.0f, 0, 1.8f);
    }

    const char* minimapLabel = "Minimap";
    const ImVec2 textSize = ImGui::CalcTextSize(minimapLabel);
    drawList->AddText(ImVec2(minimap.frameMin.x + 10.0f, minimap.frameMin.y + 8.0f),
                      IM_COL32(216, 233, 246, 255),
                      minimapLabel);
    drawList->AddLine(ImVec2(minimap.frameMin.x + 10.0f, minimap.frameMin.y + textSize.y + 12.0f),
                      ImVec2(minimap.frameMax.x - 10.0f, minimap.frameMin.y + textSize.y + 12.0f),
                      IM_COL32(36, 63, 78, 255),
                      1.0f);

    ShowHoveredRegionTooltip(
        mouseInsideMinimap,
        "Minimap navigation for the editor camera.\n"
        "Drag the blue viewport rectangle to pan smoothly.\n"
        "Click elsewhere in the minimap to recenter the editor view.");
}

void EditorApplication::DrawPagePreviewReticleNames(const ViewportState& viewport, const mfd::PageDefinition& page)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    for (int reticleIndex = 0; reticleIndex < static_cast<int>(page.staticReticles.size()); ++reticleIndex)
    {
        const mfd::ReticleGroup& reticle = page.staticReticles[static_cast<std::size_t>(reticleIndex)];
        if (!IsReticleVisibleInEditor(page, reticle))
        {
            continue;
        }

        const ReticleScreenBounds bounds = ComputeReticleScreenBounds(reticle, viewport);
        if (!bounds.valid)
        {
            continue;
        }

        const std::string label = reticle.id.empty() ? std::string {"reticle"} : reticle.id;
        const ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
        const ImVec2 tagMin(bounds.min.x + 6.0f, bounds.min.y + 6.0f);
        const ImVec2 tagMax(tagMin.x + textSize.x + 12.0f, tagMin.y + textSize.y + 6.0f);
        const bool selected = HasSelectedPageReticle(selection_.pageIndex, reticleIndex);
        const bool dimmed = ShouldDimPageReticleInCurrentFocus(page, reticle);
        drawList->AddRectFilled(tagMin,
                                tagMax,
                                selected ? IM_COL32(84, 219, 201, 220)
                                         : (dimmed ? IM_COL32(24, 30, 36, 188) : IM_COL32(33, 49, 59, 210)),
                                4.0f);
        drawList->AddText(ImVec2(tagMin.x + 6.0f, tagMin.y + 3.0f),
                          selected ? IM_COL32(12, 20, 26, 255)
                                   : (dimmed ? IM_COL32(146, 160, 170, 220) : IM_COL32(220, 235, 240, 255)),
                          label.c_str());
    }
}

void EditorApplication::DrawLayerInspectorPanel(const mfd::PageDefinition& page)
{
    const editor::LayerFocusStripModel model = layerFocusController_.BuildStripModel(page, layerFocusState_);
    ImGui::TextColored(ImVec4(0.85f, 0.91f, 0.96f, 1.0f), "Layer Inspector");
    ImGui::TextDisabled("Focus one layer without changing JSON.");
    ImGui::Separator();

    const float previewWidth = std::max(72.0f, ImGui::GetContentRegionAvail().x);
    const int previewWidthPixels = std::max(72, static_cast<int>(std::lround(previewWidth)));
    const int previewHeightPixels = static_cast<int>(kLayerInspectorPreviewHeight);

    for (std::size_t index = 0; index < model.entries.size(); ++index)
    {
        const editor::LayerFocusStripEntry& entry = model.entries[index];
        const bool pressed = ImGui::Selectable(entry.label.c_str(), entry.selected);
        if (pressed)
        {
            if (entry.fullView)
            {
                ClearLayerFocus(true);
            }
            else
            {
                layerFocusState_ = layerFocusController_.MakeFocusedState(page, entry.layerId);
                SanitizePageReticleSelectionForCurrentFocus();
                RebuildStatus("Layer focus set to '" + entry.layerId + "' on page '" + page.name + "'.", false);
            }
        }

        const std::string metaLabel =
            entry.fullView ? std::to_string(entry.reticleCount) + " page reticle(s)"
                           : std::to_string(entry.reticleCount) + " reticle(s)" +
                                 (entry.visible ? "" : "  hidden in preview");
        ImGui::TextDisabled("%s", metaLabel.c_str());

        if (const RenderTexture2D* previewTexture =
                RenderLayerPreviewThumbnail(index, page, entry, previewWidthPixels, previewHeightPixels);
            previewTexture != nullptr)
        {
            ImGui::Image((ImTextureID)(uintptr_t)previewTexture->texture.id,
                         ImVec2(previewWidth, kLayerInspectorPreviewHeight),
                         ImVec2(0.0f, 1.0f),
                         ImVec2(1.0f, 0.0f));
        }
        else
        {
            ImGui::Dummy(ImVec2(previewWidth, kLayerInspectorPreviewHeight));
        }

        if (entry.reticleCount == 0U)
        {
            ImGui::TextDisabled("No page reticles currently target this layer.");
        }

        if (!entry.fullView && !entry.visible)
        {
            ShowItemTooltip("This editor layer is currently hidden in the page preview.");
        }

        if (index + 1U < model.entries.size())
        {
            ImGui::Spacing();
            ImGui::Separator();
        }
    }

    if (layerPreviewTextures_.size() > model.entries.size())
    {
        for (std::size_t slotIndex = model.entries.size(); slotIndex < layerPreviewTextures_.size(); ++slotIndex)
        {
            if (layerPreviewTextures_[slotIndex].ready)
            {
                UnloadRenderTexture(layerPreviewTextures_[slotIndex].texture);
            }
        }
        layerPreviewTextures_.resize(model.entries.size());
    }
}

void EditorApplication::DrawProblemsPanel(const std::vector<std::string>& problemMessages)
{
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.67f, 1.0f), "Problems");
    if (!problemMessages.empty())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("(%zu)", problemMessages.size());
    }
    ImGui::TextDisabled("Validation diagnostics for the current editor state.");
    ImGui::Separator();

    if (ImGui::BeginChild("PagePreviewProblemsScrollRegion", ImVec2(0.0f, 0.0f), false))
    {
        if (problemMessages.empty())
        {
            ImGui::TextDisabled("No validation problems detected.");
        }
        else
        {
            for (std::size_t index = 0; index < problemMessages.size(); ++index)
            {
                ImGui::PushID(static_cast<int>(index));
                ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.44f, 1.0f), "%02zu.", index + 1U);
                ImGui::SameLine();
                const float wrapPos = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
                ImGui::PushTextWrapPos(wrapPos);
                ImGui::TextUnformatted(problemMessages[index].c_str());
                ImGui::PopTextWrapPos();
                if (index + 1U < problemMessages.size())
                {
                    ImGui::Spacing();
                }
                ImGui::PopID();
            }
        }
    }
    ImGui::EndChild();
}

void EditorApplication::DrawReticleUsageHighlightPlaceholder(const ViewportState& viewport)
{
    const editor::ReticleUsageHighlightResult* usageHighlight = ResolveReticleUsageHighlight();
    if (usageHighlight == nullptr)
    {
        return;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const mfd::PageDefinition* page = ActivePage();
    const editor::ReticleUsageHighlightPage* currentPageHighlight = nullptr;
    if (page != nullptr)
    {
        const auto iterator = std::find_if(usageHighlight->pages.begin(),
                                           usageHighlight->pages.end(),
                                           [this](const editor::ReticleUsageHighlightPage& usagePage)
                                           {
                                               return usagePage.currentPageIndex == selection_.pageIndex;
                                           });
        if (iterator != usageHighlight->pages.end())
        {
            currentPageHighlight = &(*iterator);
        }
    }

    if (page != nullptr && currentPageHighlight != nullptr)
    {
        for (const int reticleIndex : currentPageHighlight->matchingReticleIndices)
        {
            if (reticleIndex < 0 || reticleIndex >= static_cast<int>(page->staticReticles.size()))
            {
                continue;
            }

            const ReticleScreenBounds bounds =
                ComputeReticleScreenBounds(page->staticReticles[static_cast<std::size_t>(reticleIndex)], viewport);
            if (!bounds.valid)
            {
                continue;
            }

            drawList->AddRectFilled(bounds.min, bounds.max, IM_COL32(255, 210, 102, 24), 5.0f);
            drawList->AddRect(bounds.min, bounds.max, IM_COL32(255, 210, 102, 255), 5.0f, 0, 2.2f);
        }

        if (currentPageHighlight->matchingStrobe && page->strobe.has_value())
        {
            const ReticleScreenBounds strobeBounds = ComputeReticleScreenBounds(page->strobe->reticle, viewport);
            if (strobeBounds.valid)
            {
                drawList->AddRectFilled(strobeBounds.min, strobeBounds.max, IM_COL32(255, 210, 102, 18), 5.0f);
                drawList->AddRect(strobeBounds.min, strobeBounds.max, IM_COL32(255, 210, 102, 220), 5.0f, 0, 2.0f);
            }
        }
    }

    const bool noUsage = !usageHighlight->hasUsage;
    const std::string header = noUsage
                                   ? "No page currently uses '" + usageHighlight->templateId + "'."
                                   : std::to_string(usageHighlight->pages.size()) + " page" +
                                         (usageHighlight->pages.size() == 1U ? "" : "s") +
                                         " use '" + usageHighlight->templateId + "'.";
    const float lineHeight = ImGui::GetTextLineHeightWithSpacing();
    const std::size_t shownPages = std::min<std::size_t>(4U, usageHighlight->pages.size());
    const float panelWidth = 320.0f;
    const float panelHeight = 42.0f + lineHeight * static_cast<float>(shownPages) + (shownPages > 0U ? 6.0f : 0.0f);
    const ImVec2 panelMin(viewport.origin.x + 12.0f, viewport.origin.y + viewport.size.y - panelHeight - 12.0f);
    const ImVec2 panelMax(panelMin.x + panelWidth, panelMin.y + panelHeight);
    drawList->AddRectFilled(panelMin, panelMax, IM_COL32(33, 49, 59, 220), 6.0f);
    drawList->AddRect(panelMin, panelMax, IM_COL32(255, 210, 102, noUsage ? 160 : 220), 6.0f, 0, 1.5f);
    drawList->AddText(ImVec2(panelMin.x + 10.0f, panelMin.y + 8.0f),
                      noUsage ? IM_COL32(220, 235, 240, 255) : IM_COL32(255, 224, 176, 255),
                      header.c_str());

    float currentY = panelMin.y + 8.0f + lineHeight;
    for (std::size_t index = 0; index < shownPages; ++index)
    {
        const editor::ReticleUsageHighlightPage& usagePage = usageHighlight->pages[index];
        const std::string label = std::string("- ") + usagePage.pageName;
        drawList->AddText(ImVec2(panelMin.x + 12.0f, currentY), IM_COL32(220, 235, 240, 255), label.c_str());
        currentY += lineHeight;
    }

    if (usageHighlight->pages.size() > shownPages)
    {
        const std::string more = "+" + std::to_string(usageHighlight->pages.size() - shownPages) + " more";
        drawList->AddText(ImVec2(panelMin.x + 12.0f, currentY), IM_COL32(170, 186, 198, 255), more.c_str());
    }
}

const editor::ReticleUsageHighlightResult* EditorApplication::ResolveReticleUsageHighlight()
{
    const mfd::ReticleGroup* selectedReticle = SelectedLibraryReticle();
    if (!pagePreviewViewOptions_.highlightReticleUsages || selectedReticle == nullptr)
    {
        return nullptr;
    }

    std::filesystem::path templateFile = loaded_.window.reticleLibraryFolder;
    if (const auto iterator = files_.templateFiles.find(selection_.libraryReticleId); iterator != files_.templateFiles.end())
    {
        templateFile = iterator->second;
    }

    const std::filesystem::path assetsRoot = ResolveAssetRootForPath(templateFile);
    if (!reticleUsageHighlightCache_.dirty &&
        mfd::PageNamesEqual(reticleUsageHighlightCache_.templateId, selectedReticle->id) &&
        reticleUsageHighlightCache_.assetsRoot == assetsRoot)
    {
        return &reticleUsageHighlightCache_.result;
    }

    reticleUsageHighlightCache_.result = reticleUsageHighlightService_.BuildHighlight(
        loaded_,
        files_,
        editor::ReticleUsageHighlightRequest {
            selectedReticle->id,
            assetsRoot});
    reticleUsageHighlightCache_.templateId = selectedReticle->id;
    reticleUsageHighlightCache_.assetsRoot = assetsRoot;
    reticleUsageHighlightCache_.dirty = false;
    return &reticleUsageHighlightCache_.result;
}

void EditorApplication::InvalidateReticleUsageHighlightCache() noexcept
{
    reticleUsageHighlightCache_.dirty = true;
}

void EditorApplication::ClearLayerFocus(const bool announceStatus)
{
    if (layerFocusState_.focusedLayerId.empty())
    {
        return;
    }

    layerFocusState_.focusedLayerId.clear();
    SanitizePageReticleSelectionForCurrentFocus();
    if (announceStatus)
    {
        if (const mfd::PageDefinition* page = ActivePage(); page != nullptr)
        {
            RebuildStatus("Layer focus cleared on page '" + page->name + "'.", false);
        }
        else
        {
            RebuildStatus("Layer focus cleared.", false);
        }
    }
}

void EditorApplication::SanitizeLayerFocusForActivePage()
{
    if (const mfd::PageDefinition* page = ActivePage(); page != nullptr)
    {
        layerFocusController_.SanitizeFocusState(*page, layerFocusState_);
    }
    else
    {
        layerFocusState_ = {};
    }
}

void EditorApplication::SanitizePageReticleSelectionForCurrentFocus()
{
    if (selection_.kind != SelectionKind::PageReticle)
    {
        return;
    }

    mfd::PageDefinition* page = ActivePage();
    if (page == nullptr || selection_.pageIndex < 0 || selection_.pageIndex >= static_cast<int>(loaded_.document.pages.size()))
    {
        selection_.pageReticleIndex = -1;
        selection_.pageReticleIndices.clear();
        return;
    }

    const std::vector<int> filtered =
        layerFocusController_.FilterSelectableReticleIndices(*page, selection_.pageReticleIndices, layerFocusState_);
    if (filtered.empty())
    {
        SelectPage(selection_.pageIndex);
        return;
    }

    selection_.pageReticleIndices = filtered;
    if (std::find(filtered.begin(), filtered.end(), selection_.pageReticleIndex) == filtered.end())
    {
        selection_.pageReticleIndex = filtered.back();
    }
}

bool EditorApplication::IsPageReticleSelectableInCurrentFocus(const mfd::PageDefinition& page,
                                                              const mfd::ReticleGroup& reticle) const
{
    return layerFocusController_.IsReticleSelectable(page, reticle, layerFocusState_);
}

bool EditorApplication::ShouldDimPageReticleInCurrentFocus(const mfd::PageDefinition& page,
                                                           const mfd::ReticleGroup& reticle) const
{
    return layerFocusController_.ShouldReticleBeDimmed(page, reticle, layerFocusState_);
}

std::string EditorApplication::ActiveInsertionLayerId(const mfd::PageDefinition& page) const
{
    if (layerFocusController_.IsFocusActive(page, layerFocusState_))
    {
        return layerFocusState_.focusedLayerId;
    }

    return DefaultEditorLayerId(page);
}

void EditorApplication::DrawPagePreviewGizmos(const ViewportState& viewport, const mfd::PageDefinition& page)
{
    const std::vector<int> selectedIndices = SelectedPageReticleIndices();
    if (selectedIndices.empty() || selection_.kind != SelectionKind::PageReticle)
    {
        return;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ReticleScreenBounds selectionBounds;
    for (const int reticleIndex : selectedIndices)
    {
        const mfd::ReticleGroup& selectedReticle = page.staticReticles[static_cast<std::size_t>(reticleIndex)];
        if (!IsReticleVisibleInEditor(page, selectedReticle))
        {
            continue;
        }

        const ReticleScreenBounds bounds = ComputeReticleScreenBounds(selectedReticle, viewport);
        if (!bounds.valid)
        {
            continue;
        }

        const bool primarySelection = reticleIndex == selection_.pageReticleIndex;
        drawList->AddRect(bounds.min,
                          bounds.max,
                          primarySelection ? IM_COL32(84, 219, 201, 255) : IM_COL32(84, 219, 201, 150),
                          4.0f,
                          0,
                          primarySelection ? 2.0f : 1.2f);

        if (!selectionBounds.valid)
        {
            selectionBounds = bounds;
        }
        else
        {
            selectionBounds.min.x = std::min(selectionBounds.min.x, bounds.min.x);
            selectionBounds.min.y = std::min(selectionBounds.min.y, bounds.min.y);
            selectionBounds.max.x = std::max(selectionBounds.max.x, bounds.max.x);
            selectionBounds.max.y = std::max(selectionBounds.max.y, bounds.max.y);
        }
    }

    if (!selectionBounds.valid)
    {
        return;
    }

    selectionBounds.center = ImVec2((selectionBounds.min.x + selectionBounds.max.x) * 0.5f,
                                    (selectionBounds.min.y + selectionBounds.max.y) * 0.5f);

    if (selectedIndices.size() != 1U)
    {
        const std::string label = std::to_string(selectedIndices.size()) + " reticles selected";
        const ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
        const ImVec2 tagMin(selectionBounds.min.x + 8.0f, selectionBounds.min.y + 8.0f);
        const ImVec2 tagMax(tagMin.x + textSize.x + 14.0f, tagMin.y + textSize.y + 8.0f);
        drawList->AddRectFilled(tagMin, tagMax, IM_COL32(7, 15, 23, 220), 5.0f);
        drawList->AddText(ImVec2(tagMin.x + 7.0f, tagMin.y + 4.0f), IM_COL32(216, 233, 246, 255), label.c_str());
        return;
    }

    const mfd::ReticleGroup* reticle = SelectedPageReticle();
    if (reticle == nullptr || !IsReticleVisibleInEditor(page, *reticle))
    {
        return;
    }

    const ReticleScreenBounds bounds = ComputeReticleScreenBounds(*reticle, viewport);
    if (!bounds.valid)
    {
        return;
    }

    const mfd::Vec2 visualCenterLogical = mfd::ApplyTransform(ReticleVisualCenterLocal(*reticle), reticle->transform);
    const ImVec2 center = viewport.ToScreen(visualCenterLogical);
    const ImVec2 topLeft = bounds.min;
    const ImVec2 topRight(bounds.max.x, bounds.min.y);
    const ImVec2 bottomRight = bounds.max;
    const ImVec2 bottomLeft(bounds.min.x, bounds.max.y);
    const ImVec2 topCenter((bounds.min.x + bounds.max.x) * 0.5f, bounds.min.y);
    const ImVec2 rotateHandle(topCenter.x, topCenter.y - 26.0f);

    drawList->AddCircle(center, 5.0f, IM_COL32(84, 219, 201, 255), 18, 2.0f);
    drawList->AddLine(topCenter, rotateHandle, IM_COL32(110, 180, 250, 255), 1.5f);
    drawList->AddCircle(rotateHandle, 8.0f, IM_COL32(110, 180, 250, 255), 20, 2.0f);
    drawList->AddLine(ImVec2(rotateHandle.x + 5.0f, rotateHandle.y - 3.0f),
                      ImVec2(rotateHandle.x + 10.0f, rotateHandle.y - 7.0f),
                      IM_COL32(110, 180, 250, 255),
                      2.0f);
    drawList->AddLine(ImVec2(rotateHandle.x + 5.0f, rotateHandle.y - 3.0f),
                      ImVec2(rotateHandle.x + 10.0f, rotateHandle.y + 1.0f),
                      IM_COL32(110, 180, 250, 255),
                      2.0f);

    const auto drawCorner = [&](const ImVec2 corner)
    {
        drawList->AddRectFilled(ImVec2(corner.x - 5.5f, corner.y - 5.5f),
                                ImVec2(corner.x + 5.5f, corner.y + 5.5f),
                                IM_COL32(255, 193, 92, 255),
                                2.0f);
    };

    drawCorner(topLeft);
    drawCorner(topRight);
    drawCorner(bottomRight);
    drawCorner(bottomLeft);
}

void EditorApplication::HandlePreviewInteraction(const ViewportState& viewport)
{
    using editor::tutorial::TutorialStepId;

    mfd::PageDefinition* page = ActivePage();
    if (page == nullptr)
    {
        return;
    }

    ViewportState interactiveViewport = viewport;
    interactiveViewport.view = pagePreviewView_;

    const bool leftMouseDown = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    const bool rightMouseDown = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    const ImVec2 mouse = ImGui::GetMousePos();
    const ImVec2 viewportMax(viewport.origin.x + viewport.size.x, viewport.origin.y + viewport.size.y);
    const std::optional<mfd::Vec2> mouseLogical =
        IsPointInsideRect(mouse, viewport.origin, viewportMax) ? std::optional<mfd::Vec2> {viewport.ToLogical(mouse)}
                                                               : std::nullopt;
    const ViewportToolbarLayout toolbarLayout =
        ComputeViewportToolbarLayout(viewport.origin, mfd::SanitizeZoom(pagePreviewView_.zoom), mouseLogical);
    const bool mouseInsideViewport = IsPointInsideRect(mouse, viewport.origin, viewportMax);
    const bool mouseInsideToolbar = IsPointInsideRect(mouse, toolbarLayout.toolbarMin, toolbarLayout.toolbarMax);
    const bool helpPopupOpen = ImGui::IsPopupOpen(kPagePreviewHelpPopupId);
    if (!mouseInsideViewport || mouseInsideToolbar || helpPopupOpen)
    {
        const bool interactionButtonReleased =
            interactionMode_ == InteractionMode::PanPage ? !rightMouseDown : !leftMouseDown;
        if (interactionMode_ != InteractionMode::None && interactionButtonReleased)
        {
            interactionMode_ = InteractionMode::None;
            interactionReticleIndex_ = -1;
            interactionReticleIndices_.clear();
            interactionStartReticleTransforms_.clear();
        }
        return;
    }

    const float wheelDelta = ImGui::GetIO().MouseWheel;
    if (interactionMode_ == InteractionMode::None && std::abs(wheelDelta) > 0.0001f)
    {
        constexpr float kMinPageZoom = 0.1f;
        constexpr float kMaxPageZoom = 20.0f;
        constexpr float kWheelZoomStep = 1.12f;

        const float currentZoom = std::clamp(mfd::SanitizeZoom(pagePreviewView_.zoom), kMinPageZoom, kMaxPageZoom);
        const float nextZoom =
            std::clamp(currentZoom * std::pow(kWheelZoomStep, wheelDelta), kMinPageZoom, kMaxPageZoom);
        if (std::abs(nextZoom - currentZoom) > 0.0001f)
        {
            const mfd::Vec2 mouseLogicalBeforeZoom = interactiveViewport.ToLogical(mouse);
            const mfd::Vec2 viewedOffset = mouseLogicalBeforeZoom - pagePreviewView_.center;
            const float zoomRatio = currentZoom / nextZoom;

            pagePreviewView_.zoom = nextZoom;
            pagePreviewView_.center = {
                mouseLogicalBeforeZoom.x - viewedOffset.x * zoomRatio,
                mouseLogicalBeforeZoom.y - viewedOffset.y * zoomRatio};
            interactiveViewport.view = pagePreviewView_;
        }
    }

    const PageMinimapState minimap =
        pagePreviewViewOptions_.showMinimap ? ComputePageMinimapState(*page, interactiveViewport) : PageMinimapState {};
    const bool mouseInsideMinimap =
        pagePreviewViewOptions_.showMinimap && minimap.valid && IsPointInsideRect(mouse, minimap.contentMin, minimap.contentMax);
    if (interactionMode_ == InteractionMode::None && mouseInsideMinimap && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        interactionMode_ = InteractionMode::NavigateMinimap;
        interactionReticleIndex_ = -1;

        const LogicalBounds viewBounds = ComputeViewportLogicalBounds(interactiveViewport);
        bool clickedInsideViewRect = false;
        if (viewBounds.valid)
        {
            const ImVec2 viewA = ToMinimapScreen(minimap, viewBounds.min);
            const ImVec2 viewB = ToMinimapScreen(minimap, viewBounds.max);
            const ImVec2 viewMin(std::min(viewA.x, viewB.x), std::min(viewA.y, viewB.y));
            const ImVec2 viewMax(std::max(viewA.x, viewB.x), std::max(viewA.y, viewB.y));
            clickedInsideViewRect = IsPointInsideRect(mouse, viewMin, viewMax);
        }

        const mfd::Vec2 clickedLogical = ToMinimapLogical(minimap, mouse);
        if (clickedInsideViewRect)
        {
            minimapDragOffsetLogical_ = {
                clickedLogical.x - pagePreviewView_.center.x,
                clickedLogical.y - pagePreviewView_.center.y};
        }
        else
        {
            pagePreviewView_.center = clickedLogical;
            minimapDragOffsetLogical_ = {};
            interactiveViewport.view = pagePreviewView_;
        }
    }

    if (interactionMode_ == InteractionMode::NavigateMinimap)
    {
        if (!minimap.valid)
        {
            interactionMode_ = InteractionMode::None;
            interactionReticleIndices_.clear();
            interactionStartReticleTransforms_.clear();
            return;
        }

        const mfd::Vec2 draggedLogical = ToMinimapLogical(minimap, mouse);
        pagePreviewView_.center = {
            draggedLogical.x - minimapDragOffsetLogical_.x,
            draggedLogical.y - minimapDragOffsetLogical_.y};
        if (!leftMouseDown)
        {
            interactionMode_ = InteractionMode::None;
            interactionReticleIndices_.clear();
            interactionStartReticleTransforms_.clear();
        }
        return;
    }

    if (interactionMode_ == InteractionMode::None &&
        !mouseInsideMinimap &&
        IsMouseButtonReleased(MOUSE_BUTTON_RIGHT) &&
        !ImGui::IsMouseDragging(ImGuiMouseButton_Right))
    {
        // Keep every hovered reticle reachable so overlapping masks can still be clipped intentionally.
        const std::vector<int> hoveredReticleIndices = CollectPageReticlesAt(interactiveViewport, mouse);
        const std::vector<PageClipTarget> hoveredClipTargets = CollectPageClipTargetsAt(interactiveViewport, mouse);
        if (!hoveredReticleIndices.empty())
        {
            if (tutorial_->IsStepPhase(static_cast<int>(TutorialStepId::ClipCircleOutside), 0))
            {
                const auto tutorialTarget = std::find_if(
                    hoveredClipTargets.begin(),
                    hoveredClipTargets.end(),
                    [page, this](const PageClipTarget& candidate)
                    {
                        const mfd::ReticleGroup& reticle =
                            page->staticReticles[static_cast<std::size_t>(candidate.reticleIndex)];
                        return tutorial_->TrackedReticleId().empty() || reticle.id == tutorial_->TrackedReticleId();
                    });
                if (tutorialTarget == hoveredClipTargets.end())
                {
                    RebuildStatus("Tutorial: right-click the circle reticle created in the previous step.", true);
                    return;
                }

                tutorial_->AdvancePhase();
            }

            if (hoveredReticleIndices.size() == 1U &&
                !HasSelectedPageReticle(selection_.pageIndex, hoveredReticleIndices.front()))
            {
                SelectPageReticle(selection_.pageIndex, hoveredReticleIndices.front());
            }

            pagePreviewContextReticleIndices_ = hoveredReticleIndices;
            pagePreviewContextTargets_ = hoveredClipTargets;
            ImGui::OpenPopup("PageReticleContextMenu");
            return;
        }
    }

    if (interactionMode_ == InteractionMode::None && rightMouseDown && ImGui::IsMouseDragging(ImGuiMouseButton_Right))
    {
        interactionMode_ = InteractionMode::PanPage;
        interactionReticleIndex_ = -1;
        interactionReticleIndices_.clear();
        interactionStartReticleTransforms_.clear();
    }

    if (interactionMode_ != InteractionMode::None)
    {
        ApplyMouseTransform(interactiveViewport);
        const bool interactionButtonReleased =
            interactionMode_ == InteractionMode::PanPage ? !rightMouseDown : !leftMouseDown;
        if (interactionButtonReleased)
        {
            interactionMode_ = InteractionMode::None;
            interactionReticleIndex_ = -1;
            interactionReticleIndices_.clear();
            interactionStartReticleTransforms_.clear();
        }
        return;
    }

    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        return;
    }

    const bool additiveSelection = ImGui::GetIO().KeyCtrl;
    const std::optional<int> clickedReticleIndex = FindNearestPageReticle(viewport, mouse);
    if (!additiveSelection && page != nullptr && SelectedPageReticleCount() == 1)
    {
        mfd::ReticleGroup* selectedReticle = SelectedPageReticle();
        if (selectedReticle != nullptr && IsReticleVisibleInEditor(*page, *selectedReticle))
        {
            const ReticleScreenBounds selectedBounds = ComputeReticleScreenBounds(*selectedReticle, viewport);
            if (selectedBounds.valid)
            {
                const std::array<ImVec2, 4> selectedCorners {
                    selectedBounds.min,
                    ImVec2(selectedBounds.max.x, selectedBounds.min.y),
                    selectedBounds.max,
                    ImVec2(selectedBounds.min.x, selectedBounds.max.y)};
                const ImVec2 selectedRotateHandle(
                    (selectedBounds.min.x + selectedBounds.max.x) * 0.5f,
                    selectedBounds.min.y - 26.0f);

                const auto initializeInteraction = [&](const InteractionMode mode, const ImVec2 cornerScreen)
                {
                    interactionReticleIndex_ = selection_.pageReticleIndex;
                    interactionReticleIndices_ = {selection_.pageReticleIndex};
                    interactionStartReticleTransforms_ = {selectedReticle->transform};
                    interactionStartTransform_ = selectedReticle->transform;
                    interactionStartReticleVisualCenterLocal_ = ReticleVisualCenterLocal(*selectedReticle);
                    interactionStartMouseLogical_ = viewport.ToLogical(mouse);
                    const mfd::Vec2 interactionPivotLogical =
                        mfd::ApplyTransform(interactionStartReticleVisualCenterLocal_, interactionStartTransform_);
                    interactionStartAngleDegrees_ =
                        std::atan2(interactionStartMouseLogical_.y - interactionPivotLogical.y,
                                   interactionStartMouseLogical_.x - interactionPivotLogical.x) *
                        180.0f / 3.14159265f;
                    interactionStartDistance_ = std::max(
                        0.001f,
                        std::sqrt(
                            std::pow(interactionStartMouseLogical_.x - interactionPivotLogical.x, 2.0f) +
                            std::pow(interactionStartMouseLogical_.y - interactionPivotLogical.y, 2.0f)));
                    interactionStartCenterScreen_ = viewport.ToScreen(interactionPivotLogical);
                    interactionStartCornerScreen_ = cornerScreen;
                    PushUndoSnapshot();
                    interactionMode_ = mode;
                };

                if (pagePreviewViewOptions_.showGizmos)
                {
                    if (Distance(mouse, selectedRotateHandle) <= 16.0f)
                    {
                        initializeInteraction(InteractionMode::RotateReticle, selectedBounds.center);
                        return;
                    }

                    for (const ImVec2 corner : selectedCorners)
                    {
                        if (Distance(mouse, corner) <= 16.0f)
                        {
                            initializeInteraction(InteractionMode::ScaleReticle, corner);
                            return;
                        }
                    }
                }
            }
        }
    }

    if (!additiveSelection &&
        SelectedPageReticleCount() > 1 &&
        clickedReticleIndex.has_value() &&
        HasSelectedPageReticle(selection_.pageIndex, *clickedReticleIndex))
    {
        // Preserve the current multi-selection when dragging one of its members.
        const std::vector<int> selectedIndices = SelectedPageReticleIndices();
        interactionReticleIndex_ = selection_.pageReticleIndex;
        interactionReticleIndices_ = selectedIndices;
        interactionStartReticleTransforms_.clear();
        interactionStartReticleTransforms_.reserve(selectedIndices.size());
        for (const int reticleIndex : selectedIndices)
        {
            interactionStartReticleTransforms_.push_back(
                page->staticReticles[static_cast<std::size_t>(reticleIndex)].transform);
        }
        interactionStartMouseLogical_ = viewport.ToLogical(mouse);
        PushUndoSnapshot();
        interactionMode_ = InteractionMode::MoveReticle;
        return;
    }

    UpdateReticleSelectionFromClick(viewport, additiveSelection);
    if (additiveSelection || SelectedPageReticleCount() != 1)
    {
        return;
    }

    mfd::ReticleGroup* reticle = SelectedPageReticle();
    if (reticle == nullptr)
    {
        return;
    }

    const ReticleScreenBounds bounds = ComputeReticleScreenBounds(*reticle, viewport);
    if (!bounds.valid)
    {
        return;
    }

    const ImVec2 center = bounds.center;
    const std::array<ImVec2, 4> corners {
        bounds.min,
        ImVec2(bounds.max.x, bounds.min.y),
        bounds.max,
        ImVec2(bounds.min.x, bounds.max.y)};
    const ImVec2 rotateHandle((bounds.min.x + bounds.max.x) * 0.5f, bounds.min.y - 26.0f);

    interactionReticleIndex_ = selection_.pageReticleIndex;
    interactionReticleIndices_ = {selection_.pageReticleIndex};
    interactionStartReticleTransforms_ = {reticle->transform};
    interactionStartTransform_ = reticle->transform;
    interactionStartReticleVisualCenterLocal_ = ReticleVisualCenterLocal(*reticle);
    interactionStartMouseLogical_ = viewport.ToLogical(mouse);
    const mfd::Vec2 interactionPivotLogical =
        mfd::ApplyTransform(interactionStartReticleVisualCenterLocal_, interactionStartTransform_);
    interactionStartAngleDegrees_ =
        std::atan2(interactionStartMouseLogical_.y - interactionPivotLogical.y,
                   interactionStartMouseLogical_.x - interactionPivotLogical.x) *
        180.0f / 3.14159265f;
    interactionStartDistance_ = std::max(
        0.001f,
        std::sqrt(
            std::pow(interactionStartMouseLogical_.x - interactionPivotLogical.x, 2.0f) +
            std::pow(interactionStartMouseLogical_.y - interactionPivotLogical.y, 2.0f)));
    interactionStartCenterScreen_ = viewport.ToScreen(interactionPivotLogical);
    interactionStartCornerScreen_ = center;

    if (pagePreviewViewOptions_.showGizmos && Distance(mouse, rotateHandle) <= 16.0f)
    {
        PushUndoSnapshot();
        interactionMode_ = InteractionMode::RotateReticle;
    }
    else if (mouse.x >= bounds.min.x - 8.0f && mouse.x <= bounds.max.x + 8.0f &&
             mouse.y >= bounds.min.y - 8.0f && mouse.y <= bounds.max.y + 8.0f)
    {
        if (pagePreviewViewOptions_.showGizmos)
        {
            for (const ImVec2 corner : corners)
            {
                if (Distance(mouse, corner) <= 16.0f)
                {
                    PushUndoSnapshot();
                    interactionMode_ = InteractionMode::ScaleReticle;
                    interactionStartCornerScreen_ = corner;
                    return;
                }
            }
        }

        PushUndoSnapshot();
        interactionMode_ = InteractionMode::MoveReticle;
    }
    else
    {
        interactionMode_ = InteractionMode::None;
        interactionReticleIndex_ = -1;
        interactionReticleIndices_.clear();
        interactionStartReticleTransforms_.clear();
    }
}

bool EditorApplication::ApplyPageReticleClipping(const int reticleIndex,
                                                 const mfd::ReticleClipMode mode,
                                                 std::string primitiveId)
{
    mfd::PageDefinition* page = ActivePage();
    if (page == nullptr ||
        reticleIndex < 0 ||
        reticleIndex >= static_cast<int>(page->staticReticles.size()))
    {
        return false;
    }

    mfd::ReticleGroup& reticle = page->staticReticles[static_cast<std::size_t>(reticleIndex)];
    if (primitiveId.empty())
    {
        primitiveId = reticle.clipping.primitiveId;
    }

    if (reticle.clipping.mode == mode && reticle.clipping.primitiveId == primitiveId)
    {
        return false;
    }

    PushUndoSnapshot();
    reticle.clipping.mode = mode;
    reticle.clipping.primitiveId = std::move(primitiveId);
    pagePreviewContextReticleIndices_ = {reticleIndex};
    pagePreviewContextTargets_.clear();
    for (int primitiveIndex = 0; primitiveIndex < static_cast<int>(reticle.primitives.size()); ++primitiveIndex)
    {
        if (reticle.primitives[static_cast<std::size_t>(primitiveIndex)].id == reticle.clipping.primitiveId)
        {
            pagePreviewContextTargets_.push_back(PageClipTarget {reticleIndex, primitiveIndex});
            break;
        }
    }
    SelectPageReticle(selection_.pageIndex, reticleIndex);

    if (mode == mfd::ReticleClipMode::None)
    {
        RebuildStatus("Clipping disabled for page reticle '" + reticle.id + "'.", false);
    }
    else
    {
        RebuildStatus(std::string(ReticleClipModeLabel(mode)) + " enabled on primitive '" + reticle.clipping.primitiveId +
                          "' for page reticle '" + reticle.id + "'.",
                      false);
    }

    return true;
}

void EditorApplication::DrawPageReticleContextMenu()
{
    if (!ImGui::BeginPopup("PageReticleContextMenu"))
    {
        return;
    }

    mfd::PageDefinition* page = ActivePage();
    if (page == nullptr || pagePreviewContextReticleIndices_.empty())
    {
        ImGui::TextDisabled("No page reticle is under the mouse.");
        ImGui::EndPopup();
        return;
    }

    auto drawClipItemsForTarget = [this, page](const int reticleIndex, const int primitiveIndex)
    {
        if (reticleIndex < 0 || reticleIndex >= static_cast<int>(page->staticReticles.size()))
        {
            ImGui::TextDisabled("Invalid reticle target.");
            return;
        }

        mfd::ReticleGroup& reticle = page->staticReticles[static_cast<std::size_t>(reticleIndex)];
        if (primitiveIndex < 0 || primitiveIndex >= static_cast<int>(reticle.primitives.size()))
        {
            ImGui::TextDisabled("Invalid primitive target.");
            return;
        }

        mfd::Primitive& primitive = reticle.primitives[static_cast<std::size_t>(primitiveIndex)];
        if (primitive.id.empty() || !mfd::SupportsReticleClipPrimitive(primitive))
        {
            ImGui::TextDisabled("The selected primitive does not support clipping.");
            ImGui::TextDisabled("Supported mask shapes: triangle, square, rectangle, circle, ellipse.");
            return;
        }

        ImGui::TextDisabled("%s", (primitive.id + " (" + PrimitiveTypeLabel(primitive.type) + ")").c_str());
        ImGui::Separator();

        if (ImGui::MenuItem("Clip inside",
                            nullptr,
                            reticle.clipping.mode == mfd::ReticleClipMode::Inner &&
                                reticle.clipping.primitiveId == primitive.id))
        {
            ApplyPageReticleClipping(reticleIndex, mfd::ReticleClipMode::Inner, primitive.id);
        }
        ShowItemTooltip("Erase the inside of this convex primitive toward the page background color.");

        if (ImGui::MenuItem("Clip outside",
                            nullptr,
                            reticle.clipping.mode == mfd::ReticleClipMode::Outer &&
                                reticle.clipping.primitiveId == primitive.id))
        {
            const bool tutorialClipMatched = tutorial_->MatchesTarget("context_clip_outer");
            if (ApplyPageReticleClipping(reticleIndex, mfd::ReticleClipMode::Outer, primitive.id) &&
                tutorialClipMatched &&
                (tutorial_->TrackedReticleId().empty() || reticle.id == tutorial_->TrackedReticleId()))
            {
                tutorial_->CompleteStep();
            }
        }
        ShowItemTooltip("Erase everything outside this convex primitive toward the page background color.");
        tutorial_->DrawHalo(
            "context_clip_outer",
            "Click Clip outside",
            "Keep only the inside of the tutorial circle so you can discover page-level masking.");

        if (ImGui::MenuItem("Disable clipping",
                            nullptr,
                            reticle.clipping.mode == mfd::ReticleClipMode::None))
        {
            ApplyPageReticleClipping(reticleIndex, mfd::ReticleClipMode::None, reticle.clipping.primitiveId);
        }
        ShowItemTooltip("Disable clipping for this page reticle.");
    };

    const std::vector<int> selectedIndices = SelectedPageReticleIndices();
    const bool hasSelectedGroup = !selectedIndices.empty();
    const bool canPasteSelection = !pageReticleClipboard_.empty();
    if (hasSelectedGroup || canPasteSelection)
    {
        if (hasSelectedGroup)
        {
            ImGui::TextDisabled("%d selected reticle%s",
                                static_cast<int>(selectedIndices.size()),
                                selectedIndices.size() == 1U ? "" : "s");
        }
        else
        {
            ImGui::TextDisabled("Reticle clipboard");
        }

        if (ImGui::MenuItem("Copy selection", "Ctrl+C", false, hasSelectedGroup))
        {
            CopySelectedPageReticles();
        }
        ShowItemTooltip("Copy the currently selected page reticle group.");

        if (ImGui::MenuItem("Cut selection", "Ctrl+X", false, hasSelectedGroup))
        {
            CutSelectedPageReticles();
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }
        ShowItemTooltip("Copy the current selection to the clipboard, then remove it from the page.");

        if (ImGui::MenuItem("Paste copies", "Ctrl+V", false, canPasteSelection))
        {
            PasteCopiedPageReticles();
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }
        ShowItemTooltip("Paste the current reticle clipboard onto the active page.");

        if (ImGui::MenuItem("Extract as reticle...", nullptr, false, hasSelectedGroup))
        {
            OpenReticleExtractionPopup();
        }
        ShowItemTooltip("Replace the current page-reticle selection with one reusable library template while preserving the visual result.");

        if (ImGui::MenuItem("Delete selection", "Del", false, hasSelectedGroup))
        {
            DeleteSelection();
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }
        ShowItemTooltip("Delete the selected page reticle group.");
        ImGui::Separator();
    }

    auto collectClipTargetsForReticle = [this](const int reticleIndex)
    {
        std::vector<PageClipTarget> targets;
        for (const PageClipTarget& target : pagePreviewContextTargets_)
        {
            if (target.reticleIndex == reticleIndex)
            {
                targets.push_back(target);
            }
        }
        return targets;
    };

    auto drawReticleContextContent = [this, page, &drawClipItemsForTarget, &collectClipTargetsForReticle](const int reticleIndex)
    {
        if (reticleIndex < 0 || reticleIndex >= static_cast<int>(page->staticReticles.size()))
        {
            ImGui::TextDisabled("Invalid reticle target.");
            return;
        }

        mfd::ReticleGroup& reticle = page->staticReticles[static_cast<std::size_t>(reticleIndex)];
        const bool selected = HasSelectedPageReticle(selection_.pageIndex, reticleIndex);
        if (ImGui::MenuItem("Select only", nullptr, selected))
        {
            SelectPageReticle(selection_.pageIndex, reticleIndex);
        }
        ShowItemTooltip("Focus only this reticle in the inspector and preview.");

        const char* toggleLabel = selected ? "Remove from selection" : "Add to selection";
        if (ImGui::MenuItem(toggleLabel, "Ctrl+click"))
        {
            TogglePageReticleSelection(selection_.pageIndex, reticleIndex);
        }
        ShowItemTooltip("Add or remove this reticle from the current multi-selection.");

        const std::vector<PageClipTarget> reticleTargets = collectClipTargetsForReticle(reticleIndex);
        ImGui::Separator();
        if (reticleTargets.empty())
        {
            ImGui::TextDisabled("No convex primitive under the mouse for clipping.");
            ImGui::TextDisabled("Supported mask shapes: triangle, square, rectangle, circle, ellipse.");
            return;
        }

        if (reticleTargets.size() == 1U)
        {
            drawClipItemsForTarget(reticleIndex, reticleTargets.front().primitiveIndex);
            return;
        }

        ImGui::TextDisabled("Clip through one of the hovered primitives:");
        for (const PageClipTarget& target : reticleTargets)
        {
            const mfd::Primitive& primitive =
                reticle.primitives[static_cast<std::size_t>(target.primitiveIndex)];
            const std::string primitiveLabel =
                (primitive.id.empty() ? std::string {"primitive"} : primitive.id) +
                " (" + PrimitiveTypeLabel(primitive.type) + ")##context_primitive_" +
                std::to_string(reticleIndex) + "_" + std::to_string(target.primitiveIndex);
            if (ImGui::BeginMenu(primitiveLabel.c_str()))
            {
                drawClipItemsForTarget(reticleIndex, target.primitiveIndex);
                ImGui::EndMenu();
            }
        }
    };

    if (pagePreviewContextReticleIndices_.size() == 1U)
    {
        const int reticleIndex = pagePreviewContextReticleIndices_.front();
        if (reticleIndex >= 0 && reticleIndex < static_cast<int>(page->staticReticles.size()))
        {
            const mfd::ReticleGroup& reticle = page->staticReticles[static_cast<std::size_t>(reticleIndex)];
            ImGui::TextUnformatted(reticle.id.c_str());
            ImGui::Separator();
            drawReticleContextContent(reticleIndex);
        }
    }
    else
    {
        ImGui::TextDisabled("Reticles under the mouse");
        for (const int reticleIndex : pagePreviewContextReticleIndices_)
        {
            if (reticleIndex < 0 || reticleIndex >= static_cast<int>(page->staticReticles.size()))
            {
                continue;
            }

            const mfd::ReticleGroup& reticle = page->staticReticles[static_cast<std::size_t>(reticleIndex)];
            std::string label =
                (reticle.id.empty() ? std::string {"reticle"} : reticle.id) +
                "##context_reticle_" + std::to_string(reticleIndex);
            if (ImGui::BeginMenu(label.c_str()))
            {
                drawReticleContextContent(reticleIndex);
                ImGui::EndMenu();
            }
        }
    }

    ImGui::Separator();
    ImGui::TextDisabled("Right-click lists every hovered reticle, then lets you target clipping per hovered primitive.");

    ImGui::EndPopup();
}

void EditorApplication::HandleLibraryPreviewInteraction(const ViewportState& viewport)
{
    mfd::ReticleGroup* reticle = SelectedLibraryReticle();
    if (reticle == nullptr)
    {
        return;
    }

    const bool leftMouseDown = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    const bool rightMouseDown = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    if (interactionMode_ == InteractionMode::PanPage)
    {
        if (!rightMouseDown)
        {
            interactionMode_ = InteractionMode::None;
        }
        else
        {
            ApplyMouseTransform(viewport);
        }
        return;
    }

    const bool hasActivePrimitiveInteraction =
        interactionMode_ == InteractionMode::MovePrimitive || interactionMode_ == InteractionMode::EditPrimitiveHandle;

    if (hasActivePrimitiveInteraction)
    {
        if (interactionPrimitiveIndex_ < 0 || interactionPrimitiveIndex_ >= static_cast<int>(reticle->primitives.size()))
        {
            interactionMode_ = InteractionMode::None;
            interactionPrimitiveIndex_ = -1;
            interactionHandleKind_ = PrimitiveHandleKind::None;
            interactionHandleIndex_ = -1;
            return;
        }

        mfd::Primitive& primitive = reticle->primitives[static_cast<std::size_t>(interactionPrimitiveIndex_)];
        const mfd::Vec2 mouseLogical = viewport.ToLogical(ImGui::GetMousePos());
        const mfd::Vec2 mouseReticleLocal = InverseTransformPoint(mouseLogical, reticle->transform);

        if (interactionMode_ == InteractionMode::MovePrimitive)
        {
            primitive.transform.position =
                interactionStartPrimitive_.transform.position + (mouseReticleLocal - interactionStartMouseReticleLocal_);
        }
        else if (interactionMode_ == InteractionMode::EditPrimitiveHandle)
        {
            const mfd::Vec2 mousePrimitiveLocal = InverseTransformPoint(mouseReticleLocal, interactionStartPrimitive_.transform);

            if (auto* line = std::get_if<mfd::LineGeometry>(&primitive.geometry))
            {
                if (interactionHandleIndex_ == 0)
                {
                    line->start = mousePrimitiveLocal;
                }
                else if (interactionHandleIndex_ == 1)
                {
                    line->end = mousePrimitiveLocal;
                }
            }
            else if (auto* circle = std::get_if<mfd::CircleGeometry>(&primitive.geometry))
            {
                circle->radius = std::max(0.001f, std::sqrt(mousePrimitiveLocal.x * mousePrimitiveLocal.x +
                                                            mousePrimitiveLocal.y * mousePrimitiveLocal.y));
            }
            else if (auto* ring = std::get_if<mfd::RingGeometry>(&primitive.geometry))
            {
                const float radius = std::max(
                    0.001f,
                    std::sqrt(mousePrimitiveLocal.x * mousePrimitiveLocal.x + mousePrimitiveLocal.y * mousePrimitiveLocal.y));
                if (interactionHandleIndex_ == 0)
                {
                    ring->innerRadius = std::min(radius, std::max(0.001f, ring->outerRadius - 0.001f));
                }
                else if (interactionHandleIndex_ == 1)
                {
                    ring->outerRadius = std::max(radius, ring->innerRadius + 0.001f);
                }
            }
            else if (auto* rectangle = std::get_if<mfd::RectangleGeometry>(&primitive.geometry))
            {
                rectangle->width = std::max(0.001f, std::abs(mousePrimitiveLocal.x) * 2.0f);
                rectangle->height = std::max(0.001f, std::abs(mousePrimitiveLocal.y) * 2.0f);
            }
            else if (auto* ellipse = std::get_if<mfd::EllipseGeometry>(&primitive.geometry))
            {
                if (interactionHandleIndex_ == 0)
                {
                    ellipse->width = std::max(0.001f, std::abs(mousePrimitiveLocal.x) * 2.0f);
                }
                else
                {
                    ellipse->height = std::max(0.001f, std::abs(mousePrimitiveLocal.y) * 2.0f);
                }
            }
            else if (auto* square = std::get_if<mfd::SquareGeometry>(&primitive.geometry))
            {
                square->width = std::max(0.001f, std::abs(mousePrimitiveLocal.x) * 2.0f);
                square->height = std::max(0.001f, std::abs(mousePrimitiveLocal.y) * 2.0f);
            }
            else if (auto* diamond = std::get_if<mfd::DiamondGeometry>(&primitive.geometry))
            {
                if ((interactionHandleIndex_ % 2) == 0)
                {
                    diamond->height = std::max(0.001f, std::abs(mousePrimitiveLocal.y) * 2.0f);
                }
                else
                {
                    diamond->width = std::max(0.001f, std::abs(mousePrimitiveLocal.x) * 2.0f);
                }
            }
            else if (auto* triangle = std::get_if<mfd::TriangleGeometry>(&primitive.geometry))
            {
                if (interactionHandleIndex_ >= 0 && interactionHandleIndex_ < 3)
                {
                    triangle->points[static_cast<std::size_t>(interactionHandleIndex_)] = mousePrimitiveLocal;
                }
            }
            else if (auto* polyline = std::get_if<mfd::PolylineGeometry>(&primitive.geometry))
            {
                if (interactionHandleIndex_ >= 0 &&
                    interactionHandleIndex_ < static_cast<int>(polyline->points.size()))
                {
                    polyline->points[static_cast<std::size_t>(interactionHandleIndex_)] = mousePrimitiveLocal;
                }
            }
            else if (auto* bezier = std::get_if<mfd::BezierGeometry>(&primitive.geometry))
            {
                if (interactionHandleIndex_ >= 0 &&
                    interactionHandleIndex_ < static_cast<int>(bezier->controlPoints.size()))
                {
                    bezier->controlPoints[static_cast<std::size_t>(interactionHandleIndex_)] = mousePrimitiveLocal;
                }
            }
            else if (auto* arc = std::get_if<mfd::ArcGeometry>(&primitive.geometry))
            {
                if (interactionHandleIndex_ == 2)
                {
                    arc->radius = std::max(
                        0.001f,
                        std::sqrt(mousePrimitiveLocal.x * mousePrimitiveLocal.x + mousePrimitiveLocal.y * mousePrimitiveLocal.y));
                }
                else
                {
                    const float angleDegrees = std::atan2(mousePrimitiveLocal.y, mousePrimitiveLocal.x) * 180.0f / PI;
                    if (interactionHandleIndex_ == 0)
                    {
                        arc->startAngleDegrees = angleDegrees;
                    }
                    else if (interactionHandleIndex_ == 1)
                    {
                        arc->endAngleDegrees = angleDegrees;
                    }
                }
            }
        }

        if (!leftMouseDown)
        {
            interactionMode_ = InteractionMode::None;
            interactionPrimitiveIndex_ = -1;
            interactionHandleKind_ = PrimitiveHandleKind::None;
            interactionHandleIndex_ = -1;
        }
        return;
    }

    const ImVec2 mouse = ImGui::GetMousePos();
    const ImVec2 viewportMax(viewport.origin.x + viewport.size.x, viewport.origin.y + viewport.size.y);
    const std::optional<mfd::Vec2> mouseLogical =
        IsPointInsideRect(mouse, viewport.origin, viewportMax) ? std::optional<mfd::Vec2> {viewport.ToLogical(mouse)}
                                                               : std::nullopt;
    const ViewportToolbarLayout toolbarLayout =
        ComputeViewportToolbarLayout(viewport.origin, mfd::SanitizeZoom(libraryPreviewView_.zoom), mouseLogical);
    const bool mouseInsideViewport = IsPointInsideRect(mouse, viewport.origin, viewportMax);
    const bool mouseInsideToolbar = IsPointInsideRect(mouse, toolbarLayout.toolbarMin, toolbarLayout.toolbarMax);
    const bool helpPopupOpen = ImGui::IsPopupOpen(kLibraryPreviewHelpPopupId);
    if (!mouseInsideViewport || mouseInsideToolbar || helpPopupOpen)
    {
        return;
    }

    const float wheelDelta = ImGui::GetIO().MouseWheel;
    if (interactionMode_ == InteractionMode::None && std::abs(wheelDelta) > 0.0001f)
    {
        constexpr float kMinStudioZoom = 0.1f;
        constexpr float kMaxStudioZoom = 20.0f;
        constexpr float kWheelZoomStep = 1.12f;

        const float currentZoom =
            std::clamp(mfd::SanitizeZoom(libraryPreviewView_.zoom), kMinStudioZoom, kMaxStudioZoom);
        const float nextZoom =
            std::clamp(currentZoom * std::pow(kWheelZoomStep, wheelDelta), kMinStudioZoom, kMaxStudioZoom);
        if (std::abs(nextZoom - currentZoom) > 0.0001f)
        {
            const mfd::Vec2 mouseLogicalBeforeZoom = viewport.ToLogical(mouse);
            const mfd::Vec2 viewedOffset = mouseLogicalBeforeZoom - libraryPreviewView_.center;
            const float zoomRatio = currentZoom / nextZoom;

            libraryPreviewView_.zoom = nextZoom;
            libraryPreviewView_.center = {
                mouseLogicalBeforeZoom.x - viewedOffset.x * zoomRatio,
                mouseLogicalBeforeZoom.y - viewedOffset.y * zoomRatio};
        }
    }

    if (interactionMode_ == InteractionMode::None && rightMouseDown && ImGui::IsMouseDragging(ImGuiMouseButton_Right))
    {
        interactionMode_ = InteractionMode::PanPage;
        interactionPrimitiveIndex_ = -1;
        interactionHandleKind_ = PrimitiveHandleKind::None;
        interactionHandleIndex_ = -1;
        ApplyMouseTransform(viewport);
        return;
    }

    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        return;
    }

    std::optional<int> bestPrimitiveIndex = FindNearestLibraryPrimitive(viewport, mouse);
    if (!bestPrimitiveIndex.has_value())
    {
        SelectLibraryReticle(reticle->id);
        return;
    }

    SelectLibraryPrimitive(reticle->id, *bestPrimitiveIndex);
    mfd::Primitive& primitive = reticle->primitives[static_cast<std::size_t>(*bestPrimitiveIndex)];

    auto toScreenPoint = [&](const mfd::Vec2 localPoint)
    {
        return viewport.ToScreen(mfd::ApplyTransform(mfd::ApplyTransform(localPoint, primitive.transform), reticle->transform));
    };

    interactionPrimitiveIndex_ = *bestPrimitiveIndex;
    interactionStartPrimitive_ = primitive;
    interactionStartMouseLogical_ = viewport.ToLogical(mouse);
    interactionStartMouseReticleLocal_ = InverseTransformPoint(interactionStartMouseLogical_, reticle->transform);
    interactionStartMousePrimitiveLocal_ = InverseTransformPoint(interactionStartMouseReticleLocal_, primitive.transform);
    interactionHandleKind_ = PrimitiveHandleKind::None;
    interactionHandleIndex_ = -1;

    auto matchesHandle = [&](const ImVec2 handlePoint, const float radius = 12.0f)
    {
        return Distance(handlePoint, mouse) <= radius;
    };

    if (const auto* line = std::get_if<mfd::LineGeometry>(&primitive.geometry))
    {
        const std::array<mfd::Vec2, 2> points {line->start, line->end};
        for (int index = 0; index < 2; ++index)
        {
            if (matchesHandle(toScreenPoint(points[static_cast<std::size_t>(index)])))
            {
                PushUndoSnapshot();
                interactionMode_ = InteractionMode::EditPrimitiveHandle;
                interactionHandleKind_ = PrimitiveHandleKind::Point;
                interactionHandleIndex_ = index;
                return;
            }
        }
    }
    else if (const auto* circle = std::get_if<mfd::CircleGeometry>(&primitive.geometry))
    {
        if (matchesHandle(toScreenPoint({circle->radius, 0.0f})))
        {
            PushUndoSnapshot();
            interactionMode_ = InteractionMode::EditPrimitiveHandle;
            interactionHandleKind_ = PrimitiveHandleKind::Radius;
            interactionHandleIndex_ = 0;
            return;
        }
    }
    else if (const auto* ring = std::get_if<mfd::RingGeometry>(&primitive.geometry))
    {
        const std::array<mfd::Vec2, 2> handles {{
            {ring->innerRadius, 0.0f},
            {ring->outerRadius, 0.0f},
        }};
        for (int index = 0; index < 2; ++index)
        {
            if (matchesHandle(toScreenPoint(handles[static_cast<std::size_t>(index)])))
            {
                PushUndoSnapshot();
                interactionMode_ = InteractionMode::EditPrimitiveHandle;
                interactionHandleKind_ = PrimitiveHandleKind::Radius;
                interactionHandleIndex_ = index;
                return;
            }
        }
    }
    else if (const auto* rectangle = std::get_if<mfd::RectangleGeometry>(&primitive.geometry))
    {
        const std::array<mfd::Vec2, 4> corners {{
            {-rectangle->width * 0.5f, -rectangle->height * 0.5f},
            {rectangle->width * 0.5f, -rectangle->height * 0.5f},
            {rectangle->width * 0.5f, rectangle->height * 0.5f},
            {-rectangle->width * 0.5f, rectangle->height * 0.5f},
        }};
        for (int index = 0; index < 4; ++index)
        {
            if (matchesHandle(toScreenPoint(corners[static_cast<std::size_t>(index)])))
            {
                PushUndoSnapshot();
                interactionMode_ = InteractionMode::EditPrimitiveHandle;
                interactionHandleKind_ = PrimitiveHandleKind::RectangleCorner;
                interactionHandleIndex_ = index;
                return;
            }
        }
    }
    else if (const auto* ellipse = std::get_if<mfd::EllipseGeometry>(&primitive.geometry))
    {
        const std::array<mfd::Vec2, 2> handles {{
            {ellipse->width * 0.5f, 0.0f},
            {0.0f, ellipse->height * 0.5f},
        }};
        for (int index = 0; index < 2; ++index)
        {
            if (matchesHandle(toScreenPoint(handles[static_cast<std::size_t>(index)])))
            {
                PushUndoSnapshot();
                interactionMode_ = InteractionMode::EditPrimitiveHandle;
                interactionHandleKind_ = PrimitiveHandleKind::Radius;
                interactionHandleIndex_ = index;
                return;
            }
        }
    }
    else if (const auto* square = std::get_if<mfd::SquareGeometry>(&primitive.geometry))
    {
        const std::array<mfd::Vec2, 4> corners {{
            {-square->width * 0.5f, -square->height * 0.5f},
            {square->width * 0.5f, -square->height * 0.5f},
            {square->width * 0.5f, square->height * 0.5f},
            {-square->width * 0.5f, square->height * 0.5f},
        }};
        for (int index = 0; index < 4; ++index)
        {
            if (matchesHandle(toScreenPoint(corners[static_cast<std::size_t>(index)])))
            {
                PushUndoSnapshot();
                interactionMode_ = InteractionMode::EditPrimitiveHandle;
                interactionHandleKind_ = PrimitiveHandleKind::RectangleCorner;
                interactionHandleIndex_ = index;
                return;
            }
        }
    }
    else if (const auto* diamond = std::get_if<mfd::DiamondGeometry>(&primitive.geometry))
    {
        const std::array<mfd::Vec2, 4> handles {{
            {0.0f, diamond->height * 0.5f},
            {diamond->width * 0.5f, 0.0f},
            {0.0f, -diamond->height * 0.5f},
            {-diamond->width * 0.5f, 0.0f},
        }};
        for (int index = 0; index < 4; ++index)
        {
            if (matchesHandle(toScreenPoint(handles[static_cast<std::size_t>(index)])))
            {
                PushUndoSnapshot();
                interactionMode_ = InteractionMode::EditPrimitiveHandle;
                interactionHandleKind_ = PrimitiveHandleKind::DiamondAxis;
                interactionHandleIndex_ = index;
                return;
            }
        }
    }
    else if (const auto* triangle = std::get_if<mfd::TriangleGeometry>(&primitive.geometry))
    {
        for (int index = 0; index < 3; ++index)
        {
            if (matchesHandle(toScreenPoint(triangle->points[static_cast<std::size_t>(index)])))
            {
                PushUndoSnapshot();
                interactionMode_ = InteractionMode::EditPrimitiveHandle;
                interactionHandleKind_ = PrimitiveHandleKind::Point;
                interactionHandleIndex_ = index;
                return;
            }
        }
    }
    else if (const auto* polyline = std::get_if<mfd::PolylineGeometry>(&primitive.geometry))
    {
        for (int index = 0; index < static_cast<int>(polyline->points.size()); ++index)
        {
            if (matchesHandle(toScreenPoint(polyline->points[static_cast<std::size_t>(index)])))
            {
                PushUndoSnapshot();
                interactionMode_ = InteractionMode::EditPrimitiveHandle;
                interactionHandleKind_ = PrimitiveHandleKind::Point;
                interactionHandleIndex_ = index;
                return;
            }
        }
    }
    else if (const auto* bezier = std::get_if<mfd::BezierGeometry>(&primitive.geometry))
    {
        for (int index = 0; index < static_cast<int>(bezier->controlPoints.size()); ++index)
        {
            if (matchesHandle(toScreenPoint(bezier->controlPoints[static_cast<std::size_t>(index)])))
            {
                PushUndoSnapshot();
                interactionMode_ = InteractionMode::EditPrimitiveHandle;
                interactionHandleKind_ = PrimitiveHandleKind::Point;
                interactionHandleIndex_ = index;
                return;
            }
        }
    }
    else if (const auto* arc = std::get_if<mfd::ArcGeometry>(&primitive.geometry))
    {
        const std::vector<mfd::Vec2> arcPoints =
            ApproximateArcPoints(arc->radius, arc->startAngleDegrees, arc->endAngleDegrees, arc->segments);
        const float middleAngleDegrees = (arc->startAngleDegrees + arc->endAngleDegrees) * 0.5f;
        const float middleAngleRadians = middleAngleDegrees * PI / 180.0f;
        const mfd::Vec2 radiusHandle {
            std::cos(middleAngleRadians) * std::abs(arc->radius),
            std::sin(middleAngleRadians) * std::abs(arc->radius)};

        if (!arcPoints.empty() && matchesHandle(toScreenPoint(arcPoints.front())))
        {
            PushUndoSnapshot();
            interactionMode_ = InteractionMode::EditPrimitiveHandle;
            interactionHandleKind_ = PrimitiveHandleKind::Point;
            interactionHandleIndex_ = 0;
            return;
        }

        if (!arcPoints.empty() && matchesHandle(toScreenPoint(arcPoints.back())))
        {
            PushUndoSnapshot();
            interactionMode_ = InteractionMode::EditPrimitiveHandle;
            interactionHandleKind_ = PrimitiveHandleKind::Point;
            interactionHandleIndex_ = 1;
            return;
        }

        if (matchesHandle(toScreenPoint(radiusHandle)))
        {
            PushUndoSnapshot();
            interactionMode_ = InteractionMode::EditPrimitiveHandle;
            interactionHandleKind_ = PrimitiveHandleKind::Radius;
            interactionHandleIndex_ = 2;
            return;
        }
    }

    const ReticleScreenBounds bounds = ComputePrimitiveScreenBounds(*reticle, primitive, viewport);
    if (bounds.valid &&
        mouse.x >= bounds.min.x - 8.0f && mouse.x <= bounds.max.x + 8.0f &&
        mouse.y >= bounds.min.y - 8.0f && mouse.y <= bounds.max.y + 8.0f)
    {
        PushUndoSnapshot();
        interactionMode_ = InteractionMode::MovePrimitive;
        return;
    }

    interactionPrimitiveIndex_ = -1;
}

void EditorApplication::SelectPage(const int pageIndex)
{
    const int previousPageIndex = selection_.pageIndex;
    selection_.kind = SelectionKind::Page;
    selection_.pageIndex = std::clamp(pageIndex, 0, std::max(0, static_cast<int>(loaded_.document.pages.size()) - 1));
    if (selection_.pageIndex != previousPageIndex)
    {
        layerFocusState_ = {};
    }
    if (mfd::PageDefinition* page = ActivePage(); page != nullptr)
    {
        BootstrapEditorLayersForPage(*page);
    }
    SanitizeLayerFocusForActivePage();
    selection_.pageReticleIndex = -1;
    selection_.pageReticleIndices.clear();
    interactionMode_ = InteractionMode::None;
    interactionPrimitiveIndex_ = -1;
    interactionReticleIndex_ = -1;
    interactionReticleIndices_.clear();
    interactionStartReticleTransforms_.clear();
    interactionHandleIndex_ = -1;
    interactionHandleKind_ = PrimitiveHandleKind::None;
    ResetPagePreviewView();
}

void EditorApplication::SelectPageReticle(const int pageIndex, const int reticleIndex)
{
    if (pageIndex < 0 || pageIndex >= static_cast<int>(loaded_.document.pages.size()))
    {
        return;
    }

    mfd::PageDefinition& page = loaded_.document.pages[static_cast<std::size_t>(pageIndex)];
    if (reticleIndex < 0 || reticleIndex >= static_cast<int>(page.staticReticles.size()) ||
        !IsPageReticleSelectableInCurrentFocus(page, page.staticReticles[static_cast<std::size_t>(reticleIndex)]))
    {
        return;
    }

    selection_.kind = SelectionKind::PageReticle;
    selection_.pageIndex = pageIndex;
    selection_.pageReticleIndex = reticleIndex;
    selection_.pageReticleIndices = {reticleIndex};
    interactionMode_ = InteractionMode::None;
    interactionReticleIndex_ = -1;
    interactionReticleIndices_.clear();
    interactionStartReticleTransforms_.clear();
    interactionPrimitiveIndex_ = -1;
    interactionHandleIndex_ = -1;
    interactionHandleKind_ = PrimitiveHandleKind::None;
}

void EditorApplication::TogglePageReticleSelection(const int pageIndex, const int reticleIndex)
{
    if (pageIndex < 0 || pageIndex >= static_cast<int>(loaded_.document.pages.size()))
    {
        return;
    }

    mfd::PageDefinition& page = loaded_.document.pages[static_cast<std::size_t>(pageIndex)];
    if (reticleIndex < 0 || reticleIndex >= static_cast<int>(page.staticReticles.size()) ||
        !IsPageReticleSelectableInCurrentFocus(page, page.staticReticles[static_cast<std::size_t>(reticleIndex)]))
    {
        return;
    }

    if (selection_.kind != SelectionKind::PageReticle || selection_.pageIndex != pageIndex)
    {
        SelectPageReticle(pageIndex, reticleIndex);
        return;
    }

    auto& indices = selection_.pageReticleIndices;
    auto iterator = std::find(indices.begin(), indices.end(), reticleIndex);
    if (iterator == indices.end())
    {
        indices.push_back(reticleIndex);
        std::sort(indices.begin(), indices.end());
        selection_.pageReticleIndex = reticleIndex;
        return;
    }

    if (indices.size() == 1U)
    {
        SelectPage(pageIndex);
        return;
    }

    indices.erase(iterator);
    if (selection_.pageReticleIndex == reticleIndex)
    {
        selection_.pageReticleIndex = indices.back();
    }
}

void EditorApplication::SelectLibraryReticle(std::string templateId)
{
    selection_.kind = SelectionKind::LibraryReticle;
    selection_.libraryReticleId = std::move(templateId);
    selection_.libraryBrowserReticleId = selection_.libraryReticleId;
    selection_.pageReticleIndex = -1;
    selection_.pageReticleIndices.clear();
    selection_.primitiveIndex = -1;
    interactionMode_ = InteractionMode::None;
    interactionReticleIndex_ = -1;
    interactionReticleIndices_.clear();
    interactionStartReticleTransforms_.clear();
    interactionPrimitiveIndex_ = -1;
    interactionHandleIndex_ = -1;
    interactionHandleKind_ = PrimitiveHandleKind::None;
    ResetLibraryPreviewView();
}

void EditorApplication::SelectLibraryPrimitive(std::string templateId, const int primitiveIndex)
{
    selection_.kind = SelectionKind::LibraryPrimitive;
    selection_.libraryReticleId = std::move(templateId);
    selection_.libraryBrowserReticleId = selection_.libraryReticleId;
    selection_.pageReticleIndex = -1;
    selection_.pageReticleIndices.clear();
    selection_.primitiveIndex = primitiveIndex;
    interactionMode_ = InteractionMode::None;
    interactionReticleIndex_ = -1;
    interactionReticleIndices_.clear();
    interactionStartReticleTransforms_.clear();
    interactionPrimitiveIndex_ = -1;
    interactionHandleIndex_ = -1;
    interactionHandleKind_ = PrimitiveHandleKind::None;
}

mfd::PageDefinition* EditorApplication::ActivePage() noexcept
{
    if (selection_.pageIndex < 0 || selection_.pageIndex >= static_cast<int>(loaded_.document.pages.size()))
    {
        return nullptr;
    }

    return &loaded_.document.pages[static_cast<std::size_t>(selection_.pageIndex)];
}

const mfd::PageDefinition* EditorApplication::ActivePage() const noexcept
{
    if (selection_.pageIndex < 0 || selection_.pageIndex >= static_cast<int>(loaded_.document.pages.size()))
    {
        return nullptr;
    }

    return &loaded_.document.pages[static_cast<std::size_t>(selection_.pageIndex)];
}

mfd::ReticleGroup* EditorApplication::SelectedPageReticle() noexcept
{
    mfd::PageDefinition* page = ActivePage();
    if (page == nullptr ||
        selection_.pageReticleIndex < 0 ||
        selection_.pageReticleIndex >= static_cast<int>(page->staticReticles.size()))
    {
        return nullptr;
    }

    return &page->staticReticles[static_cast<std::size_t>(selection_.pageReticleIndex)];
}

const mfd::ReticleGroup* EditorApplication::SelectedPageReticle() const noexcept
{
    const mfd::PageDefinition* page = ActivePage();
    if (page == nullptr ||
        selection_.pageReticleIndex < 0 ||
        selection_.pageReticleIndex >= static_cast<int>(page->staticReticles.size()))
    {
        return nullptr;
    }

    return &page->staticReticles[static_cast<std::size_t>(selection_.pageReticleIndex)];
}

bool EditorApplication::HasSelectedPageReticle(const int pageIndex, const int reticleIndex) const noexcept
{
    if (selection_.kind != SelectionKind::PageReticle || selection_.pageIndex != pageIndex)
    {
        return false;
    }

    const auto& indices = selection_.pageReticleIndices;
    if (indices.empty())
    {
        return selection_.pageReticleIndex == reticleIndex;
    }

    return std::find(indices.begin(), indices.end(), reticleIndex) != indices.end();
}

std::vector<int> EditorApplication::SelectedPageReticleIndices() const
{
    std::vector<int> indices;
    if (selection_.kind != SelectionKind::PageReticle)
    {
        return indices;
    }

    const mfd::PageDefinition* page = ActivePage();
    if (page == nullptr)
    {
        return indices;
    }

    if (!selection_.pageReticleIndices.empty())
    {
        indices = selection_.pageReticleIndices;
    }
    else if (selection_.pageReticleIndex >= 0)
    {
        indices.push_back(selection_.pageReticleIndex);
    }

    indices.erase(
        std::remove_if(indices.begin(),
                       indices.end(),
                       [page](const int index)
                       {
                           return index < 0 || index >= static_cast<int>(page->staticReticles.size());
                       }),
        indices.end());
    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
    return indices;
}

int EditorApplication::SelectedPageReticleCount() const
{
    return static_cast<int>(SelectedPageReticleIndices().size());
}

bool EditorApplication::HasOpenWindow() const noexcept
{
    return !loaded_.window.sourceFile.empty();
}

void EditorApplication::CopySelectedPageReticles()
{
    mfd::PageDefinition* page = ActivePage();
    const std::vector<int> selectedIndices = SelectedPageReticleIndices();
    if (page == nullptr || selectedIndices.empty())
    {
        RebuildStatus("Select one or more page reticles to copy them.", true);
        return;
    }

    pageReticleClipboard_.clear();
    pageReticleClipboard_.reserve(selectedIndices.size());
    for (const int reticleIndex : selectedIndices)
    {
        pageReticleClipboard_.push_back(page->staticReticles[static_cast<std::size_t>(reticleIndex)]);
    }

    pageReticlePasteSerial_ = 0;
    if (pageReticleClipboard_.size() == 1U)
    {
        RebuildStatus("Reticle '" + pageReticleClipboard_.front().id + "' copied from page '" + page->name + "'.", false);
    }
    else
    {
        RebuildStatus(
            std::to_string(pageReticleClipboard_.size()) + " reticles copied from page '" + page->name + "'.",
            false);
    }
}

void EditorApplication::CutSelectedPageReticles()
{
    mfd::PageDefinition* page = ActivePage();
    const std::vector<int> selectedIndices = SelectedPageReticleIndices();
    if (page == nullptr || selectedIndices.empty())
    {
        RebuildStatus("Select one or more page reticles to cut them.", true);
        return;
    }

    pageReticleClipboard_.clear();
    pageReticleClipboard_.reserve(selectedIndices.size());
    for (const int reticleIndex : selectedIndices)
    {
        pageReticleClipboard_.push_back(page->staticReticles[static_cast<std::size_t>(reticleIndex)]);
    }
    pageReticlePasteSerial_ = 0;

    PushUndoSnapshot();
    std::vector<int> descendingIndices = selectedIndices;
    std::sort(descendingIndices.begin(), descendingIndices.end(), std::greater<int>());
    for (const int reticleIndex : descendingIndices)
    {
        if (reticleIndex < 0 || reticleIndex >= static_cast<int>(page->staticReticles.size()))
        {
            continue;
        }

        page->staticReticles.erase(page->staticReticles.begin() + reticleIndex);
    }

    SelectPage(selection_.pageIndex);
    if (pageReticleClipboard_.size() == 1U)
    {
        RebuildStatus("Reticle '" + pageReticleClipboard_.front().id + "' cut from page '" + page->name + "'.", false);
    }
    else
    {
        RebuildStatus(
            std::to_string(pageReticleClipboard_.size()) + " reticles cut from page '" + page->name + "'.",
            false);
    }
}

void EditorApplication::PasteCopiedPageReticles()
{
    mfd::PageDefinition* page = ActivePage();
    if (page == nullptr)
    {
        RebuildStatus("Select a page before pasting copied reticles.", true);
        return;
    }

    if (pageReticleClipboard_.empty())
    {
        RebuildStatus("No copied page reticle is available yet.", true);
        return;
    }

    PushUndoSnapshot();

    ++pageReticlePasteSerial_;
    const float offset = 0.035f * static_cast<float>(pageReticlePasteSerial_);
    std::vector<int> pastedIndices;
    pastedIndices.reserve(pageReticleClipboard_.size());

    for (const auto& sourceReticle : pageReticleClipboard_)
    {
        mfd::ReticleGroup pastedReticle = sourceReticle;
        const std::string baseId = pastedReticle.id.empty() ? std::string {"reticle"} : pastedReticle.id;
        pastedReticle.id = MakeUniqueReticleId(page->staticReticles, baseId);
        pastedReticle.transform.position.x = std::clamp(pastedReticle.transform.position.x + offset, -1.0f, 1.0f);
        pastedReticle.transform.position.y = std::clamp(pastedReticle.transform.position.y - offset, -1.0f, 1.0f);
        if (!pastedReticle.layerId.empty() && FindEditorLayer(*page, pastedReticle.layerId) == nullptr)
        {
            pastedReticle.layerId = ActiveInsertionLayerId(*page);
        }
        else if (pastedReticle.layerId.empty())
        {
            pastedReticle.layerId = ActiveInsertionLayerId(*page);
        }

        page->staticReticles.push_back(std::move(pastedReticle));
        pastedIndices.push_back(static_cast<int>(page->staticReticles.size()) - 1);
    }

    RefreshPageBlinkStateForEditor(*page);
    selection_.kind = SelectionKind::PageReticle;
    selection_.pageReticleIndices = pastedIndices;
    selection_.pageReticleIndex = pastedIndices.empty() ? -1 : pastedIndices.back();
    SanitizePageReticleSelectionForCurrentFocus();

    if (pastedIndices.size() == 1U)
    {
        const mfd::ReticleGroup& pastedReticle = page->staticReticles[static_cast<std::size_t>(pastedIndices.front())];
        RebuildStatus("Reticle '" + pastedReticle.id + "' pasted on page '" + page->name + "'.", false);
    }
    else
    {
        RebuildStatus(
            std::to_string(pastedIndices.size()) + " reticles pasted on page '" + page->name + "'.",
            false);
    }
}

mfd::ReticleGroup* EditorApplication::SelectedLibraryReticle() noexcept
{
    const auto iterator = loaded_.document.reticleLibrary.find(selection_.libraryReticleId);
    return iterator == loaded_.document.reticleLibrary.end() ? nullptr : &iterator->second;
}

const mfd::ReticleGroup* EditorApplication::SelectedLibraryReticle() const noexcept
{
    const auto iterator = loaded_.document.reticleLibrary.find(selection_.libraryReticleId);
    return iterator == loaded_.document.reticleLibrary.end() ? nullptr : &iterator->second;
}

mfd::Primitive* EditorApplication::SelectedLibraryPrimitive() noexcept
{
    mfd::ReticleGroup* reticle = SelectedLibraryReticle();
    if (reticle == nullptr ||
        selection_.primitiveIndex < 0 ||
        selection_.primitiveIndex >= static_cast<int>(reticle->primitives.size()))
    {
        return nullptr;
    }

    return &reticle->primitives[static_cast<std::size_t>(selection_.primitiveIndex)];
}

const mfd::Primitive* EditorApplication::SelectedLibraryPrimitive() const noexcept
{
    const mfd::ReticleGroup* reticle = SelectedLibraryReticle();
    if (reticle == nullptr ||
        selection_.primitiveIndex < 0 ||
        selection_.primitiveIndex >= static_cast<int>(reticle->primitives.size()))
    {
        return nullptr;
    }

    return &reticle->primitives[static_cast<std::size_t>(selection_.primitiveIndex)];
}

void EditorApplication::DrawReticleHoverPreviewTooltip(const mfd::ReticleGroup& reticle,
                                                       const std::string_view label,
                                                       const Color backgroundColor)
{
    if (!ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_NoSharedDelay))
    {
        return;
    }

    constexpr int kTooltipPreviewWidth = 220;
    constexpr int kTooltipPreviewHeight = 180;

    EnsureTooltipPreviewTexture(kTooltipPreviewWidth, kTooltipPreviewHeight);
    if (!tooltipPreviewTextureReady_)
    {
        return;
    }

    mfd::ReticleGroup previewReticle = reticle;
    previewReticle.visible = true;
    const mfd::PageViewState previewView =
        MakeViewFittingBounds(ComputeReticleWorldBounds(previewReticle), kTooltipPreviewWidth, kTooltipPreviewHeight);

    BeginTextureMode(tooltipPreviewTexture_);
    ClearBackground(backgroundColor);
    {
        EnsurePreviewFont();
        ApplyBilinearFilterToFont(PreviewTextFont() == nullptr ? GetFontDefault() : *PreviewTextFont());
        mfd::Canvas2D canvas(kTooltipPreviewWidth,
                             kTooltipPreviewHeight,
                             previewView,
                             PreviewTextFont(),
                             backgroundColor,
                             tooltipPreviewTextureStencilReady_,
                             &previewBezierCache_,
                             &previewImageCache_,
                             &previewTextLayoutCache_);
        canvas.DrawReticle(previewReticle);
    }
    EndTextureMode();

    ImGui::BeginTooltip();
    if (!label.empty())
    {
        ImGui::TextUnformatted(label.data(), label.data() + label.size());
        ImGui::Separator();
    }
    ImGui::Image(
        (ImTextureID)(uintptr_t)tooltipPreviewTexture_.texture.id,
        ImVec2(static_cast<float>(kTooltipPreviewWidth), static_cast<float>(kTooltipPreviewHeight)),
        ImVec2(0.0f, 1.0f),
        ImVec2(1.0f, 0.0f));
    ImGui::TextDisabled("Hover preview");
    ImGui::EndTooltip();
}

EditorApplication::ReticleScreenBounds EditorApplication::ComputePrimitiveScreenBounds(
    const mfd::ReticleGroup& reticle,
    const mfd::Primitive& primitive,
    const ViewportState& viewport) const
{
    ReticleScreenBounds bounds;

    auto includeLogicalPoint = [&](const mfd::Vec2 logicalPoint)
    {
        const ImVec2 screenPoint = viewport.ToScreen(logicalPoint);
        if (!bounds.valid)
        {
            bounds.min = screenPoint;
            bounds.max = screenPoint;
            bounds.valid = true;
            return;
        }

        bounds.min.x = std::min(bounds.min.x, screenPoint.x);
        bounds.min.y = std::min(bounds.min.y, screenPoint.y);
        bounds.max.x = std::max(bounds.max.x, screenPoint.x);
        bounds.max.y = std::max(bounds.max.y, screenPoint.y);
    };

    auto includeTransformedPoint = [&](const mfd::Vec2 localPoint)
    {
        includeLogicalPoint(mfd::ApplyTransform(mfd::ApplyTransform(localPoint, primitive.transform), reticle.transform));
    };

    if (!primitive.style.visible)
    {
        return bounds;
    }

    if (const auto* text = std::get_if<mfd::TextGeometry>(&primitive.geometry))
    {
        const float halfWidth = EstimatedTextHalfWidth(*text);
        const float halfHeight = EstimatedTextHalfHeight(*text);
        includeTransformedPoint({-halfWidth, -halfHeight});
        includeTransformedPoint({halfWidth, -halfHeight});
        includeTransformedPoint({halfWidth, halfHeight});
        includeTransformedPoint({-halfWidth, halfHeight});
    }
    else if (const auto* time = std::get_if<mfd::TimeGeometry>(&primitive.geometry))
    {
        const float halfWidth = EstimatedTextHalfWidth(*time);
        const float halfHeight = EstimatedTextHalfHeight(*time);
        includeTransformedPoint({-halfWidth, -halfHeight});
        includeTransformedPoint({halfWidth, -halfHeight});
        includeTransformedPoint({halfWidth, halfHeight});
        includeTransformedPoint({-halfWidth, halfHeight});
    }
    else if (const auto* line = std::get_if<mfd::LineGeometry>(&primitive.geometry))
    {
        includeTransformedPoint(line->start);
        includeTransformedPoint(line->end);
    }
    else if (const auto* circle = std::get_if<mfd::CircleGeometry>(&primitive.geometry))
    {
        includeTransformedPoint({-circle->radius, -circle->radius});
        includeTransformedPoint({circle->radius, -circle->radius});
        includeTransformedPoint({circle->radius, circle->radius});
        includeTransformedPoint({-circle->radius, circle->radius});
    }
    else if (const auto* ring = std::get_if<mfd::RingGeometry>(&primitive.geometry))
    {
        includeTransformedPoint({-ring->outerRadius, -ring->outerRadius});
        includeTransformedPoint({ring->outerRadius, -ring->outerRadius});
        includeTransformedPoint({ring->outerRadius, ring->outerRadius});
        includeTransformedPoint({-ring->outerRadius, ring->outerRadius});
    }
    else if (const auto* rectangle = std::get_if<mfd::RectangleGeometry>(&primitive.geometry))
    {
        includeTransformedPoint({-rectangle->width * 0.5f, -rectangle->height * 0.5f});
        includeTransformedPoint({rectangle->width * 0.5f, -rectangle->height * 0.5f});
        includeTransformedPoint({rectangle->width * 0.5f, rectangle->height * 0.5f});
        includeTransformedPoint({-rectangle->width * 0.5f, rectangle->height * 0.5f});
    }
    else if (const auto* ellipse = std::get_if<mfd::EllipseGeometry>(&primitive.geometry))
    {
        includeTransformedPoint({-ellipse->width * 0.5f, -ellipse->height * 0.5f});
        includeTransformedPoint({ellipse->width * 0.5f, -ellipse->height * 0.5f});
        includeTransformedPoint({ellipse->width * 0.5f, ellipse->height * 0.5f});
        includeTransformedPoint({-ellipse->width * 0.5f, ellipse->height * 0.5f});
    }
    else if (const auto* square = std::get_if<mfd::SquareGeometry>(&primitive.geometry))
    {
        includeTransformedPoint({-square->width * 0.5f, -square->height * 0.5f});
        includeTransformedPoint({square->width * 0.5f, -square->height * 0.5f});
        includeTransformedPoint({square->width * 0.5f, square->height * 0.5f});
        includeTransformedPoint({-square->width * 0.5f, square->height * 0.5f});
    }
    else if (const auto* diamond = std::get_if<mfd::DiamondGeometry>(&primitive.geometry))
    {
        includeTransformedPoint({0.0f, diamond->height * 0.5f});
        includeTransformedPoint({diamond->width * 0.5f, 0.0f});
        includeTransformedPoint({0.0f, -diamond->height * 0.5f});
        includeTransformedPoint({-diamond->width * 0.5f, 0.0f});
    }
    else if (const auto* triangle = std::get_if<mfd::TriangleGeometry>(&primitive.geometry))
    {
        includeTransformedPoint(triangle->points[0]);
        includeTransformedPoint(triangle->points[1]);
        includeTransformedPoint(triangle->points[2]);
    }
    else if (const auto* polyline = std::get_if<mfd::PolylineGeometry>(&primitive.geometry))
    {
        for (const auto& point : polyline->points)
        {
            includeTransformedPoint(point);
        }
    }
    else if (const auto* bezier = std::get_if<mfd::BezierGeometry>(&primitive.geometry))
    {
        for (const auto& point : bezier->controlPoints)
        {
            includeTransformedPoint(point);
        }
    }
    else if (const auto* arc = std::get_if<mfd::ArcGeometry>(&primitive.geometry))
    {
        for (const auto& point :
             ApproximateArcPoints(arc->radius, arc->startAngleDegrees, arc->endAngleDegrees, arc->segments))
        {
            includeTransformedPoint(point);
        }

        if (primitive.style.filled)
        {
            includeTransformedPoint({});
        }
    }

    if (bounds.valid)
    {
        bounds.center = ImVec2((bounds.min.x + bounds.max.x) * 0.5f, (bounds.min.y + bounds.max.y) * 0.5f);
    }

    return bounds;
}

EditorApplication::ReticleScreenBounds EditorApplication::ComputeReticleScreenBounds(
    const mfd::ReticleGroup& reticle,
    const ViewportState& viewport) const
{
    ReticleScreenBounds bounds;

    for (const auto& primitive : reticle.primitives)
    {
        const ReticleScreenBounds primitiveBounds = ComputePrimitiveScreenBounds(reticle, primitive, viewport);
        if (!primitiveBounds.valid)
        {
            continue;
        }

        if (!bounds.valid)
        {
            bounds = primitiveBounds;
            continue;
        }

        bounds.min.x = std::min(bounds.min.x, primitiveBounds.min.x);
        bounds.min.y = std::min(bounds.min.y, primitiveBounds.min.y);
        bounds.max.x = std::max(bounds.max.x, primitiveBounds.max.x);
        bounds.max.y = std::max(bounds.max.y, primitiveBounds.max.y);
    }

    if (bounds.valid)
    {
        bounds.center = ImVec2((bounds.min.x + bounds.max.x) * 0.5f, (bounds.min.y + bounds.max.y) * 0.5f);
    }

    return bounds;
}

float EditorApplication::PrimitiveHitDistancePixels(const mfd::ReticleGroup& reticle,
                                                    const mfd::Primitive& primitive,
                                                    const ViewportState& viewport,
                                                    const ImVec2 mousePosition) const
{
    const ReticleScreenBounds bounds = ComputePrimitiveScreenBounds(reticle, primitive, viewport);
    if (!bounds.valid)
    {
        return std::numeric_limits<float>::max();
    }

    const auto distanceToPoint = [&](const ImVec2 point)
    {
        const float dx = point.x - mousePosition.x;
        const float dy = point.y - mousePosition.y;
        return std::sqrt(dx * dx + dy * dy);
    };

    auto distanceToSegment = [&](const ImVec2 a, const ImVec2 b)
    {
        const float abx = b.x - a.x;
        const float aby = b.y - a.y;
        const float apx = mousePosition.x - a.x;
        const float apy = mousePosition.y - a.y;
        const float lengthSquared = abx * abx + aby * aby;
        if (lengthSquared <= 0.0001f)
        {
            return distanceToPoint(a);
        }

        const float factor = std::clamp((apx * abx + apy * aby) / lengthSquared, 0.0f, 1.0f);
        const ImVec2 projection(a.x + abx * factor, a.y + aby * factor);
        return distanceToPoint(projection);
    };

    float bestDistance = distanceToPoint(bounds.center);

    auto toScreenPoint = [&](const mfd::Vec2 localPoint)
    {
        return viewport.ToScreen(mfd::ApplyTransform(mfd::ApplyTransform(localPoint, primitive.transform), reticle.transform));
    };

    if (const auto* text = std::get_if<mfd::TextGeometry>(&primitive.geometry))
    {
        const float halfWidth = EstimatedTextHalfWidth(*text);
        const float halfHeight = EstimatedTextHalfHeight(*text);
        const ImVec2 tl = toScreenPoint({-halfWidth, -halfHeight});
        const ImVec2 br = toScreenPoint({halfWidth, halfHeight});
        const float minX = std::min(tl.x, br.x);
        const float maxX = std::max(tl.x, br.x);
        const float minY = std::min(tl.y, br.y);
        const float maxY = std::max(tl.y, br.y);
        if (mousePosition.x >= minX - 6.0f && mousePosition.x <= maxX + 6.0f &&
            mousePosition.y >= minY - 6.0f && mousePosition.y <= maxY + 6.0f)
        {
            bestDistance = std::min(bestDistance, 2.0f);
        }
        return bestDistance;
    }

    if (const auto* time = std::get_if<mfd::TimeGeometry>(&primitive.geometry))
    {
        const float halfWidth = EstimatedTextHalfWidth(*time);
        const float halfHeight = EstimatedTextHalfHeight(*time);
        const ImVec2 tl = toScreenPoint({-halfWidth, -halfHeight});
        const ImVec2 br = toScreenPoint({halfWidth, halfHeight});
        const float minX = std::min(tl.x, br.x);
        const float maxX = std::max(tl.x, br.x);
        const float minY = std::min(tl.y, br.y);
        const float maxY = std::max(tl.y, br.y);
        if (mousePosition.x >= minX - 6.0f && mousePosition.x <= maxX + 6.0f &&
            mousePosition.y >= minY - 6.0f && mousePosition.y <= maxY + 6.0f)
        {
            bestDistance = std::min(bestDistance, 2.0f);
        }
        return bestDistance;
    }

    if (const auto* line = std::get_if<mfd::LineGeometry>(&primitive.geometry))
    {
        return std::min(bestDistance, distanceToSegment(toScreenPoint(line->start), toScreenPoint(line->end)));
    }

    if (const auto* image = std::get_if<mfd::ImageGeometry>(&primitive.geometry))
    {
        const std::array<ImVec2, 4> imageCorners {
            toScreenPoint({-image->width * 0.5f, -image->height * 0.5f}),
            toScreenPoint({image->width * 0.5f, -image->height * 0.5f}),
            toScreenPoint({image->width * 0.5f, image->height * 0.5f}),
            toScreenPoint({-image->width * 0.5f, image->height * 0.5f})};
        std::vector<ImVec2> polygon(imageCorners.begin(), imageCorners.end());
        if (IsPointInsidePolygon(polygon, mousePosition))
        {
            return 1.0f;
        }

        for (std::size_t index = 0; index < imageCorners.size(); ++index)
        {
            const std::size_t nextIndex = (index + 1U) % imageCorners.size();
            bestDistance = std::min(bestDistance, distanceToSegment(imageCorners[index], imageCorners[nextIndex]));
        }

        return bestDistance;
    }

    if (const auto* circle = std::get_if<mfd::CircleGeometry>(&primitive.geometry))
    {
        const ImVec2 center = toScreenPoint({});
        const ImVec2 edge = toScreenPoint({circle->radius, 0.0f});
        const float radiusPixels = std::max(1.0f, Distance(center, edge));
        const float centerDistance = distanceToPoint(center);
        bestDistance = std::min(bestDistance, std::abs(centerDistance - radiusPixels));
        if (centerDistance <= radiusPixels + 6.0f)
        {
            bestDistance = std::min(bestDistance, 3.0f);
        }
        return bestDistance;
    }

    if (const auto* ring = std::get_if<mfd::RingGeometry>(&primitive.geometry))
    {
        const ImVec2 center = toScreenPoint({});
        const ImVec2 innerEdge = toScreenPoint({ring->innerRadius, 0.0f});
        const ImVec2 outerEdge = toScreenPoint({ring->outerRadius, 0.0f});
        float innerRadiusPixels = Distance(center, innerEdge);
        float outerRadiusPixels = std::max(1.0f, Distance(center, outerEdge));
        if (innerRadiusPixels > outerRadiusPixels)
        {
            std::swap(innerRadiusPixels, outerRadiusPixels);
        }

        const float centerDistance = distanceToPoint(center);
        float ringDistance = 0.0f;
        if (centerDistance < innerRadiusPixels)
        {
            ringDistance = innerRadiusPixels - centerDistance;
        }
        else if (centerDistance > outerRadiusPixels)
        {
            ringDistance = centerDistance - outerRadiusPixels;
        }
        else
        {
            ringDistance = std::min(centerDistance - innerRadiusPixels, outerRadiusPixels - centerDistance);
        }

        if (centerDistance >= innerRadiusPixels - 6.0f && centerDistance <= outerRadiusPixels + 6.0f)
        {
            ringDistance = std::min(ringDistance, 2.0f);
        }

        return ringDistance;
    }

    if (const auto* arc = std::get_if<mfd::ArcGeometry>(&primitive.geometry))
    {
        const std::vector<mfd::Vec2> logicalArcPoints =
            ApproximateArcPoints(arc->radius, arc->startAngleDegrees, arc->endAngleDegrees, arc->segments);
        std::vector<ImVec2> arcPoints;
        arcPoints.reserve(logicalArcPoints.size());
        for (const auto& point : logicalArcPoints)
        {
            arcPoints.push_back(toScreenPoint(point));
        }

        for (std::size_t index = 0; index + 1U < arcPoints.size(); ++index)
        {
            bestDistance = std::min(bestDistance, distanceToSegment(arcPoints[index], arcPoints[index + 1U]));
        }

        if (primitive.style.filled && arcPoints.size() >= 2U)
        {
            std::vector<ImVec2> sectorPoints;
            sectorPoints.reserve(arcPoints.size() + 1U);
            sectorPoints.push_back(toScreenPoint({}));
            sectorPoints.insert(sectorPoints.end(), arcPoints.begin(), arcPoints.end());
            for (std::size_t index = 0; index + 1U < sectorPoints.size(); ++index)
            {
                bestDistance = std::min(bestDistance, distanceToSegment(sectorPoints[index], sectorPoints[index + 1U]));
            }
            bestDistance = std::min(bestDistance, distanceToSegment(sectorPoints.back(), sectorPoints.front()));
            if (IsPointInsidePolygon(sectorPoints, mousePosition))
            {
                bestDistance = std::min(bestDistance, 2.0f);
            }
        }

        return bestDistance;
    }

    std::vector<ImVec2> points;

    if (const auto* rectangle = std::get_if<mfd::RectangleGeometry>(&primitive.geometry))
    {
        points = {
            toScreenPoint({-rectangle->width * 0.5f, -rectangle->height * 0.5f}),
            toScreenPoint({rectangle->width * 0.5f, -rectangle->height * 0.5f}),
            toScreenPoint({rectangle->width * 0.5f, rectangle->height * 0.5f}),
            toScreenPoint({-rectangle->width * 0.5f, rectangle->height * 0.5f})};
    }
    else if (const auto* ellipse = std::get_if<mfd::EllipseGeometry>(&primitive.geometry))
    {
        for (const auto& point : ApproximateEllipsePoints(ellipse->width, ellipse->height))
        {
            points.push_back(toScreenPoint(point));
        }
    }
    else if (const auto* square = std::get_if<mfd::SquareGeometry>(&primitive.geometry))
    {
        points = {
            toScreenPoint({-square->width * 0.5f, -square->height * 0.5f}),
            toScreenPoint({square->width * 0.5f, -square->height * 0.5f}),
            toScreenPoint({square->width * 0.5f, square->height * 0.5f}),
            toScreenPoint({-square->width * 0.5f, square->height * 0.5f})};
    }
    else if (const auto* diamond = std::get_if<mfd::DiamondGeometry>(&primitive.geometry))
    {
        points = {
            toScreenPoint({0.0f, diamond->height * 0.5f}),
            toScreenPoint({diamond->width * 0.5f, 0.0f}),
            toScreenPoint({0.0f, -diamond->height * 0.5f}),
            toScreenPoint({-diamond->width * 0.5f, 0.0f})};
    }
    else if (const auto* triangle = std::get_if<mfd::TriangleGeometry>(&primitive.geometry))
    {
        points = {
            toScreenPoint(triangle->points[0]),
            toScreenPoint(triangle->points[1]),
            toScreenPoint(triangle->points[2])};
    }
    else if (const auto* polyline = std::get_if<mfd::PolylineGeometry>(&primitive.geometry))
    {
        for (const auto& point : polyline->points)
        {
            points.push_back(toScreenPoint(point));
        }

        for (std::size_t index = 0; index + 1 < points.size(); ++index)
        {
            bestDistance = std::min(bestDistance, distanceToSegment(points[index], points[index + 1]));
        }

        if (polyline->closed && points.size() > 2)
        {
            bestDistance = std::min(bestDistance, distanceToSegment(points.back(), points.front()));
        }

        return bestDistance;
    }
    else if (const auto* bezier = std::get_if<mfd::BezierGeometry>(&primitive.geometry))
    {
        for (const auto& point : bezier->controlPoints)
        {
            points.push_back(toScreenPoint(point));
        }
    }
    else if (const auto* image = std::get_if<mfd::ImageGeometry>(&primitive.geometry))
    {
        points = {
            toScreenPoint({-image->width * 0.5f, -image->height * 0.5f}),
            toScreenPoint({image->width * 0.5f, -image->height * 0.5f}),
            toScreenPoint({image->width * 0.5f, image->height * 0.5f}),
            toScreenPoint({-image->width * 0.5f, image->height * 0.5f})};
    }

    for (std::size_t index = 0; index < points.size(); ++index)
    {
        bestDistance = std::min(bestDistance, distanceToPoint(points[index]));
        if (index + 1 < points.size())
        {
            bestDistance = std::min(bestDistance, distanceToSegment(points[index], points[index + 1]));
        }
    }

    if (points.size() > 2)
    {
        bestDistance = std::min(bestDistance, distanceToSegment(points.back(), points.front()));
        if (IsPointInsidePolygon(points, mousePosition))
        {
            bestDistance = std::min(bestDistance, 2.0f);
        }
    }

    return bestDistance;
}

float EditorApplication::ReticleHitDistancePixels(const mfd::ReticleGroup& reticle,
                                                  const ViewportState& viewport,
                                                  const ImVec2 mousePosition) const
{
    const ReticleScreenBounds bounds = ComputeReticleScreenBounds(reticle, viewport);
    if (!bounds.valid)
    {
        return std::numeric_limits<float>::max();
    }

    float bestDistance = Distance(bounds.center, mousePosition);
    for (const auto& primitive : reticle.primitives)
    {
        bestDistance = std::min(bestDistance, PrimitiveHitDistancePixels(reticle, primitive, viewport, mousePosition));
    }

    return bestDistance;
}

void EditorApplication::UpdateReticleSelectionFromClick(const ViewportState& viewport, const bool additiveSelection)
{
    if (const auto reticleIndex = FindNearestPageReticle(viewport, ImGui::GetMousePos()); reticleIndex.has_value())
    {
        if (additiveSelection)
        {
            TogglePageReticleSelection(selection_.pageIndex, *reticleIndex);
        }
        else
        {
            SelectPageReticle(selection_.pageIndex, *reticleIndex);
        }
    }
    else if (!additiveSelection)
    {
        SelectPage(selection_.pageIndex);
    }
}

std::vector<int> EditorApplication::CollectPageReticlesAt(const ViewportState& viewport, const ImVec2 mousePosition) const
{
    const mfd::PageDefinition* page = ActivePage();
    if (page == nullptr)
    {
        return {};
    }

    std::vector<PageReticleHit> hits;
    for (int reticleIndex = 0; reticleIndex < static_cast<int>(page->staticReticles.size()); ++reticleIndex)
    {
        const auto& reticle = page->staticReticles[static_cast<std::size_t>(reticleIndex)];
        if (!IsReticleVisibleInEditor(*page, reticle) || !IsPageReticleSelectableInCurrentFocus(*page, reticle))
        {
            continue;
        }

        const ReticleScreenBounds bounds = ComputeReticleScreenBounds(reticle, viewport);
        if (!bounds.valid)
        {
            continue;
        }

        float distance = ReticleHitDistancePixels(reticle, viewport, mousePosition);
        const bool mouseInsideBounds =
            mousePosition.x >= bounds.min.x - 8.0f && mousePosition.x <= bounds.max.x + 8.0f &&
            mousePosition.y >= bounds.min.y - 8.0f && mousePosition.y <= bounds.max.y + 8.0f;
        const bool directHit = distance <= 6.0f;
        if (mouseInsideBounds)
        {
            distance = std::min(distance, 4.0f);
        }

        if (!mouseInsideBounds && !directHit && distance > 36.0f)
        {
            continue;
        }

        const float area = std::max(1.0f, (bounds.max.x - bounds.min.x) * (bounds.max.y - bounds.min.y));
        hits.push_back(PageReticleHit {reticleIndex, distance, area, directHit, mouseInsideBounds, reticle.drawOnTop ? 1 : 0});
    }

    std::sort(hits.begin(),
              hits.end(),
              [](const PageReticleHit& lhs, const PageReticleHit& rhs)
              {
                  if (lhs.directHit != rhs.directHit)
                  {
                      return lhs.directHit && !rhs.directHit;
                  }
                  if (lhs.boundsHit != rhs.boundsHit)
                  {
                      return lhs.boundsHit && !rhs.boundsHit;
                  }
                  if (lhs.drawPriority != rhs.drawPriority)
                  {
                      return lhs.drawPriority > rhs.drawPriority;
                  }
                  if (std::abs(lhs.distance - rhs.distance) > 0.25f)
                  {
                      return lhs.distance < rhs.distance;
                  }
                  if (std::abs(lhs.area - rhs.area) > 0.5f)
                  {
                      return lhs.area < rhs.area;
                  }
                  return lhs.reticleIndex > rhs.reticleIndex;
              });

    std::vector<int> reticleIndices;
    reticleIndices.reserve(hits.size());
    for (const PageReticleHit& hit : hits)
    {
        reticleIndices.push_back(hit.reticleIndex);
    }
    return reticleIndices;
}

std::optional<int> EditorApplication::FindNearestPageReticle(const ViewportState& viewport, const ImVec2 mousePosition) const
{
    const std::vector<int> hits = CollectPageReticlesAt(viewport, mousePosition);
    return hits.empty() ? std::nullopt : std::optional<int> {hits.front()};
}

std::vector<EditorApplication::PageClipTarget> EditorApplication::CollectPageClipTargetsAt(
    const ViewportState& viewport,
    const ImVec2 mousePosition) const
{
    const mfd::PageDefinition* page = ActivePage();
    if (page == nullptr)
    {
        return {};
    }

    std::vector<PageClipPrimitiveHit> hits;
    for (int reticleIndex = 0; reticleIndex < static_cast<int>(page->staticReticles.size()); ++reticleIndex)
    {
        const auto& reticle = page->staticReticles[static_cast<std::size_t>(reticleIndex)];
        if (!IsReticleVisibleInEditor(*page, reticle))
        {
            continue;
        }

        const float reticleDistance = ReticleHitDistancePixels(reticle, viewport, mousePosition);
        for (int primitiveIndex = 0; primitiveIndex < static_cast<int>(reticle.primitives.size()); ++primitiveIndex)
        {
            const auto& primitive = reticle.primitives[static_cast<std::size_t>(primitiveIndex)];
            if (primitive.id.empty() || !mfd::SupportsReticleClipPrimitive(primitive) || !primitive.style.visible)
            {
                continue;
            }

            const ReticleScreenBounds bounds = ComputePrimitiveScreenBounds(reticle, primitive, viewport);
            if (!bounds.valid)
            {
                continue;
            }

            float primitiveDistance = PrimitiveHitDistancePixels(reticle, primitive, viewport, mousePosition);
            const bool mouseInsideBounds =
                mousePosition.x >= bounds.min.x - 8.0f && mousePosition.x <= bounds.max.x + 8.0f &&
                mousePosition.y >= bounds.min.y - 8.0f && mousePosition.y <= bounds.max.y + 8.0f;
            if (mouseInsideBounds)
            {
                primitiveDistance = std::min(primitiveDistance, 3.0f);
            }

            if (!mouseInsideBounds && primitiveDistance > 10.0f)
            {
                continue;
            }

            hits.push_back(PageClipPrimitiveHit {
                reticleIndex,
                primitiveIndex,
                primitiveDistance,
                reticleDistance});
        }
    }

    std::sort(hits.begin(),
              hits.end(),
              [](const PageClipPrimitiveHit& lhs, const PageClipPrimitiveHit& rhs)
              {
                  if (std::abs(lhs.primitiveDistance - rhs.primitiveDistance) > 0.25f)
                  {
                      return lhs.primitiveDistance < rhs.primitiveDistance;
                  }
                  if (std::abs(lhs.reticleDistance - rhs.reticleDistance) > 0.25f)
                  {
                      return lhs.reticleDistance < rhs.reticleDistance;
                  }
                  if (lhs.reticleIndex != rhs.reticleIndex)
                  {
                      return lhs.reticleIndex > rhs.reticleIndex;
                  }
                  return lhs.primitiveIndex > rhs.primitiveIndex;
              });

    std::vector<PageClipTarget> targets;
    targets.reserve(hits.size());
    for (const PageClipPrimitiveHit& hit : hits)
    {
        targets.push_back(PageClipTarget {hit.reticleIndex, hit.primitiveIndex});
    }
    return targets;
}

std::optional<EditorApplication::PageClipTarget> EditorApplication::FindNearestPageClipPrimitive(
    const ViewportState& viewport,
    const ImVec2 mousePosition) const
{
    const std::vector<PageClipTarget> targets = CollectPageClipTargetsAt(viewport, mousePosition);
    return targets.empty() ? std::nullopt : std::optional<PageClipTarget> {targets.front()};
}

std::optional<int> EditorApplication::FindNearestLibraryPrimitive(const ViewportState& viewport,
                                                                  const ImVec2 mousePosition) const
{
    const mfd::ReticleGroup* reticle = SelectedLibraryReticle();
    if (reticle == nullptr)
    {
        return std::nullopt;
    }

    float bestDistance = 32.0f;
    float bestArea = std::numeric_limits<float>::max();
    std::optional<int> bestIndex;

    for (int primitiveIndex = 0; primitiveIndex < static_cast<int>(reticle->primitives.size()); ++primitiveIndex)
    {
        const mfd::Primitive& primitive = reticle->primitives[static_cast<std::size_t>(primitiveIndex)];
        const ReticleScreenBounds bounds = ComputePrimitiveScreenBounds(*reticle, primitive, viewport);
        if (!bounds.valid)
        {
            continue;
        }

        float distance = PrimitiveHitDistancePixels(*reticle, primitive, viewport, mousePosition);
        if (mousePosition.x >= bounds.min.x - 8.0f && mousePosition.x <= bounds.max.x + 8.0f &&
            mousePosition.y >= bounds.min.y - 8.0f && mousePosition.y <= bounds.max.y + 8.0f)
        {
            distance = std::min(distance, 3.0f);
        }

        const float area = std::max(1.0f, (bounds.max.x - bounds.min.x) * (bounds.max.y - bounds.min.y));
        if (distance < bestDistance - 0.25f ||
            (std::abs(distance - bestDistance) <= 0.25f && area < bestArea))
        {
            bestDistance = distance;
            bestArea = area;
            bestIndex = primitiveIndex;
        }
    }

    return bestIndex;
}

void EditorApplication::ApplyMouseTransform(const ViewportState& viewport)
{
    mfd::PageDefinition* page = ActivePage();
    if (interactionMode_ == InteractionMode::PanPage)
    {
        const float scale = viewport.LogicalScale();
        if (!viewport.valid || scale <= 0.0f)
        {
            interactionMode_ = InteractionMode::None;
            interactionReticleIndices_.clear();
            interactionStartReticleTransforms_.clear();
            return;
        }

        mfd::PageViewState* targetView = nullptr;
        if (selection_.kind == SelectionKind::LibraryReticle || selection_.kind == SelectionKind::LibraryPrimitive)
        {
            targetView = &libraryPreviewView_;
        }
        else
        {
            targetView = &pagePreviewView_;
        }

        const float zoom = mfd::SanitizeZoom(targetView->zoom);
        const ImVec2 mouseDelta = ImGui::GetIO().MouseDelta;
        targetView->center.x -= mouseDelta.x / (scale * zoom);
        targetView->center.y += mouseDelta.y / (scale * zoom);
        return;
    }

    if (interactionReticleIndex_ < 0)
    {
        return;
    }

    if (page == nullptr || interactionReticleIndex_ >= static_cast<int>(page->staticReticles.size()))
    {
        interactionMode_ = InteractionMode::None;
        interactionReticleIndex_ = -1;
        interactionReticleIndices_.clear();
        interactionStartReticleTransforms_.clear();
        return;
    }

    mfd::ReticleGroup& reticle = page->staticReticles[static_cast<std::size_t>(interactionReticleIndex_)];
    const mfd::Vec2 mouseLogical = viewport.ToLogical(ImGui::GetMousePos());

    switch (interactionMode_)
    {
    case InteractionMode::PanPage:
        break;

    case InteractionMode::MoveReticle:
    {
        const mfd::Vec2 translationDelta = mouseLogical - interactionStartMouseLogical_;
        if (!interactionReticleIndices_.empty() &&
            interactionReticleIndices_.size() == interactionStartReticleTransforms_.size())
        {
            // Apply one shared translation delta to the exact transform snapshot captured at drag start.
            for (std::size_t index = 0; index < interactionReticleIndices_.size(); ++index)
            {
                const int movedReticleIndex = interactionReticleIndices_[index];
                if (movedReticleIndex < 0 || movedReticleIndex >= static_cast<int>(page->staticReticles.size()))
                {
                    continue;
                }

                mfd::Transform2D nextTransform = interactionStartReticleTransforms_[index];
                nextTransform.position = nextTransform.position + translationDelta;
                page->staticReticles[static_cast<std::size_t>(movedReticleIndex)].transform = nextTransform;
            }
        }
        else
        {
            reticle.transform.position = interactionStartTransform_.position + translationDelta;
        }
        break;
    }

    case InteractionMode::RotateReticle:
    {
        const mfd::Vec2 interactionPivotLogical =
            mfd::ApplyTransform(interactionStartReticleVisualCenterLocal_, interactionStartTransform_);
        const float currentAngle =
            std::atan2(mouseLogical.y - interactionPivotLogical.y,
                       mouseLogical.x - interactionPivotLogical.x) *
            180.0f / 3.14159265f;
        const float nextRotationDegrees =
            interactionStartTransform_.rotationDegrees + (currentAngle - interactionStartAngleDegrees_);
        reticle.transform = BuildTransformKeepingLocalPointWorldPosition(
            interactionStartTransform_,
            interactionStartReticleVisualCenterLocal_,
            nextRotationDegrees,
            interactionStartTransform_.scale);
        break;
    }

    case InteractionMode::ScaleReticle:
    {
        const ImVec2 mouseScreen = ImGui::GetMousePos();
        const float startDistance = std::max(8.0f, Distance(interactionStartCornerScreen_, interactionStartCenterScreen_));
        const float currentDistance = std::max(4.0f, Distance(mouseScreen, interactionStartCenterScreen_));
        const float factor = std::clamp(currentDistance / startDistance, 0.1f, 10.0f);
        const mfd::Vec2 nextScale {
            std::max(0.05f, interactionStartTransform_.scale.x * factor),
            std::max(0.05f, interactionStartTransform_.scale.y * factor)};
        reticle.transform = BuildTransformKeepingLocalPointWorldPosition(
            interactionStartTransform_,
            interactionStartReticleVisualCenterLocal_,
            interactionStartTransform_.rotationDegrees,
            nextScale);
        break;
    }

    case InteractionMode::None:
        break;
    }
}

void EditorApplication::DrawPopups()
{
    using editor::tutorial::TutorialStepId;

    if (showNewPagePopup_)
    {
        ImGui::OpenPopup("Create new page");
        showNewPagePopup_ = false;
    }
    if (showNewWindowPopup_)
    {
        ImGui::OpenPopup("Create new window");
        showNewWindowPopup_ = false;
    }
    if (showAssetFolderPickerPopup_)
    {
        ImGui::OpenPopup("Choose asset folder");
        showAssetFolderPickerPopup_ = false;
    }

    if (showNewLibraryReticlePopup_)
    {
        ImGui::OpenPopup("Create new library reticle");
        showNewLibraryReticlePopup_ = false;
    }

    if (showDuplicateLibraryReticlePopup_)
    {
        ImGui::OpenPopup("Duplicate library reticle");
        showDuplicateLibraryReticlePopup_ = false;
    }

    if (pageImportPopup_.openRequested)
    {
        ImGui::OpenPopup("Import page");
        pageImportPopup_.openRequested = false;
    }

    if (pageRenamePopup_.openRequested)
    {
        ImGui::OpenPopup("Rename page globally");
        pageRenamePopup_.openRequested = false;
    }

    if (reticleRenamePopup_.openRequested)
    {
        ImGui::OpenPopup("Rename reticle globally");
        reticleRenamePopup_.openRequested = false;
    }

    if (reticleExtractionPopup_.openRequested)
    {
        ImGui::OpenPopup("Extract as reticle");
        reticleExtractionPopup_.openRequested = false;
    }

    if (designExportPopup_.openRequested)
    {
        ImGui::OpenPopup("Export design");
        designExportPopup_.openRequested = false;
    }

    if (pageManagementPopup_.openRequested)
    {
        ImGui::OpenPopup("Manage page");
        pageManagementPopup_.openRequested = false;
    }

    if (tutorial_->ConsumeResumePopupRequest())
    {
        ImGui::OpenPopup("Tutorial progress");
    }

    if (ImGui::BeginPopupModal("Tutorial progress", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextWrapped("A tutorial progress snapshot already exists. Continue where you stopped or restart from scratch?");
        if (AccentButton("Continue"))
        {
            tutorial_->ResumeFromSavedProgress();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Restart from scratch"))
        {
            tutorial_->RestartFromScratch();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal("Create new window", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextDisabled("Window file and runtime parameters");
        ImGui::InputText("Window file", newWindowDraft_.windowFile.data(), newWindowDraft_.windowFile.size());
        ImGui::SameLine();
        if (ImGui::Button("Browse window folder..."))
        {
            OpenAssetFolderPicker(AssetFolderPickerTarget::WindowFile);
        }
        ShowItemTooltip("Choose the folder that will receive the new window JSON. Prefer the source repo assets folder, not _Exec.");
        ImGui::InputText("Window title", newWindowDraft_.title.data(), newWindowDraft_.title.size());
        int windowSize[2] {newWindowDraft_.width, newWindowDraft_.height};
        if (ImGui::InputInt2("Size (px)", windowSize))
        {
            newWindowDraft_.width = windowSize[0];
            newWindowDraft_.height = windowSize[1];
        }
        int windowPosition[2] {newWindowDraft_.positionX, newWindowDraft_.positionY};
        if (ImGui::InputInt2("Position (px)", windowPosition))
        {
            newWindowDraft_.positionX = windowPosition[0];
            newWindowDraft_.positionY = windowPosition[1];
        }
        ImGui::InputText("Font file (optional)", newWindowDraft_.fontFile.data(), newWindowDraft_.fontFile.size());
        ImGui::InputText("Reticle library folder", newWindowDraft_.reticleLibraryFolder.data(), newWindowDraft_.reticleLibraryFolder.size());
        ImGui::SameLine();
        if (ImGui::Button("Browse reticle folder..."))
        {
            OpenAssetFolderPicker(AssetFolderPickerTarget::ReticleLibraryFolder);
        }
        ShowItemTooltip("Choose where new reticle template JSON files should be saved.");

        ImGui::SeparatorText("Commands UDP (incoming)");
        ImGui::Checkbox("Enable command UDP", &newWindowDraft_.commandUdpEnabled);
        ImGui::InputText("Command address", newWindowDraft_.commandAddress.data(), newWindowDraft_.commandAddress.size());
        ImGui::InputInt("Command port", &newWindowDraft_.commandPort);
        ImGui::InputInt("Command max packet", &newWindowDraft_.commandMaxPacketSize);

        ImGui::SeparatorText("Feedback UDP (outgoing)");
        ImGui::Checkbox("Enable feedback UDP", &newWindowDraft_.feedbackUdpEnabled);
        ImGui::InputText("Feedback address", newWindowDraft_.feedbackAddress.data(), newWindowDraft_.feedbackAddress.size());
        ImGui::InputInt("Feedback port", &newWindowDraft_.feedbackPort);
        ImGui::InputInt("Feedback max packet", &newWindowDraft_.feedbackMaxPacketSize);

        ImGui::SeparatorText("Initial content");
        ImGui::Checkbox("Create one initial page", &newWindowDraft_.createInitialPage);
        if (newWindowDraft_.createInitialPage)
        {
            ImGui::InputText("First page name", newWindowDraft_.firstPageName.data(), newWindowDraft_.firstPageName.size());
            ImGui::InputText("First page title", newWindowDraft_.firstPageTitle.data(), newWindowDraft_.firstPageTitle.size());
            ImGui::InputText("First page file", newWindowDraft_.firstPageFile.data(), newWindowDraft_.firstPageFile.size());
            ImGui::SameLine();
            if (ImGui::Button("Browse page folder..."))
            {
                OpenAssetFolderPicker(AssetFolderPickerTarget::FirstPageFile);
            }
            ShowItemTooltip("Choose the folder that will receive the first page JSON.");
            ImGui::ColorEdit4("First page background", &newWindowDraft_.firstPageBackground.x);
        }

        ImGui::TextDisabled("Use the repo source assets folders, not the staged runtime copy under _Exec.");

        if (AccentButton("Create window"))
        {
            const bool tutorialCreateMatched = tutorial_->MatchesTarget("popup_window_create");
            if (CreateNewWindow())
            {
                if (tutorialCreateMatched)
                {
                    tutorial_->CompleteStep();
                }
                ImGui::CloseCurrentPopup();
            }
        }
        ShowItemTooltip("Build a new in-memory window definition, optionally with one page, then use Save to write JSON files.");
        tutorial_->DrawHalo(
            "popup_window_create",
            "Click Create window",
            "Commit the tutorial window using the prefilled authoring values shown in this dialog.");
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            if (tutorial_->IsStep(static_cast<int>(TutorialStepId::CreateWindow)))
            {
                tutorial_->ResetPhase();
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal("Create new page", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::InputText("Page name", newPageDraft_.name.data(), newPageDraft_.name.size());
        ShowItemTooltip("Internal page id used in JSON, generated file names and API references.");
        ImGui::InputText("Title", newPageDraft_.title.data(), newPageDraft_.title.size());
        ShowItemTooltip("Optional human-readable title shown in the editor and runtime UI.");
        ImGui::InputText("File", newPageDraft_.fileName.data(), newPageDraft_.fileName.size());
        ShowItemTooltip("JSON file path written for this page. The .json extension is added automatically when missing.");
        ImGui::SameLine();
        if (ImGui::Button("Browse page folder..."))
        {
            OpenAssetFolderPicker(AssetFolderPickerTarget::NewPageFile);
        }
        ShowItemTooltip("Choose the folder that will receive the new page JSON. Prefer the source repo assets folder, not _Exec.");
        ImGui::ColorEdit4("Background", &newPageDraft_.background.x);
        ShowItemTooltip("Initial page background color.");
        ImGui::TextDisabled("Use the repo source assets folders, not the staged runtime copy under _Exec.");

        if (AccentButton("Create page"))
        {
            const bool tutorialCreateMatched = tutorial_->MatchesTarget("popup_page_create");
            if (CreateNewPage())
            {
                if (tutorialCreateMatched)
                {
                    tutorial_->CompleteStep();
                }
                ImGui::CloseCurrentPopup();
            }
        }
        ShowItemTooltip("Create the new page and add it to the current window.");
        tutorial_->DrawHalo(
            "popup_page_create",
            "Click Create page",
            "Commit the tutorial page so the walkthrough can move to the next authoring action.");
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            if (tutorial_->IsStep(static_cast<int>(TutorialStepId::CreatePage1)) ||
                tutorial_->IsStep(static_cast<int>(TutorialStepId::CreatePage2)))
            {
                tutorial_->ResetPhase();
            }
            ImGui::CloseCurrentPopup();
        }
        ShowItemTooltip("Close this dialog without creating a page.");
        ImGui::EndPopup();
    }

    DrawAssetFolderPickerPopup();
    DrawPageImportPopup();
    DrawPageRenamePopup();
    DrawReticleRenamePopup();
    DrawReticleExtractionPopup();
    DrawDesignExportPopup();
    DrawPageManagementPopup();

    if (ImGui::BeginPopupModal("Create new library reticle", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::InputText("Reticle id", newLibraryReticleDraft_.id.data(), newLibraryReticleDraft_.id.size());
        ShowItemTooltip("Template id used by the shared reticle library.");
        if (ImGui::BeginCombo("Primitive", PrimitiveTypeLabel(kPrimitiveTypes[static_cast<std::size_t>(newLibraryReticleDraft_.primitiveTypeIndex)]).c_str()))
        {
            for (int index = 0; index < static_cast<int>(kPrimitiveTypes.size()); ++index)
            {
                const bool selected = newLibraryReticleDraft_.primitiveTypeIndex == index;
                if (ImGui::Selectable(PrimitiveTypeLabel(kPrimitiveTypes[static_cast<std::size_t>(index)]).c_str(), selected))
                {
                    newLibraryReticleDraft_.primitiveTypeIndex = index;
                }
            }
            ImGui::EndCombo();
        }
        ShowItemTooltip("Choose the first primitive that will seed the new reticle template.");

        if (AccentButton("Create reticle"))
        {
            const bool tutorialCreateMatched = tutorial_->MatchesTarget("popup_reticle_create");
            if (CreateNewLibraryReticleFromPrimitive())
            {
                if (tutorialCreateMatched)
                {
                    if (tutorial_->ShouldAdvanceReticleCreatePhase())
                    {
                        tutorial_->AdvancePhase();
                    }
                    else
                    {
                        tutorial_->CompleteStep();
                    }
                }
                ImGui::CloseCurrentPopup();
            }
        }
        ShowItemTooltip("Create the new library reticle and open it in the reticle studio.");
        tutorial_->DrawHalo(
            "popup_reticle_create",
            "Click Create reticle",
            "Create the tutorial template currently prepared in this dialog.");
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            if (tutorial_->ShouldResetReticleCreatePopupOnCancel())
            {
                tutorial_->ResetPhase();
            }
            ImGui::CloseCurrentPopup();
        }
        ShowItemTooltip("Close this dialog without creating a reticle.");
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal("Duplicate library reticle", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::InputText("New reticle id", duplicateLibraryReticleDraft_.id.data(), duplicateLibraryReticleDraft_.id.size());
        ShowItemTooltip("New template id for the duplicated library reticle.");
        if (AccentButton("Duplicate"))
        {
            DuplicateSelectedLibraryReticle();
            ImGui::CloseCurrentPopup();
        }
        ShowItemTooltip("Create a full copy of the selected library reticle under the new id.");
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ShowItemTooltip("Close this dialog without duplicating the reticle.");
        ImGui::EndPopup();
    }
}

void EditorApplication::DrawAssetFolderPickerPopup()
{
    if (!ImGui::BeginPopupModal("Choose asset folder", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        return;
    }

    const auto defaultFolderForTarget = [this]() -> std::filesystem::path
    {
        switch (assetFolderPickerTarget_)
        {
        case AssetFolderPickerTarget::WindowFile:
            return DefaultProjectAssetFolder("assets/windows");
        case AssetFolderPickerTarget::ReticleLibraryFolder:
            return DefaultProjectAssetFolder("assets/reticles");
        case AssetFolderPickerTarget::FirstPageFile:
        case AssetFolderPickerTarget::NewPageFile:
            return DefaultProjectAssetFolder("assets/pages");
        case AssetFolderPickerTarget::None:
        default:
            return DefaultProjectAssetFolder("assets");
        }
    };

    if (assetFolderPickerCurrentFolder_.empty())
    {
        assetFolderPickerCurrentFolder_ = defaultFolderForTarget();
    }
    if (assetFolderPickerCurrentFolder_.is_relative())
    {
        assetFolderPickerCurrentFolder_ = std::filesystem::absolute(assetFolderPickerCurrentFolder_).lexically_normal();
    }

    ImGui::TextWrapped("Choose where the new asset should be written. Keep authored JSON files in the source assets tree, not in _Exec.");
    ImGui::Separator();
    ImGui::TextDisabled("Current folder");
    ImGui::TextWrapped("%s", assetFolderPickerCurrentFolder_.string().c_str());

    if (ImGui::Button("Project assets"))
    {
        assetFolderPickerCurrentFolder_ = DefaultProjectAssetFolder("assets");
    }
    ImGui::SameLine();
    if (ImGui::Button("Windows"))
    {
        assetFolderPickerCurrentFolder_ = DefaultProjectAssetFolder("assets/windows");
    }
    ImGui::SameLine();
    if (ImGui::Button("Pages"))
    {
        assetFolderPickerCurrentFolder_ = DefaultProjectAssetFolder("assets/pages");
    }
    ImGui::SameLine();
    if (ImGui::Button("Reticles"))
    {
        assetFolderPickerCurrentFolder_ = DefaultProjectAssetFolder("assets/reticles");
    }

    const bool canGoUp = assetFolderPickerCurrentFolder_.has_parent_path() &&
                         assetFolderPickerCurrentFolder_ != assetFolderPickerCurrentFolder_.root_path();
    ImGui::BeginDisabled(!canGoUp);
    if (ImGui::Button("Up"))
    {
        assetFolderPickerCurrentFolder_ = assetFolderPickerCurrentFolder_.parent_path();
    }
    ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::BeginChild("AssetFolderList", ImVec2(560.0f, 260.0f), true);
    std::vector<std::filesystem::path> childFolders;
    std::error_code directoryError;
    if (std::filesystem::is_directory(assetFolderPickerCurrentFolder_, directoryError))
    {
        for (const auto& entry : std::filesystem::directory_iterator(assetFolderPickerCurrentFolder_, directoryError))
        {
            if (entry.is_directory())
            {
                childFolders.push_back(entry.path());
            }
        }
    }
    std::sort(childFolders.begin(), childFolders.end());
    for (const std::filesystem::path& childFolder : childFolders)
    {
        const std::string label = childFolder.filename().string();
        if (ImGui::Selectable(label.c_str(), false))
        {
            assetFolderPickerCurrentFolder_ = childFolder.lexically_normal();
        }
    }
    if (childFolders.empty())
    {
        ImGui::TextDisabled("No subfolders in this directory.");
    }
    ImGui::EndChild();

    const bool blockedFolder = IsExecStagingPath(assetFolderPickerCurrentFolder_);
    if (blockedFolder)
    {
        ImGui::TextColored(
            ImVec4(0.95f, 0.38f, 0.38f, 1.0f),
            "This folder is inside _Exec. Choose the source assets tree instead.");
    }

    if (AccentButton("Use this folder"))
    {
        if (!blockedFolder)
        {
            switch (assetFolderPickerTarget_)
            {
            case AssetFolderPickerTarget::WindowFile:
            {
                const std::filesystem::path fileName = JsonFileNameOrFallback(
                    std::filesystem::path(newWindowDraft_.windowFile.data()),
                    "new_window.json");
                CopyTextBuffer(newWindowDraft_.windowFile, (assetFolderPickerCurrentFolder_ / fileName).lexically_normal().string());
                break;
            }

            case AssetFolderPickerTarget::ReticleLibraryFolder:
                CopyTextBuffer(newWindowDraft_.reticleLibraryFolder, assetFolderPickerCurrentFolder_.lexically_normal().string());
                break;

            case AssetFolderPickerTarget::FirstPageFile:
            {
                const std::filesystem::path fileName = JsonFileNameOrFallback(
                    std::filesystem::path(newWindowDraft_.firstPageFile.data()),
                    "page1.json");
                CopyTextBuffer(newWindowDraft_.firstPageFile, (assetFolderPickerCurrentFolder_ / fileName).lexically_normal().string());
                break;
            }

            case AssetFolderPickerTarget::NewPageFile:
            {
                const std::filesystem::path fileName = JsonFileNameOrFallback(
                    std::filesystem::path(newPageDraft_.fileName.data()),
                    "new_page.json");
                CopyTextBuffer(newPageDraft_.fileName, (assetFolderPickerCurrentFolder_ / fileName).lexically_normal().string());
                break;
            }

            case AssetFolderPickerTarget::None:
            default:
                break;
            }

            assetFolderPickerTarget_ = AssetFolderPickerTarget::None;
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
    {
        assetFolderPickerTarget_ = AssetFolderPickerTarget::None;
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void EditorApplication::DrawPageImportPopup()
{
    if (!ImGui::BeginPopupModal("Import page", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        return;
    }

    const editor::PageImportPlan plan =
        pageImportService_.BuildPlan(loaded_, files_, BuildPageImportRequest(pageImportPopup_.sourcePageFile));

    ImGui::TextColored(ImVec4(0.33f, 0.86f, 0.78f, 1.0f), "Import page asset");
    ImGui::TextWrapped("Review the staged page and reticle-template import before it is added to the current window.");
    ImGui::Separator();

    ImGui::TextDisabled("Source");
    ImGui::TextWrapped("%s", plan.sourcePageFile.empty() ? "<none>" : plan.sourcePageFile.string().c_str());

    if (!plan.sourcePageName.empty())
    {
        ImGui::TextDisabled("Page name");
        if (mfd::PageNamesEqual(plan.sourcePageName, plan.targetPageName))
        {
            ImGui::TextWrapped("%s", plan.sourcePageName.c_str());
        }
        else
        {
            ImGui::TextWrapped("%s -> %s", plan.sourcePageName.c_str(), plan.targetPageName.c_str());
        }
    }

    if (!plan.targetPageFile.empty())
    {
        ImGui::TextDisabled("Target page file");
        ImGui::TextWrapped("%s", plan.targetPageFile.string().c_str());
        ImGui::TextColored(ImportDispositionColor(plan.pageDisposition),
                           "Page policy: %s",
                           ImportDispositionLabel(plan.pageDisposition));
    }

    ImGui::SeparatorText("Reticle dependencies");
    if (plan.reticles.empty())
    {
        ImGui::TextDisabled("No reticle template dependency detected for this page.");
    }
    else
    {
        for (const editor::ReticleImportPlan& reticlePlan : plan.reticles)
        {
            ImGui::PushID(reticlePlan.sourceTemplateId.c_str());
            ImGui::TextColored(ImportDispositionColor(reticlePlan.disposition),
                               "[%s] %s",
                               ImportDispositionLabel(reticlePlan.disposition),
                               reticlePlan.sourceTemplateId.c_str());
            if (mfd::PageNamesEqual(reticlePlan.sourceTemplateId, reticlePlan.targetTemplateId))
            {
                ImGui::TextWrapped("Target: %s", reticlePlan.targetFile.string().c_str());
            }
            else
            {
                ImGui::TextWrapped("%s -> %s", reticlePlan.sourceTemplateId.c_str(), reticlePlan.targetTemplateId.c_str());
                ImGui::TextWrapped("Target: %s", reticlePlan.targetFile.string().c_str());
            }
            ImGui::PopID();
        }
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Imported assets are staged in memory first and written with File > Save.");
    ImGui::TextDisabled("Deterministic collision policy: missing target = copy, identical target = keep existing, different target = rename copy.");

    if (!plan.error.empty())
    {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.95f, 0.42f, 0.42f, 1.0f), "%s", plan.error.c_str());
    }

    ImGui::Spacing();
    ImGui::BeginDisabled(!plan.canExecute);
    if (AccentButton("Import page"))
    {
        if (ExecutePageImportPlan(plan))
        {
            pageImportPopup_ = {};
            ImGui::CloseCurrentPopup();
            ImGui::EndDisabled();
            ImGui::EndPopup();
            return;
        }
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
    {
        pageImportPopup_ = {};
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void EditorApplication::DrawPageRenamePopup()
{
    if (!ImGui::BeginPopupModal("Rename page globally", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        return;
    }

    const bool pageIndexValid = pageRenamePopup_.pageIndex >= 0 &&
                                pageRenamePopup_.pageIndex < static_cast<int>(loaded_.document.pages.size());
    if (!pageIndexValid)
    {
        ImGui::TextWrapped("The selected page is no longer available.");
        if (ImGui::Button("Close"))
        {
            pageRenamePopup_ = {};
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        return;
    }

    const mfd::PageDefinition& page = loaded_.document.pages[static_cast<std::size_t>(pageRenamePopup_.pageIndex)];

    ImGui::TextColored(ImVec4(0.33f, 0.86f, 0.78f, 1.0f), "Rename page globally");
    ImGui::TextWrapped("Review every shared window reference before renaming this page asset across the current scanned asset tree.");
    ImGui::Separator();

    ImGui::TextDisabled("Old name");
    ImGui::TextWrapped("%s", page.name.c_str());

    ImGui::InputText("New name", pageRenamePopup_.newName.data(), pageRenamePopup_.newName.size());
    ShowItemTooltip("Use the safe rename workflow to update the page JSON and every scanned window defaultPage reference consistently.");

    const editor::RenamePagePlan plan =
        pageRenameService_.BuildPlan(loaded_,
                                     files_,
                                     BuildPageRenameRequest(pageRenamePopup_.pageIndex, pageRenamePopup_.newName.data()));

    ImGui::SeparatorText("References found");
    if (plan.references.empty())
    {
        ImGui::TextDisabled("No scanned window reference is currently eligible for this rename.");
    }
    else
    {
        for (const editor::RenamePageReference& reference : plan.references)
        {
            ImGui::PushID(reference.windowFile.string().c_str());
            ImGui::TextWrapped("%s", reference.windowFile.string().c_str());
            if (reference.updatesDefaultPage)
            {
                ImGui::TextDisabled("defaultPage will be updated in this window JSON");
            }
            ImGui::PopID();
        }
    }

    ImGui::SeparatorText("Files to modify");
    for (const std::filesystem::path& file : plan.filesToModify)
    {
        ImGui::TextWrapped("%s", file.string().c_str());
    }
    if (plan.filesToModify.empty())
    {
        ImGui::TextDisabled("No file would be rewritten.");
    }

    if (!plan.collisions.empty())
    {
        ImGui::SeparatorText("Collisions");
        for (const editor::RenamePageCollision& collision : plan.collisions)
        {
            ImGui::TextColored(ImVec4(0.95f, 0.42f, 0.42f, 1.0f),
                               "%s already exposes page '%s'",
                               collision.windowFile.string().c_str(),
                               collision.conflictingPageName.c_str());
        }
    }

    if (!plan.warnings.empty())
    {
        ImGui::SeparatorText("Warnings");
        for (const std::string& warning : plan.warnings)
        {
            ImGui::TextColored(ImVec4(0.95f, 0.78f, 0.38f, 1.0f), "%s", warning.c_str());
        }
    }

    ImGui::Spacing();
    ImGui::TextDisabled("This workflow updates the scanned JSON assets directly across the current asset tree.");

    if (!plan.error.empty())
    {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.95f, 0.42f, 0.42f, 1.0f), "%s", plan.error.c_str());
    }

    ImGui::Spacing();
    ImGui::BeginDisabled(!plan.canExecute);
    if (AccentButton("Rename page"))
    {
        if (ExecutePageRenamePlan(plan))
        {
            pageRenamePopup_ = {};
            ImGui::CloseCurrentPopup();
            ImGui::EndDisabled();
            ImGui::EndPopup();
            return;
        }
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
    {
        if (tutorial_->MatchesTarget("popup_page_rename_cancel"))
        {
            tutorial_->CompleteStep();
        }
        pageRenamePopup_ = {};
        ImGui::CloseCurrentPopup();
    }
    tutorial_->DrawHalo(
        "popup_page_rename_cancel",
        "Close the rename popup",
        "This discovery step only inspects the safe page-rename workflow. Close it without executing the rename.");

    ImGui::EndPopup();
}

void EditorApplication::DrawReticleRenamePopup()
{
    if (!ImGui::BeginPopupModal("Rename reticle globally", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        return;
    }

    const auto reticleIterator = loaded_.document.reticleLibrary.find(reticleRenamePopup_.currentTemplateId);
    if (reticleIterator == loaded_.document.reticleLibrary.end())
    {
        ImGui::TextWrapped("The selected reticle template is no longer available.");
        if (ImGui::Button("Close"))
        {
            reticleRenamePopup_ = {};
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        return;
    }

    const mfd::ReticleGroup& reticle = reticleIterator->second;

    ImGui::TextColored(ImVec4(0.33f, 0.86f, 0.78f, 1.0f), "Rename reticle globally");
    ImGui::TextWrapped(
        "Pages reference reticle templates by logical id. Review every scanned page reference before renaming this shared template across the current asset tree.");
    ImGui::Separator();

    ImGui::TextDisabled("Old id");
    ImGui::TextWrapped("%s", reticle.id.empty() ? reticleIterator->first.c_str() : reticle.id.c_str());

    ImGui::InputText("New id", reticleRenamePopup_.newName.data(), reticleRenamePopup_.newName.size());
    ShowItemTooltip("Use the safe rename workflow to update the template JSON id and every scanned page template reference consistently.");

    ImGui::Checkbox("Rename template JSON file too", &reticleRenamePopup_.renameTemplateFile);
    ShowItemTooltip("Also move the template JSON file to the default file name derived from the new template id.");

    const editor::RenameReticlePlan plan =
        reticleRenameService_.BuildPlan(loaded_,
                                        files_,
                                        BuildReticleRenameRequest(
                                            reticleRenamePopup_.currentTemplateId,
                                            reticleRenamePopup_.newName.data(),
                                            reticleRenamePopup_.renameTemplateFile));

    ImGui::SeparatorText("Template file");
    if (plan.currentTemplateFile.empty())
    {
        ImGui::TextDisabled("No tracked template file is currently available.");
    }
    else
    {
        ImGui::TextWrapped("Current: %s", plan.currentTemplateFile.string().c_str());
        if (reticleRenamePopup_.renameTemplateFile)
        {
            ImGui::TextWrapped("Target: %s", plan.targetTemplateFile.string().c_str());
        }
        else
        {
            ImGui::TextDisabled("Logical rename only: keep the current template JSON file path.");
        }
    }

    ImGui::SeparatorText("References found");
    if (plan.references.empty())
    {
        ImGui::TextDisabled("No scanned page currently references this template. Only the template JSON will be rewritten.");
    }
    else
    {
        for (const editor::RenameReticleReference& reference : plan.references)
        {
            ImGui::PushID((reference.pageFile.string() + reference.ownerReticleId).c_str());
            ImGui::TextWrapped("%s", reference.pageFile.string().c_str());
            ImGui::TextDisabled("%s '%s' on page '%s'",
                                ReticleReferenceKindLabel(reference.kind),
                                reference.ownerReticleId.c_str(),
                                reference.pageName.c_str());
            ImGui::PopID();
        }
    }

    ImGui::SeparatorText("Files to modify");
    for (const std::filesystem::path& file : plan.filesToModify)
    {
        ImGui::TextWrapped("%s", file.string().c_str());
    }
    if (plan.filesToModify.empty())
    {
        ImGui::TextDisabled("No file would be rewritten.");
    }

    if (!plan.filesToDelete.empty())
    {
        ImGui::SeparatorText("Files to delete");
        for (const std::filesystem::path& file : plan.filesToDelete)
        {
            ImGui::TextWrapped("%s", file.string().c_str());
        }
    }

    if (!plan.collisions.empty())
    {
        ImGui::SeparatorText("Collisions");
        for (const editor::RenameReticleCollision& collision : plan.collisions)
        {
            ImGui::TextColored(ImVec4(0.95f, 0.42f, 0.42f, 1.0f),
                               "%s already uses template '%s' through %s '%s' on page '%s'",
                               collision.pageFile.string().c_str(),
                               collision.conflictingTemplateId.c_str(),
                               ReticleReferenceKindLabel(collision.kind),
                               collision.conflictingReticleId.c_str(),
                               collision.pageName.c_str());
        }
    }

    if (!plan.warnings.empty())
    {
        ImGui::SeparatorText("Warnings");
        for (const std::string& warning : plan.warnings)
        {
            ImGui::TextColored(ImVec4(0.95f, 0.78f, 0.38f, 1.0f), "%s", warning.c_str());
        }
    }

    ImGui::Spacing();
    ImGui::TextDisabled("This workflow updates the scanned JSON assets directly across the current asset tree.");
    ImGui::TextDisabled("After a successful rename, regenerate the generated client API if this template is exposed there.");

    if (!plan.error.empty())
    {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.95f, 0.42f, 0.42f, 1.0f), "%s", plan.error.c_str());
    }

    ImGui::Spacing();
    ImGui::BeginDisabled(!plan.canExecute);
    if (AccentButton("Rename reticle"))
    {
        if (ExecuteReticleRenamePlan(plan))
        {
            reticleRenamePopup_ = {};
            ImGui::CloseCurrentPopup();
            ImGui::EndDisabled();
            ImGui::EndPopup();
            return;
        }
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
    {
        if (tutorial_->MatchesTarget("popup_reticle_rename_cancel"))
        {
            tutorial_->CompleteStep();
        }
        reticleRenamePopup_ = {};
        ImGui::CloseCurrentPopup();
    }
    tutorial_->DrawHalo(
        "popup_reticle_rename_cancel",
        "Close the rename popup",
        "This discovery step only inspects the safe reticle-rename workflow. Close it without executing the rename.");

    ImGui::EndPopup();
}

void EditorApplication::DrawReticleExtractionPopup()
{
    if (!ImGui::BeginPopupModal("Extract as reticle", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        return;
    }

    ImGui::TextColored(ImVec4(0.33f, 0.86f, 0.78f, 1.0f), "Extract a reusable reticle");
    ImGui::TextWrapped(
        "Review the current page-reticle selection before replacing it with one reusable library template. The visual result should stay identical or nearly identical after extraction.");
    ImGui::Separator();

    ImGui::InputText("Template id", reticleExtractionPopup_.templateId.data(), reticleExtractionPopup_.templateId.size());
    ShowItemTooltip("Logical id assigned to the new shared reticle template. Planning resolves collisions automatically.");
    ImGui::InputText("Template file", reticleExtractionPopup_.templateFile.data(), reticleExtractionPopup_.templateFile.size());
    ShowItemTooltip("Optional JSON file path for the new shared template. Clear it to let the editor derive the default file from the target template id.");

    const editor::ReticleExtractionPlan plan =
        reticleExtractionService_.BuildPlan(loaded_, files_, BuildReticleExtractionRequest());

    ImGui::SeparatorText("Selection");
    if (plan.sourceReticleIds.empty())
    {
        ImGui::TextDisabled("No page reticle is currently selected.");
    }
    else
    {
        for (const std::string& reticleId : plan.sourceReticleIds)
        {
            ImGui::BulletText("%s", reticleId.c_str());
        }
    }

    ImGui::SeparatorText("Extraction result");
    if (plan.targetTemplateId.empty())
    {
        ImGui::TextDisabled("No target template id is currently available.");
    }
    else
    {
        ImGui::TextWrapped("Template id: %s", plan.targetTemplateId.c_str());
        if (plan.templateIdAdjusted)
        {
            ImGui::TextColored(ImVec4(0.95f, 0.78f, 0.38f, 1.0f),
                               "The requested id collides with the current library. The editor will stage '%s' instead.",
                               plan.targetTemplateId.c_str());
        }
    }

    if (plan.targetTemplateFile.empty())
    {
        ImGui::TextDisabled("No target template file is currently available.");
    }
    else
    {
        ImGui::TextWrapped("Template file: %s", plan.targetTemplateFile.string().c_str());
        if (plan.templateFileAdjusted)
        {
            ImGui::TextColored(ImVec4(0.95f, 0.78f, 0.38f, 1.0f),
                               "The requested file collides with an existing staged or on-disk asset. A unique file name will be used.");
        }
    }

    ImGui::Text("Flattened primitives: %d", static_cast<int>(plan.extractedPrimitiveCount));
    ImGui::Text("Replacement reticle id: %s",
                plan.replacementInstanceId.empty() ? "<pending>" : plan.replacementInstanceId.c_str());
    ImGui::Text("Editor layer: %s", plan.targetLayerId.empty() ? "<none>" : plan.targetLayerId.c_str());
    ImGui::TextDisabled("Draw order: %s", plan.drawOnTop ? "draw on top" : "regular page reticle order");

    ImGui::Spacing();
    ImGui::TextDisabled("The new template is staged in memory first, then written with File > Save.");
    ImGui::TextDisabled("Unsupported cases are rejected here instead of partially mutating the page.");

    if (!plan.error.empty())
    {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.95f, 0.42f, 0.42f, 1.0f), "%s", plan.error.c_str());
    }

    ImGui::Spacing();
    ImGui::BeginDisabled(!plan.canExecute);
    if (AccentButton("Extract reticle"))
    {
        if (ExecuteReticleExtractionPlan(plan))
        {
            reticleExtractionPopup_ = {};
            ImGui::CloseCurrentPopup();
            ImGui::EndDisabled();
            ImGui::EndPopup();
            return;
        }
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
    {
        if (tutorial_->MatchesTarget("popup_reticle_extract_cancel"))
        {
            tutorial_->CompleteStep();
        }
        reticleExtractionPopup_ = {};
        ImGui::CloseCurrentPopup();
    }
    tutorial_->DrawHalo(
        "popup_reticle_extract_cancel",
        "Close the extraction popup",
        "This discovery step only inspects the extraction workflow. Close it without replacing the selected reticle.");

    ImGui::EndPopup();
}

void EditorApplication::DrawDesignExportPopup()
{
    if (!ImGui::BeginPopupModal("Export design", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        return;
    }

    if (!HasOpenWindow())
    {
        ImGui::TextWrapped("Open one window before exporting design documentation.");
        if (ImGui::Button("Close"))
        {
            designExportPopup_ = {};
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        return;
    }

    ImGui::TextColored(ImVec4(0.33f, 0.86f, 0.78f, 1.0f), "Export design");
    ImGui::TextWrapped(
        "Generate Markdown ICD files and exploded designer views for the currently loaded window without modifying authored JSON assets.");
    ImGui::Separator();

    if (designExportPopup_.exportCompleted)
    {
        ImGui::TextWrapped("Design export created:");
        ImGui::TextWrapped("%s", designExportPopup_.exportedFolder.string().c_str());

        if (!designExportPopup_.warnings.empty())
        {
            ImGui::SeparatorText("Warnings");
            for (const std::string& warning : designExportPopup_.warnings)
            {
                ImGui::TextColored(ImVec4(0.95f, 0.78f, 0.38f, 1.0f), "%s", warning.c_str());
            }
        }

        if (AccentButton("Open folder"))
        {
            std::string error;
            if (!editor::OpenFolderInFileExplorer(designExportPopup_.exportedFolder, &error))
            {
                RebuildStatus(error.empty() ? "Opening the export folder failed." : error, true);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Close"))
        {
            designExportPopup_ = {};
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        return;
    }

    ImGui::InputText("Output folder", designExportPopup_.outputFolder.data(), designExportPopup_.outputFolder.size());
    ImGui::SameLine();
    if (ImGui::Button("Browse..."))
    {
        std::string dialogError;
        const std::filesystem::path initialFolder =
            designExportPopup_.outputFolder.front() == '\0' ? windowFile_.parent_path()
                                                            : std::filesystem::path(designExportPopup_.outputFolder.data());
        if (const auto selectedFolder =
                editor::OpenFolderDialog(initialFolder, "Select design export folder", &dialogError);
            selectedFolder.has_value())
        {
            CopyTextBuffer(designExportPopup_.outputFolder, selectedFolder->lexically_normal().string());
        }
        else if (!dialogError.empty())
        {
            RebuildStatus(dialogError, true);
        }
    }
    ShowItemTooltip("Choose the folder where the design export should be created.");

    ImGui::SeparatorText("Options");
    ImGui::Checkbox("Export Markdown ICD", &designExportPopup_.exportMarkdownIcd);
    ImGui::Checkbox("Export exploded designer views", &designExportPopup_.exportExplodedViews);
    ImGui::Checkbox("Include canvas coordinates", &designExportPopup_.includeCanvasCoordinates);
    ImGui::Checkbox("Include generated C++ usage snippets", &designExportPopup_.includeCppSnippets);
    ImGui::Checkbox("Include strobe section", &designExportPopup_.includeStrobe);
    ImGui::Checkbox("Include blink section", &designExportPopup_.includeBlink);
    ImGui::Checkbox("Include primitive ids when available", &designExportPopup_.includePrimitiveIds);
    ImGui::Checkbox("Include mapping hash when available", &designExportPopup_.includeMappingHash);

    const editor::DesignExportPlan plan = designExportService_.BuildPlan(BuildDesignExportRequest());

    if (plan.canExecute)
    {
        ImGui::SeparatorText("Plan");
        ImGui::TextWrapped("Final output: %s", plan.outputFolder.string().c_str());
        ImGui::Text("Files: %d", static_cast<int>(plan.filesToWrite.size()));
    }

    if (!plan.warnings.empty())
    {
        ImGui::SeparatorText("Warnings");
        for (const std::string& warning : plan.warnings)
        {
            ImGui::TextColored(ImVec4(0.95f, 0.78f, 0.38f, 1.0f), "%s", warning.c_str());
        }
    }

    if (!plan.error.empty())
    {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.95f, 0.42f, 0.42f, 1.0f), "%s", plan.error.c_str());
    }

    ImGui::Spacing();
    ImGui::BeginDisabled(!plan.canExecute);
    if (AccentButton("Export"))
    {
        ExecuteDesignExportPlan(plan);
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
    {
        if (tutorial_->MatchesTarget("popup_design_export_cancel"))
        {
            tutorial_->CompleteStep();
        }
        designExportPopup_ = {};
        ImGui::CloseCurrentPopup();
    }
    tutorial_->DrawHalo(
        "popup_design_export_cancel",
        "Close the export popup",
        "This discovery step only inspects the design-export workflow. Close it without generating files.");

    ImGui::EndPopup();
}

void EditorApplication::DrawPageManagementPopup()
{
    if (!ImGui::BeginPopupModal("Manage page", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        return;
    }

    const bool deleteAsset = pageManagementPopup_.action == PageManagementAction::DeleteAsset;
    const bool pageIndexValid = pageManagementPopup_.pageIndex >= 0 &&
                                pageManagementPopup_.pageIndex < static_cast<int>(loaded_.document.pages.size());
    if (!pageIndexValid)
    {
        ImGui::TextWrapped("The selected page is no longer available.");
        if (ImGui::Button("Close"))
        {
            pageManagementPopup_ = {};
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        return;
    }

    const mfd::PageDefinition& page = loaded_.document.pages[static_cast<std::size_t>(pageManagementPopup_.pageIndex)];
    if (deleteAsset)
    {
        ImGui::TextColored(ImVec4(0.95f, 0.50f, 0.42f, 1.0f), "Delete page asset");
        ImGui::TextWrapped("This removes the page from the current window and deletes its JSON file on the next save.");
    }
    else
    {
        ImGui::TextColored(ImVec4(0.33f, 0.86f, 0.78f, 1.0f), "Remove page from window");
        ImGui::TextWrapped("This detaches the page from the current window but keeps its JSON file on disk.");
    }

    ImGui::Separator();
    ImGui::TextDisabled("Page");
    ImGui::TextWrapped("%s", page.name.c_str());

    if (page.defaultPage && loaded_.document.pages.size() > 1U)
    {
        const int currentReplacementIndex =
            pageManagementPopup_.replacementPageIndex >= 0 ? pageManagementPopup_.replacementPageIndex
                                                           : SuggestReplacementPageIndex(loaded_.document.pages, pageManagementPopup_.pageIndex);
        pageManagementPopup_.replacementPageIndex = currentReplacementIndex;

        std::string replacementLabel = "Select replacement";
        if (currentReplacementIndex >= 0 &&
            currentReplacementIndex < static_cast<int>(loaded_.document.pages.size()) &&
            currentReplacementIndex != pageManagementPopup_.pageIndex)
        {
            replacementLabel = loaded_.document.pages[static_cast<std::size_t>(currentReplacementIndex)].name;
        }

        if (ImGui::BeginCombo("Replacement default page", replacementLabel.c_str()))
        {
            for (int index = 0; index < static_cast<int>(loaded_.document.pages.size()); ++index)
            {
                if (index == pageManagementPopup_.pageIndex)
                {
                    continue;
                }

                const bool selected = pageManagementPopup_.replacementPageIndex == index;
                if (ImGui::Selectable(loaded_.document.pages[static_cast<std::size_t>(index)].name.c_str(), selected))
                {
                    pageManagementPopup_.replacementPageIndex = index;
                }
                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ShowItemTooltip("Choose the page that should become the new default after this operation.");
    }
    else
    {
        pageManagementPopup_.replacementPageIndex = -1;
    }

    const std::optional<int> replacementPageIndex =
        pageManagementPopup_.replacementPageIndex >= 0 ? std::optional<int> {pageManagementPopup_.replacementPageIndex}
                                                       : std::nullopt;
    const editor::PageRemovePlan removePlan =
        pageManagementService_.BuildRemovePlan(loaded_,
                                               files_,
                                               editor::PageRemoveRequest {pageManagementPopup_.pageIndex, replacementPageIndex});
    const editor::PageDeletePlan deletePlan =
        pageManagementService_.BuildDeletePlan(loaded_,
                                               files_,
                                               editor::PageDeleteRequest {pageManagementPopup_.pageIndex,
                                                                          replacementPageIndex,
                                                                          DefaultProjectAssetFolder("assets"),
                                                                          pageManagementPopup_.allowOutsideAssetsRoot});

    if (deleteAsset)
    {
        ImGui::Separator();
        ImGui::TextDisabled("File");
        ImGui::TextWrapped("%s", deletePlan.pageFile.empty() ? "<unknown>" : deletePlan.pageFile.string().c_str());

        if (deletePlan.outsideAssetsRoot)
        {
            ImGui::TextColored(ImVec4(0.95f, 0.42f, 0.42f, 1.0f),
                               "The page file is outside the protected source assets root.");
            ImGui::Checkbox("Allow delete outside source assets root", &pageManagementPopup_.allowOutsideAssetsRoot);
        }

        ImGui::Checkbox("Confirm page asset deletion on next save", &pageManagementPopup_.confirmDelete);
        ShowItemTooltip("This confirmation is required before the page file is marked for deletion.");
    }

    const std::string errorMessage = deleteAsset ? deletePlan.error : removePlan.error;
    if (!errorMessage.empty())
    {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.95f, 0.42f, 0.42f, 1.0f), "%s", errorMessage.c_str());
    }

    const bool canExecutePlan = deleteAsset ? (deletePlan.canExecute && pageManagementPopup_.confirmDelete) : removePlan.canExecute;
    ImGui::Spacing();
    ImGui::BeginDisabled(!canExecutePlan);
    if (AccentButton(deleteAsset ? "Delete page asset" : "Remove page from window"))
    {
        const bool executed = deleteAsset ? ExecutePageDeletePlan(deletePlan) : ExecutePageRemovePlan(removePlan);
        if (executed)
        {
            pageManagementPopup_ = {};
            ImGui::CloseCurrentPopup();
            ImGui::EndDisabled();
            ImGui::EndPopup();
            return;
        }
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
    {
        pageManagementPopup_ = {};
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void EditorApplication::SeedNewWindowAssetDraftPaths()
{
    const std::filesystem::path windowFile = std::filesystem::path(newWindowDraft_.windowFile.data()).lexically_normal();
    if (windowFile.empty() || !windowFile.is_absolute() || IsExecStagingPath(windowFile))
    {
        CopyTextBuffer(newWindowDraft_.windowFile, DefaultProjectAssetFolder("assets/windows/new_window.json").string());
    }

    const std::filesystem::path resolvedWindowFile = std::filesystem::path(newWindowDraft_.windowFile.data()).lexically_normal();
    const std::filesystem::path reticleFolder = std::filesystem::path(newWindowDraft_.reticleLibraryFolder.data()).lexically_normal();
    if (reticleFolder.empty() || !reticleFolder.is_absolute() || IsExecStagingPath(reticleFolder))
    {
        CopyTextBuffer(newWindowDraft_.reticleLibraryFolder, DefaultSiblingAssetFile(resolvedWindowFile, "reticles", "").string());
    }

    const std::filesystem::path firstPageFile = std::filesystem::path(newWindowDraft_.firstPageFile.data()).lexically_normal();
    if (firstPageFile.empty() || !firstPageFile.is_absolute() || IsExecStagingPath(firstPageFile))
    {
        CopyTextBuffer(newWindowDraft_.firstPageFile, DefaultSiblingAssetFile(resolvedWindowFile, "pages", "page1.json").string());
    }
}

void EditorApplication::SeedNewPageAssetDraftPath()
{
    const std::filesystem::path pageFile = std::filesystem::path(newPageDraft_.fileName.data()).lexically_normal();
    if (!pageFile.empty() && pageFile.is_absolute() && !IsExecStagingPath(pageFile))
    {
        return;
    }

    const std::filesystem::path windowFile = loaded_.window.sourceFile.empty()
                                                 ? DefaultProjectAssetFolder("assets/windows/new_window.json")
                                                 : loaded_.window.sourceFile;
    const std::filesystem::path defaultFileName = JsonFileNameOrFallback(pageFile, "new_page.json");
    CopyTextBuffer(newPageDraft_.fileName, DefaultSiblingAssetFile(windowFile, "pages", defaultFileName.string()).string());
}

void EditorApplication::PrepareTutorialStep()
{
    using editor::tutorial::TutorialStepId;

    tutorial_->ClampStepIndex();
    tutorial_->ResetPhase();
    tutorial_->ClearFocusLayer();

    const auto setPrimitiveDraft = [&](const mfd::PrimitiveType primitiveType)
    {
        for (int index = 0; index < static_cast<int>(kPrimitiveTypes.size()); ++index)
        {
            if (kPrimitiveTypes[static_cast<std::size_t>(index)] == primitiveType)
            {
                newLibraryReticleDraft_.primitiveTypeIndex = index;
                break;
            }
        }
    };
    const auto selectTutorialPageOrFallback = [this](const std::string_view pageName)
    {
        if (const int pageIndex = FindPageIndexByName(loaded_, pageName); pageIndex >= 0)
        {
            SelectPage(pageIndex);
            return;
        }

        if (!loaded_.document.pages.empty())
        {
            SelectPage(std::clamp(selection_.pageIndex, 0, static_cast<int>(loaded_.document.pages.size()) - 1));
        }
    };
    const auto focusLibraryReticleInBrowser = [this](const std::string_view templateId)
    {
        const auto iterator = loaded_.document.reticleLibrary.find(std::string {templateId});
        if (iterator == loaded_.document.reticleLibrary.end())
        {
            return;
        }

        selection_.libraryReticleId = iterator->first;
        selection_.libraryBrowserReticleId = iterator->first;
        selection_.primitiveIndex = -1;
    };

    const auto tutorialSteps = editor::tutorial::Steps();
    const editor::tutorial::TutorialStepDefinition& step =
        tutorialSteps[static_cast<std::size_t>(tutorial_->StepIndex())];
    if (!editor::tutorial::IsUiStep(step))
    {
        return;
    }

    switch (tutorial_->StepIndex())
    {
    case static_cast<int>(TutorialStepId::CreateWindow):
        tutorial_->ClearTrackedReticle();
        CopyTextBuffer(newWindowDraft_.windowFile, DefaultProjectAssetFolder("assets/windows/mfd_tutorial.json").string());
        CopyTextBuffer(newWindowDraft_.title, "MFD Tutorial");
        newWindowDraft_.width = 480;
        newWindowDraft_.height = 480;
        newWindowDraft_.positionX = 120;
        newWindowDraft_.positionY = 80;
        CopyTextBuffer(newWindowDraft_.fontFile, "");
        CopyTextBuffer(newWindowDraft_.reticleLibraryFolder, DefaultProjectAssetFolder("assets/reticles").string());
        newWindowDraft_.commandUdpEnabled = true;
        CopyTextBuffer(newWindowDraft_.commandAddress, "127.0.0.1");
        newWindowDraft_.commandPort = 49000;
        newWindowDraft_.commandMaxPacketSize = 65507;
        newWindowDraft_.feedbackUdpEnabled = true;
        CopyTextBuffer(newWindowDraft_.feedbackAddress, "127.0.0.1");
        newWindowDraft_.feedbackPort = 49001;
        newWindowDraft_.feedbackMaxPacketSize = 65507;
        newWindowDraft_.createInitialPage = false;
        CopyTextBuffer(newWindowDraft_.firstPageName, "Page1");
        CopyTextBuffer(newWindowDraft_.firstPageTitle, "Page 1");
        CopyTextBuffer(newWindowDraft_.firstPageFile, DefaultProjectAssetFolder("assets/pages/mfd_tutorial_page1.json").string());
        newWindowDraft_.firstPageBackground = ImVec4(0.0f, 0.125f, 0.376f, 1.0f);
        break;
    case static_cast<int>(TutorialStepId::CreateRadarTrackReticle):
        CopyTextBuffer(newLibraryReticleDraft_.id, "mfd_tutorial_radar_track");
        setPrimitiveDraft(mfd::PrimitiveType::Diamond);
        break;
    case static_cast<int>(TutorialStepId::CreateCircleReticle):
        CopyTextBuffer(newLibraryReticleDraft_.id, "mfd_tutorial_circle");
        setPrimitiveDraft(mfd::PrimitiveType::Circle);
        break;
    case static_cast<int>(TutorialStepId::CreateStrobeCursorReticle):
        CopyTextBuffer(newLibraryReticleDraft_.id, kTutorialStrobeCursorTemplateId);
        setPrimitiveDraft(mfd::PrimitiveType::Line);
        break;
    case static_cast<int>(TutorialStepId::CreatePage1):
        CopyTextBuffer(newPageDraft_.name, "Page1");
        CopyTextBuffer(newPageDraft_.title, "Page 1");
        CopyTextBuffer(newPageDraft_.fileName, DefaultProjectAssetFolder("assets/pages/mfd_tutorial_page1.json").string());
        newPageDraft_.background = ImVec4(0.0f, 0.125f, 0.376f, 1.0f);
        break;
    case static_cast<int>(TutorialStepId::CreateRadarTrackLayerOnPage1):
        selectTutorialPageOrFallback("Page1");
        break;
    case static_cast<int>(TutorialStepId::AllowPage1DynamicReticleTemplate):
        selectTutorialPageOrFallback("Page1");
        break;
    case static_cast<int>(TutorialStepId::AssignPage1StrobeTemplate):
        selectTutorialPageOrFallback("Page1");
        break;
    case static_cast<int>(TutorialStepId::AddCircleReticleToPage1):
    {
        selectTutorialPageOrFallback("Page1");
        if (loaded_.document.reticleLibrary.find("mfd_tutorial_circle") != loaded_.document.reticleLibrary.end())
        {
            SelectLibraryReticle("mfd_tutorial_circle");
        }
        break;
    }
    case static_cast<int>(TutorialStepId::ClipCircleOutside):
    {
        if (const int pageIndex = FindPageIndexByName(loaded_, "Page1"); pageIndex >= 0)
        {
            SelectPage(pageIndex);
            if (!tutorial_->TrackedReticleId().empty())
            {
                const mfd::PageDefinition& page = loaded_.document.pages[static_cast<std::size_t>(pageIndex)];
                if (const int reticleIndex = FindPageReticleIndexById(page, tutorial_->TrackedReticleId()); reticleIndex >= 0)
                {
                    SelectPageReticle(pageIndex, reticleIndex);
                }
            }
        }
        break;
    }
    case static_cast<int>(TutorialStepId::AddAndHideEditorLayer):
        selectTutorialPageOrFallback("Page1");
        break;
    case static_cast<int>(TutorialStepId::CreatePage2):
        CopyTextBuffer(newPageDraft_.name, "Page2");
        CopyTextBuffer(newPageDraft_.title, "Page 2");
        CopyTextBuffer(newPageDraft_.fileName, DefaultProjectAssetFolder("assets/pages/mfd_tutorial_page2.json").string());
        newPageDraft_.background = ImVec4(0.04f, 0.08f, 0.14f, 1.0f);
        break;
    case static_cast<int>(TutorialStepId::CreateProgressBarReticle):
        CopyTextBuffer(newLibraryReticleDraft_.id, "mfd_tutorial_progress_bar");
        setPrimitiveDraft(mfd::PrimitiveType::Rectangle);
        break;
    case static_cast<int>(TutorialStepId::ExposeProgressBarFillPrimitive):
        if (loaded_.document.reticleLibrary.find("mfd_tutorial_progress_bar") != loaded_.document.reticleLibrary.end())
        {
            SelectLibraryPrimitive("mfd_tutorial_progress_bar", 0);
        }
        break;
    case static_cast<int>(TutorialStepId::AddProgressBarToPage2):
        selectTutorialPageOrFallback("Page2");
        if (loaded_.document.reticleLibrary.find("mfd_tutorial_progress_bar") !=
            loaded_.document.reticleLibrary.end())
        {
            SelectLibraryReticle("mfd_tutorial_progress_bar");
        }
        break;
    case static_cast<int>(TutorialStepId::ShowPageContext):
        selectTutorialPageOrFallback("Page2");
        pagePreviewViewOptions_.showPageContext = false;
        break;
    case static_cast<int>(TutorialStepId::ShowLayerInspector):
        selectTutorialPageOrFallback("Page1");
        pagePreviewViewOptions_.showPageContext = false;
        pagePreviewViewOptions_.showLayerInspector = false;
        break;
    case static_cast<int>(TutorialStepId::ShowMinimap):
        selectTutorialPageOrFallback("Page1");
        pagePreviewViewOptions_.showPageContext = false;
        pagePreviewViewOptions_.showMinimap = false;
        break;
    case static_cast<int>(TutorialStepId::ShowReticleUsageHighlights):
        selectTutorialPageOrFallback("Page1");
        focusLibraryReticleInBrowser("mfd_tutorial_circle");
        pagePreviewViewOptions_.highlightReticleUsages = false;
        break;
    case static_cast<int>(TutorialStepId::ShowProblemsPanel):
        selectTutorialPageOrFallback("Page1");
        pagePreviewViewOptions_.showPageContext = false;
        pagePreviewViewOptions_.showProblemsPanel = false;
        break;
    case static_cast<int>(TutorialStepId::ToggleFullscreenPreview):
        if (fullscreenPreviewController_.IsActive())
        {
            ToggleFullscreenPagePreview();
        }
        selectTutorialPageOrFallback("Page1");
        pagePreviewViewOptions_.showPageContext = false;
        break;
    case static_cast<int>(TutorialStepId::InspectPageImportWorkflow):
        selectTutorialPageOrFallback("Page1");
        break;
    case static_cast<int>(TutorialStepId::InspectPageRenameWorkflow):
        selectTutorialPageOrFallback("Page1");
        break;
    case static_cast<int>(TutorialStepId::InspectReticleRenameWorkflow):
        if (loaded_.document.reticleLibrary.find("mfd_tutorial_circle") != loaded_.document.reticleLibrary.end())
        {
            SelectLibraryReticle("mfd_tutorial_circle");
        }
        break;
    case static_cast<int>(TutorialStepId::InspectDesignExportWorkflow):
        if (fullscreenPreviewController_.IsActive())
        {
            ToggleFullscreenPagePreview();
        }
        selectTutorialPageOrFallback("Page1");
        pagePreviewViewOptions_.showPageContext = false;
        break;
    default:
        break;
    }
}

bool EditorApplication::CreateNewWindow()
{
    const std::filesystem::path windowFile = std::filesystem::path(newWindowDraft_.windowFile.data()).lexically_normal();
    if (windowFile.empty())
    {
        RebuildStatus("Window file cannot be empty.", true);
        return false;
    }
    if (IsExecStagingPath(windowFile))
    {
        RebuildStatus("Choose a source assets folder for the window JSON, not a staged _Exec folder.", true);
        return false;
    }

    const std::string windowTitle = newWindowDraft_.title.data();
    if (windowTitle.empty())
    {
        RebuildStatus("Window title cannot be empty.", true);
        return false;
    }

    if (newWindowDraft_.width <= 0 || newWindowDraft_.height <= 0)
    {
        RebuildStatus("Window size must be strictly positive.", true);
        return false;
    }

    const std::filesystem::path windowBaseFolder = windowFile.parent_path();

    mfd::LoadedWindowConfiguration next {};
    next.window.sourceFile = windowFile;
    next.window.title = windowTitle;
    next.window.width = std::max(1, newWindowDraft_.width);
    next.window.height = std::max(1, newWindowDraft_.height);
    next.window.positionX = newWindowDraft_.positionX;
    next.window.positionY = newWindowDraft_.positionY;
    next.window.targetFps = 60;

    const std::filesystem::path fontPath = std::filesystem::path(newWindowDraft_.fontFile.data()).lexically_normal();
    if (!fontPath.empty())
    {
        next.window.fontFile = fontPath.is_relative() ? (windowBaseFolder / fontPath).lexically_normal() : fontPath;
    }

    const std::filesystem::path reticleFolder =
        std::filesystem::path(newWindowDraft_.reticleLibraryFolder.data()).lexically_normal();
    const std::filesystem::path effectiveReticleFolder =
        reticleFolder.empty() ? DefaultSiblingAssetFile(windowFile, "reticles", "") : reticleFolder;
    if (IsExecStagingPath(effectiveReticleFolder))
    {
        RebuildStatus("Choose a source assets folder for the reticle library, not a staged _Exec folder.", true);
        return false;
    }
    next.window.reticleLibraryFolder = effectiveReticleFolder.is_relative()
                                           ? (windowBaseFolder / effectiveReticleFolder).lexically_normal()
                                           : effectiveReticleFolder;

    mfd::WindowUdpCommandTransport commandUdp {};
    commandUdp.enabled = newWindowDraft_.commandUdpEnabled;
    commandUdp.address = newWindowDraft_.commandAddress.data();
    commandUdp.port = static_cast<std::uint16_t>(
        std::clamp(newWindowDraft_.commandPort, 0, static_cast<int>(std::numeric_limits<std::uint16_t>::max())));
    commandUdp.maxPacketSize = std::max(512, newWindowDraft_.commandMaxPacketSize);
    next.window.commandTransports.udp = commandUdp;

    mfd::WindowUdpFeedbackTransport feedbackUdp {};
    feedbackUdp.enabled = newWindowDraft_.feedbackUdpEnabled;
    feedbackUdp.address = newWindowDraft_.feedbackAddress.data();
    feedbackUdp.port = static_cast<std::uint16_t>(
        std::clamp(newWindowDraft_.feedbackPort, 0, static_cast<int>(std::numeric_limits<std::uint16_t>::max())));
    feedbackUdp.maxPacketSize = std::max(512, newWindowDraft_.feedbackMaxPacketSize);
    next.window.feedbackTransports.udp = feedbackUdp;

    editor::EditorFileLayout nextFiles {};
    if (std::filesystem::exists(next.window.reticleLibraryFolder))
    {
        std::string discoverError;
        if (!editor::DiscoverReticleTemplateFiles(next.window.reticleLibraryFolder, nextFiles, &discoverError))
        {
            RebuildStatus(discoverError, true);
            return false;
        }
    }
    if (newWindowDraft_.createInitialPage)
    {
        const std::string pageName = newWindowDraft_.firstPageName.data();
        if (pageName.empty())
        {
            RebuildStatus("Initial page name cannot be empty.", true);
            return false;
        }

        mfd::PageDefinition page {};
        page.name = pageName;
        page.normalizedName = mfd::NormalizePageName(pageName);
        page.title = newWindowDraft_.firstPageTitle.data();
        page.backgroundColor = ToColorRgba(newWindowDraft_.firstPageBackground);
        page.defaultPage = true;
        page.layers.push_back(mfd::PageLayerDefinition {std::string(mfd::kDefaultPageLayerId)});
        BootstrapEditorLayersForPage(page);
        next.document.pages.push_back(page);

        std::filesystem::path pageFile = std::filesystem::path(newWindowDraft_.firstPageFile.data()).lexically_normal();
        if (pageFile.empty())
        {
            pageFile = editor::DefaultPageFilePath(windowFile, page.name);
        }
        if (pageFile.is_relative())
        {
            pageFile = (windowBaseFolder / pageFile).lexically_normal();
        }
        if (IsExecStagingPath(pageFile))
        {
            RebuildStatus("Choose a source assets folder for the first page JSON, not a staged _Exec folder.", true);
            return false;
        }
        nextFiles.pageFiles.push_back(pageFile);
        next.window.pageFiles = nextFiles.pageFiles;
    }

    PushUndoSnapshot();
    loaded_ = std::move(next);
    files_ = std::move(nextFiles);
    windowFile_ = loaded_.window.sourceFile;
    ApplyPreviewFontFile(loaded_.window.fontFile);
    SelectPage(DefaultPageIndex(loaded_.document.pages));
    RebuildStatus("New window draft created. Use File > Save to write JSON files on disk.", false);
    return true;
}

bool EditorApplication::CreateNewPage()
{
    using editor::tutorial::TutorialStepId;

    const std::string pageName = newPageDraft_.name.data();
    if (pageName.empty())
    {
        RebuildStatus("Page name cannot be empty.", true);
        return false;
    }

    std::filesystem::path pageFile = std::filesystem::path(newPageDraft_.fileName.data()).lexically_normal();
    if (pageFile.empty())
    {
        pageFile = editor::DefaultPageFilePath(loaded_.window.sourceFile, pageName);
    }
    if (pageFile.is_relative())
    {
        pageFile = (loaded_.window.sourceFile.parent_path() / pageFile).lexically_normal();
    }
    if (pageFile.extension().empty())
    {
        pageFile += ".json";
    }
    if (IsExecStagingPath(pageFile))
    {
        RebuildStatus("Choose a source assets folder for the page JSON, not a staged _Exec folder.", true);
        return false;
    }

    PushUndoSnapshot();

    mfd::PageDefinition page;
    page.name = pageName;
    page.normalizedName = mfd::NormalizePageName(pageName);
    page.title = newPageDraft_.title.data();
    page.backgroundColor = ToColorRgba(newPageDraft_.background);
    page.layers.push_back(mfd::PageLayerDefinition {std::string(mfd::kDefaultPageLayerId)});
    if (tutorial_ != nullptr &&
        tutorial_->IsStep(static_cast<int>(TutorialStepId::CreatePage1)) &&
        page.name == "Page1")
    {
        page.layers.push_back(mfd::PageLayerDefinition {"overlay"});
        page.dynamicReticleBindings.push_back(mfd::DynamicReticleLayerBinding {
            "inspired_steering_cue",
            "overlay",
            0});
    }
    BootstrapEditorLayersForPage(page);

    loaded_.document.pages.push_back(page);
    files_.pageFiles.push_back(pageFile.lexically_normal());
    loaded_.window.pageFiles = files_.pageFiles;

    SelectPage(static_cast<int>(loaded_.document.pages.size()) - 1);
    RebuildStatus("Page '" + page.name + "' created.", false);
    return true;
}

bool EditorApplication::CreateNewLibraryReticleFromPrimitive()
{
    const std::string reticleId = newLibraryReticleDraft_.id.data();
    if (reticleId.empty())
    {
        RebuildStatus("Library reticle id cannot be empty.", true);
        return false;
    }

    const mfd::PrimitiveType primitiveType =
        kPrimitiveTypes[static_cast<std::size_t>(newLibraryReticleDraft_.primitiveTypeIndex)];
    std::string tutorialError;
    if (!tutorial_->ValidateNewLibraryReticleDraft(reticleId, primitiveType, tutorialError))
    {
        RebuildStatus(tutorialError, true);
        return false;
    }

    PushUndoSnapshot();
    mfd::ReticleGroup reticle = MakePrimitiveReticle(
        reticleId,
        primitiveType);
    tutorial_->ConfigureCreatedLibraryReticle(reticle);
    loaded_.document.reticleLibrary[reticle.id] = reticle;
    files_.templateFiles[reticle.id] = editor::DefaultTemplateFilePath(loaded_.window.reticleLibraryFolder, reticle.id);
    SelectLibraryReticle(reticle.id);
    RebuildStatus("Library reticle '" + reticle.id + "' created.", false);
    return true;
}

void EditorApplication::DuplicateSelectedLibraryReticle()
{
    const mfd::ReticleGroup* source = SelectedLibraryReticle();
    if (source == nullptr)
    {
        RebuildStatus("No library reticle selected.", true);
        return;
    }

    const std::string newId = duplicateLibraryReticleDraft_.id.data();
    if (newId.empty())
    {
        RebuildStatus("Duplicate id cannot be empty.", true);
        return;
    }

    PushUndoSnapshot();
    mfd::ReticleGroup copy = *source;
    copy.id = newId;
    copy.sourceTemplateId.clear();
    loaded_.document.reticleLibrary[newId] = copy;
    files_.templateFiles[newId] = editor::DefaultTemplateFilePath(loaded_.window.reticleLibraryFolder, newId);
    SelectLibraryReticle(newId);
    RebuildStatus("Library reticle duplicated as '" + newId + "'.", false);
}

bool EditorApplication::CreatePageReticleInstanceFromTemplate(const std::string_view templateId, const mfd::Vec2 position)
{
    mfd::PageDefinition* page = ActivePage();
    if (page == nullptr)
    {
        RebuildStatus("Select a page before dropping a library reticle.", true);
        return false;
    }

    const auto iterator = loaded_.document.reticleLibrary.find(std::string(templateId));
    if (iterator == loaded_.document.reticleLibrary.end())
    {
        RebuildStatus("Unknown library reticle: " + std::string(templateId), true);
        return false;
    }

    PushUndoSnapshot();

    const std::string instanceId = MakeUniqueReticleId(page->staticReticles, templateId);
    mfd::ReticleGroup instance = mfd::InstantiateReticle(
        iterator->second,
        instanceId,
        mfd::Transform2D {position, 0.0f, {1.0f, 1.0f}},
        {});
    instance.visible = true;
    instance.layerId = ActiveInsertionLayerId(*page);

    const LogicalBounds localBounds = ComputeReticleLocalBounds(instance);
    if (localBounds.valid)
    {
        const float width = std::max(0.001f, localBounds.max.x - localBounds.min.x);
        const float height = std::max(0.001f, localBounds.max.y - localBounds.min.y);
        const float maxDimension = std::max(width * std::abs(instance.transform.scale.x),
                                            height * std::abs(instance.transform.scale.y));
        if (maxDimension < 0.12f)
        {
            const float scaleFactor = std::clamp(0.12f / std::max(0.001f, maxDimension), 1.0f, 6.0f);
            instance.transform.scale.x = std::max(0.05f, instance.transform.scale.x * scaleFactor);
            instance.transform.scale.y = std::max(0.05f, instance.transform.scale.y * scaleFactor);
        }

        instance.transform.position =
            position - mfd::Rotate(mfd::Scale(localBounds.center, instance.transform.scale), instance.transform.rotationDegrees);
    }

    page->staticReticles.push_back(std::move(instance));
    SelectPageReticle(selection_.pageIndex, static_cast<int>(page->staticReticles.size()) - 1);
    RebuildStatus("Reticle '" + std::string(templateId) + "' dropped on page '" + page->name + "'.", false);
    return true;
}

mfd::ReticleGroup EditorApplication::MakePrimitiveReticle(std::string id, const mfd::PrimitiveType primitiveType)
{
    mfd::ReticleGroup reticle;
    reticle.id = std::move(id);

    mfd::Primitive primitive;
    primitive.id = "primitive_01";
    primitive.type = primitiveType;

    switch (primitiveType)
    {
    case mfd::PrimitiveType::Text:
        primitive.geometry = mfd::TextGeometry {"TXT", 0.06f};
        break;
    case mfd::PrimitiveType::Time:
        primitive.geometry = mfd::TimeGeometry {"%H:%M:%S", false, 0.06f};
        break;
    case mfd::PrimitiveType::Line:
        primitive.geometry = mfd::LineGeometry {{-0.10f, 0.0f}, {0.10f, 0.0f}};
        break;
    case mfd::PrimitiveType::Circle:
        primitive.geometry = mfd::CircleGeometry {0.10f};
        break;
    case mfd::PrimitiveType::Ring:
        primitive.geometry = mfd::RingGeometry {0.06f, 0.10f, 64};
        primitive.style.filled = true;
        primitive.style.fillColor = mfd::ColorRgba {0, 255, 102, 48};
        break;
    case mfd::PrimitiveType::Rectangle:
        primitive.geometry = mfd::RectangleGeometry {0.24f, 0.14f};
        break;
    case mfd::PrimitiveType::Ellipse:
        primitive.geometry = mfd::EllipseGeometry {0.24f, 0.14f};
        break;
    case mfd::PrimitiveType::Square:
        primitive.geometry = mfd::SquareGeometry {0.20f, 0.20f};
        break;
    case mfd::PrimitiveType::Diamond:
        primitive.geometry = mfd::DiamondGeometry {0.22f, 0.22f};
        break;
    case mfd::PrimitiveType::Triangle:
        primitive.geometry = mfd::TriangleGeometry {{{{-0.10f, -0.08f}, {0.10f, -0.08f}, {0.0f, 0.12f}}}};
        break;
    case mfd::PrimitiveType::Polyline:
        primitive.geometry = mfd::PolylineGeometry {{{{-0.10f, -0.10f}, {0.0f, 0.12f}, {0.10f, -0.10f}}}, false};
        break;
    case mfd::PrimitiveType::Bezier:
        primitive.geometry = mfd::BezierGeometry {{{{-0.12f, -0.12f}, {-0.04f, 0.12f}, {0.04f, -0.12f}, {0.12f, 0.12f}}}, 32};
        break;
    case mfd::PrimitiveType::Arc:
        primitive.geometry = mfd::ArcGeometry {0.12f, -45.0f, 135.0f, 48};
        break;
    case mfd::PrimitiveType::Image:
        primitive.geometry = mfd::ImageGeometry {};
        break;
    }

    reticle.primitives.push_back(std::move(primitive));
    return reticle;
}

mfd::PageStrobeDefinition EditorApplication::MakePageStrobeFromTemplate(
    const mfd::PageDefinition& page,
    const mfd::ReticleGroup& templ,
    const std::optional<mfd::PageStrobeDefinition>& previousStrobe)
{
    const std::string baseId =
        previousStrobe.has_value() && !previousStrobe->reticle.id.empty()
            ? previousStrobe->reticle.id
            : (templ.id.empty() ? std::string {"strobe"} : templ.id + "_strobe");

    mfd::PageStrobeDefinition strobe;
    strobe.reticle = mfd::InstantiateReticle(templ, MakeUniqueReticleId(page.staticReticles, baseId));
    strobe.reticle.visible = true;

    if (previousStrobe.has_value())
    {
        strobe.reticle.visible = previousStrobe->reticle.visible;
        strobe.reticle.transform = previousStrobe->reticle.transform;
        strobe.reticle.overrides = previousStrobe->reticle.overrides;
        strobe.reticle.blink = previousStrobe->reticle.blink;
        strobe.reticle.clipping = previousStrobe->reticle.clipping;
        strobe.capture = previousStrobe->capture;
        strobe.magnet = previousStrobe->magnet;
    }

    return strobe;
}

std::string EditorApplication::MakeUniqueReticleId(const std::vector<mfd::ReticleGroup>& groups, const std::string_view baseId)
{
    std::string candidate = std::string(baseId);
    int suffix = 1;

    while (ReticleIdExistsExact(groups, candidate))
    {
        candidate = std::string(baseId) + "_" + std::to_string(suffix++);
    }

    return candidate;
}

std::string EditorApplication::MakeUniqueLayerId(const mfd::PageDefinition& page, const std::string_view baseId)
{
    std::string candidate = baseId.empty() ? std::string {"layer"} : std::string(baseId);
    int suffix = 2;

    while (PageLayerIdExistsExact(page, candidate))
    {
        candidate = std::string(baseId.empty() ? "layer" : baseId) + "_" + std::to_string(suffix++);
    }

    return candidate;
}

void EditorApplication::DrawPageInspector()
{
    mfd::PageDefinition* page = ActivePage();
    if (page == nullptr)
    {
        ImGui::TextDisabled("No page selected.");
        return;
    }

    ImGui::TextColored(ImVec4(0.33f, 0.86f, 0.78f, 1.0f), "Page");
    ImGui::TextDisabled("Edit the page and work directly in the preview.");

    if (ImGui::Button("Remove page from window"))
    {
        OpenPageManagementPopup(PageManagementAction::RemoveFromWindow, selection_.pageIndex);
        return;
    }
    ShowItemTooltip("Remove the currently selected page from this window while keeping its authored JSON file.");

    ImGui::SameLine();
    if (ImGui::Button("Delete page asset..."))
    {
        OpenPageManagementPopup(PageManagementAction::DeleteAsset, selection_.pageIndex);
        return;
    }
    ShowItemTooltip("Remove the page from this window and mark its JSON file for deletion on the next save.");

    ImGui::SameLine();
    if (ImGui::Button("Rename page globally..."))
    {
        OpenPageRenamePopup(selection_.pageIndex);
        return;
    }
    ShowItemTooltip("Rename this page asset safely across the current asset tree and update every referenced window defaultPage.");

    ImGui::SameLine();
    ImGui::TextDisabled("Shortcut: Suppr opens the delete confirmation");

    const bool canPasteReticles = !pageReticleClipboard_.empty();
    ImGui::BeginDisabled(!canPasteReticles);
    if (ImGui::Button("Paste copied reticles"))
    {
        PasteCopiedPageReticles();
        ImGui::EndDisabled();
        return;
    }
    ShowItemTooltip("Paste copied page reticles onto this page.");
    ImGui::EndDisabled();

    if (canPasteReticles)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("Shortcut: Ctrl+V");
    }

    std::array<char, 128> name {};
    std::array<char, 128> title {};
    CopyTextBuffer(name, page->name);
    CopyTextBuffer(title, page->title);
    ImVec4 background = ToImGuiColor(page->backgroundColor);

    const bool nameChanged = ImGui::InputText("Name", name.data(), name.size());
    ShowItemTooltip("Internal page id used in JSON and API references.");
    if (ImGui::IsItemActivated())
    {
        PushUndoSnapshot();
    }
    if (nameChanged)
    {
        page->name = name.data();
        page->normalizedName = mfd::NormalizePageName(page->name);
    }

    const bool titleChanged = ImGui::InputText("Title", title.data(), title.size());
    ShowItemTooltip("Human-readable title shown in the editor and optionally in the runtime page chrome.");
    if (ImGui::IsItemActivated())
    {
        PushUndoSnapshot();
    }
    if (titleChanged)
    {
        page->title = title.data();
    }

    const bool bgChanged = ImGui::ColorEdit4("Background", &background.x);
    ShowItemTooltip("Preview and runtime background color for this page.");
    if (ImGui::IsItemActivated())
    {
        PushUndoSnapshot();
    }
    if (bgChanged)
    {
        page->backgroundColor = ToColorRgba(background);
    }

    const bool centerChanged = ImGui::DragFloat2("View center", &page->view.center.x, 0.01f, -1.0f, 1.0f, "%.3f");
    ShowItemTooltip(
        "Authored page camera center stored in the JSON.\n"
        "This is different from the temporary mouse-wheel zoom and pan used only by the editor preview.");
    if (ImGui::IsItemActivated())
    {
        PushUndoSnapshot();
    }
    if (centerChanged)
    {
        page->view.center.x = std::clamp(page->view.center.x, -1.0f, 1.0f);
        page->view.center.y = std::clamp(page->view.center.y, -1.0f, 1.0f);
        pagePreviewView_ = page->view;
        pagePreviewView_.zoom = mfd::SanitizeZoom(pagePreviewView_.zoom);
    }

    const bool zoomChanged = ImGui::DragFloat("Zoom", &page->view.zoom, 0.02f, 0.1f, 20.0f, "%.3f");
    ShowItemTooltip(
        "Authored default page zoom stored in the JSON.\n"
        "Mouse-wheel zoom in the preview does not change this value.");
    if (ImGui::IsItemActivated())
    {
        PushUndoSnapshot();
    }
    if (zoomChanged)
    {
        page->view.zoom = mfd::SanitizeZoom(page->view.zoom);
        pagePreviewView_ = page->view;
    }

    bool defaultPage = page->defaultPage;
    if (ImGui::Checkbox("Default page for this window", &defaultPage))
    {
        PushUndoSnapshot();
        if (defaultPage)
        {
            for (auto& candidate : loaded_.document.pages)
            {
                candidate.defaultPage = false;
            }
        }

        page->defaultPage = defaultPage;
    }
    ShowItemTooltip("Mark this page as the default page opened by the runtime for this window.");

    ImGui::TextDisabled("If no page is marked default, the runtime opens the first page in the window JSON.");

    DrawPageBlinkInspector(*page);
    DrawPageLayerInspector(*page);
    DrawPageDynamicTemplateInspector(*page);
    DrawPageStrobeInspector(*page);

    ImGui::Spacing();
    ImGui::TextDisabled("Static reticles: %d", static_cast<int>(page->staticReticles.size()));
    const int editorVisibleReticleCount = static_cast<int>(std::count_if(
        page->staticReticles.begin(),
        page->staticReticles.end(),
        [page](const mfd::ReticleGroup& reticle)
        {
            return IsReticleVisibleInEditor(*page, reticle);
        }));
    ImGui::TextDisabled("Visible in editor: %d", editorVisibleReticleCount);
}

void EditorApplication::DrawPageDynamicTemplateInspector(mfd::PageDefinition& page)
{
    constexpr std::array<TutorialDynamicTemplateInfo, 1> kTutorialDynamicTemplates {{
        {"mfd_tutorial_radar_track",
         "RadarTrackLayer",
         "page_dynamic_template_mfd_tutorial_radar_track",
         "Add mfd_tutorial_radar_track",
         "Bind the tutorial radar-track template to RadarTrackLayer on Page1 without touching the existing steering cue."},
    }};

    static std::unordered_map<std::string, DynamicBindingDraftState, mfd::TransparentStringHash, mfd::TransparentStringEqual>
        s_bindingDrafts;

    BootstrapEditorLayersForPage(page);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.72f, 0.86f, 0.95f, 1.0f), "Dynamic reticles");
    ImGui::TextDisabled("Choose one reticle template, choose one page layer, then click Add.");
    ImGui::TextDisabled("These entries are runtime-only bindings. They do not create authored static reticles on the page canvas.");

    std::vector<std::string> templateIds;
    templateIds.reserve(loaded_.document.reticleLibrary.size() + files_.templateFiles.size());
    for (const auto& entry : loaded_.document.reticleLibrary)
    {
        templateIds.push_back(entry.first);
    }
    for (const auto& entry : files_.templateFiles)
    {
        templateIds.push_back(entry.first);
    }
    std::sort(templateIds.begin(), templateIds.end());
    templateIds.erase(std::unique(templateIds.begin(), templateIds.end()), templateIds.end());

    DynamicBindingDraftState& draft = s_bindingDrafts[page.normalizedName];
    if ((draft.layerId.empty() || mfd::FindPageLayerDefinition(page, draft.layerId) == nullptr) && !page.layers.empty())
    {
        draft.layerId = page.layers.front().id;
    }

    const TutorialDynamicTemplateInfo* activeTutorialTemplate = nullptr;
    for (const TutorialDynamicTemplateInfo& tutorialTemplate : kTutorialDynamicTemplates)
    {
        if (tutorial_->MatchesTarget(tutorialTemplate.targetId))
        {
            activeTutorialTemplate = &tutorialTemplate;
            break;
        }
    }

    std::vector<std::string> availableTemplateIds;
    availableTemplateIds.reserve(templateIds.size());
    for (const std::string& templateId : templateIds)
    {
        if (!PageHasDynamicTemplateBinding(page, templateId))
        {
            availableTemplateIds.push_back(templateId);
        }
    }

    if (activeTutorialTemplate != nullptr &&
        std::any_of(availableTemplateIds.begin(),
                    availableTemplateIds.end(),
                    [activeTutorialTemplate](const std::string& candidate)
                    {
                        return mfd::PageNamesEqual(candidate, activeTutorialTemplate->templateId);
                    }))
    {
        draft.templateId = std::string(activeTutorialTemplate->templateId);
    }
    else if (!availableTemplateIds.empty() &&
             !std::any_of(availableTemplateIds.begin(),
                          availableTemplateIds.end(),
                          [&draft](const std::string& candidate)
                          {
                              return candidate == draft.templateId;
                          }))
    {
        draft.templateId = availableTemplateIds.front();
    }
    else if (availableTemplateIds.empty())
    {
        draft.templateId.clear();
    }

    if (activeTutorialTemplate != nullptr &&
        mfd::FindPageLayerDefinition(page, activeTutorialTemplate->preferredLayerId) != nullptr)
    {
        draft.layerId = std::string(activeTutorialTemplate->preferredLayerId);
    }

    ImGui::TextDisabled("Configured on this page: %d", static_cast<int>(page.dynamicReticleBindings.size()));
    ImGui::TextDisabled("Each reticle template can appear at most once in this dynamic list.");

    if (templateIds.empty())
    {
        ImGui::TextDisabled("No library reticle is available yet. Create one first.");
        return;
    }

    if (page.layers.empty())
    {
        ImGui::TextDisabled("No page layer is available yet. Add one in Page layers first.");
        return;
    }

    const bool canAddBinding = !draft.templateId.empty() && !draft.layerId.empty() &&
                               mfd::FindPageLayerDefinition(page, draft.layerId) != nullptr;

    if (ImGui::BeginCombo("Reticle template", draft.templateId.empty() ? "<none>" : draft.templateId.c_str()))
    {
        if (availableTemplateIds.empty())
        {
            ImGui::TextDisabled("All library templates are already bound on this page.");
        }
        else
        {
            for (const std::string& templateId : availableTemplateIds)
            {
                const bool selected = draft.templateId == templateId;
                if (ImGui::Selectable(templateId.c_str(), selected))
                {
                    draft.templateId = templateId;
                }
                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
        }
        ImGui::EndCombo();
    }
    ShowItemTooltip("Choose one unbound library reticle template to expose as one page-scoped dynamic reticle type.");

    if (ImGui::BeginCombo("Layer", draft.layerId.empty() ? "<none>" : draft.layerId.c_str()))
    {
        for (const mfd::PageLayerDefinition& layer : page.layers)
        {
            const bool selected = draft.layerId == layer.id;
            if (ImGui::Selectable(layer.id.c_str(), selected))
            {
                draft.layerId = layer.id;
            }
            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ShowItemTooltip("Choose which page layer will own runtime instances created from this dynamic reticle type.");

    ImGui::BeginDisabled(!canAddBinding);
    if (AccentButton("Add"))
    {
        if (activeTutorialTemplate != nullptr &&
            !mfd::PageNamesEqual(draft.templateId, activeTutorialTemplate->templateId))
        {
            RebuildStatus("Tutorial: choose '" + std::string(activeTutorialTemplate->templateId) + "' before clicking Add.", true);
        }
        else
        {
            PushUndoSnapshot();
            page.dynamicReticleBindings.push_back(mfd::DynamicReticleLayerBinding {
                draft.templateId,
                draft.layerId,
                NextPageDynamicOrderInLayer(page, draft.layerId)});

            if (activeTutorialTemplate != nullptr)
            {
                tutorial_->CompleteStep();
            }

            RebuildStatus("Dynamic reticle '" + draft.templateId + "' added on layer '" + draft.layerId +
                              "' for page '" + page.name + "'.",
                          false);
        }
    }
    ImGui::EndDisabled();
    ShowItemTooltip("Add the selected reticle template to the page dynamic-reticle list with the chosen layer.");

    if (activeTutorialTemplate != nullptr &&
        mfd::PageNamesEqual(draft.templateId, activeTutorialTemplate->templateId))
    {
        tutorial_->DrawHalo(
            activeTutorialTemplate->targetId.data(),
            activeTutorialTemplate->label,
            activeTutorialTemplate->reason);
    }

    if (page.dynamicReticleBindings.empty())
    {
        ImGui::TextDisabled("No dynamic reticle is configured on this page yet.");
        return;
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Dynamic reticles on this page:");

    std::vector<std::size_t> sortedBindingIndexes(page.dynamicReticleBindings.size());
    std::iota(sortedBindingIndexes.begin(), sortedBindingIndexes.end(), 0U);
    std::sort(sortedBindingIndexes.begin(),
              sortedBindingIndexes.end(),
              [&page](const std::size_t lhsIndex, const std::size_t rhsIndex)
              {
                  const auto& lhs = page.dynamicReticleBindings[lhsIndex];
                  const auto& rhs = page.dynamicReticleBindings[rhsIndex];
                  if (PageLayerOrder(page, lhs.layerId) != PageLayerOrder(page, rhs.layerId))
                  {
                      return PageLayerOrder(page, lhs.layerId) < PageLayerOrder(page, rhs.layerId);
                  }
                  if (lhs.orderInLayer != rhs.orderInLayer)
                  {
                      return lhs.orderInLayer < rhs.orderInLayer;
                  }
                  return lhs.templateId < rhs.templateId;
              });

    for (std::size_t displayIndex = 0; displayIndex < sortedBindingIndexes.size(); ++displayIndex)
    {
        const std::size_t bindingIndex = sortedBindingIndexes[displayIndex];
        mfd::DynamicReticleLayerBinding& binding = page.dynamicReticleBindings[bindingIndex];

        ImGui::PushID(static_cast<int>(bindingIndex));
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("%s", binding.templateId.c_str());
        ImGui::TextDisabled("orderInLayer %d", binding.orderInLayer);

        if (ImGui::BeginCombo("Layer", binding.layerId.c_str()))
        {
            for (const mfd::PageLayerDefinition& layer : page.layers)
            {
                const bool selected = binding.layerId == layer.id;
                if (ImGui::Selectable(layer.id.c_str(), selected))
                {
                    PushUndoSnapshot();
                    binding.layerId = layer.id;
                    binding.orderInLayer =
                        NextPageDynamicOrderInLayer(page, binding.layerId, static_cast<int>(bindingIndex));
                }
                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ShowItemTooltip("Choose which page layer owns runtime instances of this dynamic reticle type.");

        ImGui::SameLine();
        if (ImGui::Button("Remove"))
        {
            PushUndoSnapshot();
            const std::string removedTemplateId = binding.templateId;
            page.dynamicReticleBindings.erase(page.dynamicReticleBindings.begin() + static_cast<std::ptrdiff_t>(bindingIndex));
            RebuildStatus("Dynamic reticle '" + removedTemplateId + "' removed from page '" + page.name + "'.", false);
            ImGui::PopID();
            break;
        }
        ShowItemTooltip("Remove this dynamic reticle type from the page.");

        std::vector<std::size_t> siblingIndexes;
        for (std::size_t siblingIndex : sortedBindingIndexes)
        {
            if (mfd::PageNamesEqual(page.dynamicReticleBindings[siblingIndex].layerId, binding.layerId))
            {
                siblingIndexes.push_back(siblingIndex);
            }
        }

        const auto siblingIterator = std::find(siblingIndexes.begin(), siblingIndexes.end(), bindingIndex);
        const bool hasPreviousSibling = siblingIterator != siblingIndexes.begin() && siblingIterator != siblingIndexes.end();
        const bool hasNextSibling = siblingIterator != siblingIndexes.end() && std::next(siblingIterator) != siblingIndexes.end();

        ImGui::BeginDisabled(!hasPreviousSibling);
        if (ImGui::Button("Move earlier"))
        {
            PushUndoSnapshot();
            mfd::DynamicReticleLayerBinding& previous = page.dynamicReticleBindings[*std::prev(siblingIterator)];
            std::swap(binding.orderInLayer, previous.orderInLayer);
        }
        ImGui::EndDisabled();
        ShowItemTooltip("Swap orderInLayer with the previous dynamic binding on the same runtime layer.");

        ImGui::SameLine();
        ImGui::BeginDisabled(!hasNextSibling);
        if (ImGui::Button("Move later"))
        {
            PushUndoSnapshot();
            mfd::DynamicReticleLayerBinding& next = page.dynamicReticleBindings[*std::next(siblingIterator)];
            std::swap(binding.orderInLayer, next.orderInLayer);
        }
        ImGui::EndDisabled();
        ShowItemTooltip("Swap orderInLayer with the next dynamic binding on the same runtime layer.");

        ImGui::PopID();
    }

}

void EditorApplication::DrawPageStrobeInspector(mfd::PageDefinition& page)
{
    using editor::tutorial::TutorialStepId;

    constexpr std::string_view kTutorialStrobeTargetId = "page_strobe_template";
    constexpr std::string_view kTutorialStrobeReticleId = "tutorial_strobe";

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.72f, 0.86f, 0.95f, 1.0f), "Strobe");
    ImGui::TextDisabled("Assign one library reticle template as the optional page strobe.");
    ImGui::TextDisabled("Each page can expose at most one strobe.");

    std::vector<std::string> templateIds;
    templateIds.reserve(loaded_.document.reticleLibrary.size());
    for (const auto& entry : loaded_.document.reticleLibrary)
    {
        templateIds.push_back(entry.first);
    }
    std::sort(templateIds.begin(), templateIds.end());

    const std::string currentTemplateId =
        page.strobe.has_value() ? page.strobe->reticle.sourceTemplateId : std::string {};
    const char* currentTemplateLabel =
        !page.strobe.has_value() ? "None" : (currentTemplateId.empty() ? "<custom strobe>" : currentTemplateId.c_str());

    if (ImGui::BeginCombo("Strobe template", currentTemplateLabel))
    {
        const bool noneSelected = !page.strobe.has_value();
        if (ImGui::Selectable("None", noneSelected) && page.strobe.has_value())
        {
            PushUndoSnapshot();
            page.strobe.reset();
            RebuildStatus("Strobe removed from page '" + page.name + "'.", false);
        }
        if (noneSelected)
        {
            ImGui::SetItemDefaultFocus();
        }

        for (const std::string& templateId : templateIds)
        {
            const bool selected = page.strobe.has_value() && currentTemplateId == templateId;
            if (ImGui::Selectable(templateId.c_str(), selected) && !selected)
            {
                if (tutorial_->MatchesTarget(kTutorialStrobeTargetId) && templateId != kTutorialStrobeCursorTemplateId)
                {
                    RebuildStatus("Tutorial: choose 'mfd_tutorial_strobe_cursor' as the Page1 strobe template.", true);
                    continue;
                }

                const auto iterator = loaded_.document.reticleLibrary.find(templateId);
                if (iterator != loaded_.document.reticleLibrary.end())
                {
                    PushUndoSnapshot();
                    page.strobe = MakePageStrobeFromTemplate(page, iterator->second, page.strobe);
                    if (tutorial_->MatchesTarget(kTutorialStrobeTargetId))
                    {
                        page.strobe->reticle.id = std::string(kTutorialStrobeReticleId);
                    }
                    RefreshBlinkBindingForEditor(page, page.strobe->reticle.blink);
                    RebuildStatus("Strobe template '" + templateId + "' assigned to page '" + page.name + "'.", false);
                    if (tutorial_->MatchesTarget(kTutorialStrobeTargetId))
                    {
                        tutorial_->CompleteStep();
                    }
                }
            }
            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }

        ImGui::EndCombo();
    }
    ShowItemTooltip("Choose which shared reticle template is instantiated as the page strobe.");
    tutorial_->DrawHalo(
        kTutorialStrobeTargetId.data(),
        "Choose mfd_tutorial_strobe_cursor",
        "Assign the cross cursor as the real Page1 strobe before the tutorial client starts driving it.");

    ImGui::BeginDisabled(!page.strobe.has_value());
    if (ImGui::Button("Remove strobe") && page.strobe.has_value())
    {
        PushUndoSnapshot();
        page.strobe.reset();
        RebuildStatus("Strobe removed from page '" + page.name + "'.", false);
    }
    ImGui::EndDisabled();
    ShowItemTooltip("Remove the optional strobe definition from this page.");

    if (!page.strobe.has_value())
    {
        if (templateIds.empty())
        {
            ImGui::TextDisabled("No library reticle is available yet. Create one first.");
        }
        else
        {
            ImGui::TextDisabled("No strobe assigned to this page.");
        }
        return;
    }

    ImGui::TextDisabled("Strobe id: %s", page.strobe->reticle.id.c_str());
    if (!currentTemplateId.empty())
    {
        ImGui::TextDisabled("Source template: %s", currentTemplateId.c_str());
    }

    const char* captureShapeLabel =
        page.strobe->capture.shape == mfd::StrobeCaptureShape::Circle ? "Circle" : "Rectangle";
    if (ImGui::BeginCombo("Capture shape", captureShapeLabel))
    {
        const bool circleSelected = page.strobe->capture.shape == mfd::StrobeCaptureShape::Circle;
        if (ImGui::Selectable("Circle", circleSelected) && !circleSelected)
        {
            PushUndoSnapshot();
            page.strobe->capture.shape = mfd::StrobeCaptureShape::Circle;
        }
        if (circleSelected)
        {
            ImGui::SetItemDefaultFocus();
        }

        const bool rectangleSelected = page.strobe->capture.shape == mfd::StrobeCaptureShape::Rectangle;
        if (ImGui::Selectable("Rectangle", rectangleSelected) && !rectangleSelected)
        {
            PushUndoSnapshot();
            page.strobe->capture.shape = mfd::StrobeCaptureShape::Rectangle;
        }
        if (rectangleSelected)
        {
            ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }
    ShowItemTooltip("Choose the capture area shape used by the page strobe.");

    if (page.strobe->capture.shape == mfd::StrobeCaptureShape::Circle)
    {
        if (ImGui::DragFloat("Capture radius", &page.strobe->capture.radius, 0.002f, 0.001f, 1.0f, "%.4f"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            page.strobe->capture.radius = std::max(0.001f, page.strobe->capture.radius);
        }
        ShowItemTooltip("Radius of the circular capture area around the strobe.");
    }
    else
    {
        if (ImGui::DragFloat2("Capture size", &page.strobe->capture.size.x, 0.002f, 0.001f, 2.0f, "%.4f"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            page.strobe->capture.size.x = std::max(0.001f, page.strobe->capture.size.x);
            page.strobe->capture.size.y = std::max(0.001f, page.strobe->capture.size.y);
        }
        ShowItemTooltip("Width and height of the rectangular capture area around the strobe.");
    }

    bool magnetEnabled = page.strobe->magnet.enabled;
    if (ImGui::Checkbox("Magnet enabled", &magnetEnabled))
    {
        PushUndoSnapshot();
        page.strobe->magnet.enabled = magnetEnabled;
    }
    ShowItemTooltip("Enable attraction toward nearby dynamic reticles when the strobe moves.");

    if (page.strobe->magnet.enabled)
    {
        if (ImGui::DragFloat("Magnet radius", &page.strobe->magnet.radius, 0.002f, 0.001f, 1.0f, "%.4f"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            page.strobe->magnet.radius = std::max(0.001f, page.strobe->magnet.radius);
        }
        ShowItemTooltip("Maximum distance used to snap the strobe toward nearby targets.");

        if (ImGui::DragFloat("Magnet strength", &page.strobe->magnet.strength, 0.01f, 0.0f, 1.0f, "%.2f"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            page.strobe->magnet.strength = std::clamp(page.strobe->magnet.strength, 0.0f, 1.0f);
        }
        ShowItemTooltip("Blend factor applied when the strobe is attracted toward one target.");

        bool visualShapeEnabled = page.strobe->magnet.visualShapeEnabled;
        if (ImGui::Checkbox("Change visual shape while magnetized", &visualShapeEnabled))
        {
            PushUndoSnapshot();
            page.strobe->magnet.visualShapeEnabled = visualShapeEnabled;
        }
        ShowItemTooltip("Optional visual cue only: the authored strobe reticle is kept unless this is enabled.");

        ImGui::BeginDisabled(!page.strobe->magnet.visualShapeEnabled);
        const char* visualShapeLabel =
            page.strobe->magnet.visualShape == mfd::StrobeMagnetVisualShape::Circle ? "Circle" : "Square";
        if (ImGui::BeginCombo("Magnetized visual shape", visualShapeLabel))
        {
            const bool circleSelected = page.strobe->magnet.visualShape == mfd::StrobeMagnetVisualShape::Circle;
            if (ImGui::Selectable("Circle", circleSelected) && !circleSelected)
            {
                PushUndoSnapshot();
                page.strobe->magnet.visualShape = mfd::StrobeMagnetVisualShape::Circle;
            }
            if (circleSelected)
            {
                ImGui::SetItemDefaultFocus();
            }

            const bool squareSelected = page.strobe->magnet.visualShape == mfd::StrobeMagnetVisualShape::Square;
            if (ImGui::Selectable("Square", squareSelected) && !squareSelected)
            {
                PushUndoSnapshot();
                page.strobe->magnet.visualShape = mfd::StrobeMagnetVisualShape::Square;
            }
            if (squareSelected)
            {
                ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }
        ShowItemTooltip("Shape used only while the runtime reports this strobe as magnetized.");

        if (ImGui::DragFloat("Magnetized visual size", &page.strobe->magnet.visualShapeSize, 0.002f, 0.001f, 1.0f, "%.4f"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            page.strobe->magnet.visualShapeSize = std::max(0.001f, page.strobe->magnet.visualShapeSize);
        }
        ShowItemTooltip("Logical diameter/side length of the optional magnetized visual cue.");
        ImGui::EndDisabled();
    }
}

void EditorApplication::DrawPageLayerInspector(mfd::PageDefinition& page)
{
    BootstrapEditorLayersForPage(page);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.82f, 0.73f, 0.94f, 1.0f), "Page layers");
    ImGui::TextDisabled("Page layers drive runtime draw order. Visibility below only affects the editor preview.");

    if (AccentButton("Add layer"))
    {
        const bool tutorialRadarTrackLayerMatched = tutorial_->MatchesTarget("inspector_add_radar_track_layer");
        const bool tutorialAddLayerMatched = tutorial_->MatchesTarget("inspector_add_layer");
        if (tutorialRadarTrackLayerMatched && mfd::FindPageLayerDefinition(page, "RadarTrackLayer") != nullptr)
        {
            tutorial_->CompleteStep();
            RebuildStatus("RadarTrackLayer is already present on page '" + page.name + "'.", false);
            return;
        }

        PushUndoSnapshot();
        const std::string newLayerId = tutorialRadarTrackLayerMatched ? std::string {"RadarTrackLayer"}
                                                                      : MakeUniqueLayerId(page, "layer");
        page.layers.push_back(mfd::PageLayerDefinition {newLayerId});
        page.editor.layers.push_back(mfd::EditorLayerDefinition {newLayerId, true});
        if (tutorialRadarTrackLayerMatched)
        {
            tutorial_->CompleteStep();
        }
        else if (tutorialAddLayerMatched)
        {
            tutorial_->SetFocusLayerId(page.layers.back().id);
            tutorial_->AdvancePhase();
        }
        RebuildStatus("Runtime layer added to page '" + page.name + "'.", false);
    }
    ShowItemTooltip("Create one new runtime page layer. The editor also tracks its temporary visibility state.");
    tutorial_->DrawHalo(
        "inspector_add_radar_track_layer",
        "Create RadarTrackLayer",
        "Create one dedicated runtime layer for Page1 radar tracks. The existing steering cue already stays on its own layer.");
    tutorial_->DrawHalo(
        "inspector_add_layer",
        "Click Add layer",
        "Create one extra page layer so the tutorial can show how layer-based authoring visibility works.");

    if (page.layers.empty())
    {
        ImGui::TextDisabled("No runtime layer exists on this page yet.");
        return;
    }

    for (std::size_t index = 0; index < page.layers.size(); ++index)
    {
        mfd::PageLayerDefinition& layer = page.layers[index];
        mfd::EditorLayerDefinition& editorLayer = page.editor.layers[index];
        const std::size_t assignedReticles = CountEditorLayerAssignments(page, layer.id);
        const std::size_t assignedBindings = CountDynamicLayerBindings(page, layer.id);
        const std::string dynamicBindingSummary = SummarizeDynamicLayerBindings(page, layer.id);

        ImGui::PushID(static_cast<int>(index));
        ImGui::Spacing();
        ImGui::Separator();

        bool visible = editorLayer.visible;
        if (ImGui::Checkbox("Visible", &visible))
        {
            PushUndoSnapshot();
            editorLayer.visible = visible;
            SanitizePageReticleSelectionForCurrentFocus();
            if (tutorial_->MatchesTarget("inspector_layer_visibility") &&
                layer.id == tutorial_->FocusLayerId() &&
                !editorLayer.visible)
            {
                tutorial_->CompleteStep();
            }
        }
        ShowItemTooltip("Show or hide this layer in the editor preview only.");
        if (layer.id == tutorial_->FocusLayerId())
        {
            tutorial_->DrawHalo(
                "inspector_layer_visibility",
                "Click Visible",
                "Hide the layer you just created to confirm that editor visibility stays separate from runtime layer order.");
        }

        ImGui::SameLine();
        ImGui::TextDisabled("%zu reticle%s | %zu dynamic binding%s",
                            assignedReticles,
                            assignedReticles == 1U ? "" : "s",
                            assignedBindings,
                            assignedBindings == 1U ? "" : "s");

        if (!dynamicBindingSummary.empty())
        {
            ImGui::TextDisabled("Dynamic reticles: %s", dynamicBindingSummary.c_str());
        }

        std::array<char, 128> layerName {};
        CopyTextBuffer(layerName, layer.id);
        const bool nameChanged = ImGui::InputText("Layer id", layerName.data(), layerName.size());
        ShowItemTooltip("Unique layer identifier used by page reticles on this page.");
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        if (nameChanged)
        {
            const std::string nextId = layerName.data();
            const bool duplicateId = std::any_of(
                page.layers.begin(),
                page.layers.end(),
                [&layer, &nextId](const mfd::PageLayerDefinition& candidate)
                {
                    return &candidate != &layer && candidate.id == nextId;
                });

            if (nextId.empty())
            {
                RebuildStatus("Layer id cannot be empty.", true);
            }
            else if (duplicateId)
            {
                RebuildStatus("Layer ids must stay unique inside one page.", true);
            }
            else if (nextId != layer.id)
            {
                const std::string previousId = layer.id;
                layer.id = nextId;
                editorLayer.id = nextId;
                RenameEditorLayerReferences(page, previousId, layer.id);
                if (layerFocusState_.focusedLayerId == previousId)
                {
                    layerFocusState_.focusedLayerId = layer.id;
                }
                SanitizePageReticleSelectionForCurrentFocus();
            }
        }

        ImGui::BeginDisabled(page.layers.size() <= 1U || assignedReticles > 0U || assignedBindings > 0U);
        if (ImGui::Button("Remove layer"))
        {
            const std::string removedLayerId = layer.id;
            PushUndoSnapshot();
            page.layers.erase(page.layers.begin() + static_cast<std::ptrdiff_t>(index));
            page.editor.layers.erase(page.editor.layers.begin() + static_cast<std::ptrdiff_t>(index));
            SanitizeLayerFocusForActivePage();
            SanitizePageReticleSelectionForCurrentFocus();
            RebuildStatus("Runtime layer '" + removedLayerId + "' removed from page '" + page.name + "'.", false);

            ImGui::PopID();
            break;
        }
        ImGui::EndDisabled();
        ShowItemTooltip("Delete one unused runtime layer. A layer cannot be removed while reticles or dynamic bindings still reference it.");

        ImGui::PopID();
    }
}

void EditorApplication::DrawPageBlinkInspector(mfd::PageDefinition& page)
{
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.96f, 0.81f, 0.52f, 1.0f), "Blink types");
    ImGui::TextDisabled("Blink names are page-local. Same effective duration means same phase.");

    if (AccentButton("Add blink type"))
    {
        PushUndoSnapshot();

        mfd::PageBlinkDefinition blinkType;
        blinkType.name = MakeUniqueBlinkTypeName(page, "blink");
        blinkType.normalizedName = mfd::NormalizePageName(blinkType.name);
        blinkType.durationMs = 1000;
        page.blinkTypes.push_back(std::move(blinkType));

        if (page.blinkTypes.size() == 1)
        {
            page.defaultBlinkTypeName = page.blinkTypes.front().name;
            page.normalizedDefaultBlinkTypeName = page.blinkTypes.front().normalizedName;
        }

        RefreshPageBlinkStateForEditor(page);
        RebuildStatus("Blink type added to page '" + page.name + "'.", false);
    }
    ShowItemTooltip("Create a named blink rhythm that page reticles can reference.");

    if (page.blinkTypes.empty())
    {
        ImGui::TextDisabled("No blink type defined on this page yet.");
        return;
    }

    const std::size_t effectiveDefaultIndex = EffectiveDefaultBlinkTypeIndex(page);
    const std::string defaultBlinkPreview =
        effectiveDefaultIndex == kInvalidBlinkTypeIndex ? std::string {"<none>"} : page.blinkTypes[effectiveDefaultIndex].name;

    if (ImGui::BeginCombo("Default blink", defaultBlinkPreview.c_str()))
    {
        for (std::size_t index = 0; index < page.blinkTypes.size(); ++index)
        {
            const bool selected = index == effectiveDefaultIndex;
            if (ImGui::Selectable(page.blinkTypes[index].name.c_str(), selected))
            {
                PushUndoSnapshot();
                page.defaultBlinkTypeName = page.blinkTypes[index].name;
                page.normalizedDefaultBlinkTypeName = page.blinkTypes[index].normalizedName;
                RefreshPageBlinkStateForEditor(page);
            }

            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }

        ImGui::EndCombo();
    }
    ShowItemTooltip("Fallback blink used when a reticle enables blink without choosing a named type.");

    ImGui::TextDisabled("Used when a reticle enables blink without choosing an explicit type.");

    for (std::size_t index = 0; index < page.blinkTypes.size(); ++index)
    {
        auto& blinkType = page.blinkTypes[index];

        ImGui::PushID(static_cast<int>(index));
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Blink %d", static_cast<int>(index) + 1);
        if (index == effectiveDefaultIndex)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("(default)");
        }

        std::array<char, 128> blinkName {};
        CopyTextBuffer(blinkName, blinkType.name);
        const bool nameChanged = ImGui::InputText("Name", blinkName.data(), blinkName.size());
        ShowItemTooltip("Display name stored in the page JSON for this blink definition.");
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        if (nameChanged)
        {
            const std::string nextName = blinkName.data();
            const std::string nextNormalizedName = mfd::NormalizePageName(nextName);
            if (nextNormalizedName.empty())
            {
                RebuildStatus("Blink type name cannot be empty.", true);
            }
            else if (const std::size_t existingIndex = FindBlinkTypeIndex(page, nextNormalizedName);
                     existingIndex != kInvalidBlinkTypeIndex && existingIndex != index)
            {
                RebuildStatus("Blink type names must be unique inside one page.", true);
            }
            else if (nextName != blinkType.name)
            {
                const std::string previousNormalizedName = blinkType.normalizedName;
                blinkType.name = nextName;
                blinkType.normalizedName = nextNormalizedName;
                RenameBlinkReferences(page, previousNormalizedName, blinkType.name);
                RefreshPageBlinkStateForEditor(page);
            }
        }

        int durationMs = static_cast<int>(blinkType.durationMs);
        const bool durationChanged = ImGui::DragInt("Duration (ms)", &durationMs, 10.0f, 1, 60000);
        ShowItemTooltip("Blink cycle duration in milliseconds.");
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        if (durationChanged)
        {
            blinkType.durationMs = static_cast<std::uint32_t>(std::max(1, durationMs));
            RefreshPageBlinkStateForEditor(page);
        }

        if (ImGui::Button("Remove blink type"))
        {
            const std::string removedName = blinkType.name;
            const std::string removedNormalizedName = blinkType.normalizedName;
            const std::size_t clearedBindingCount = CountBlinkReferences(page, removedNormalizedName);
            const bool removedWasDefault = page.normalizedDefaultBlinkTypeName == removedNormalizedName;

            PushUndoSnapshot();
            page.blinkTypes.erase(page.blinkTypes.begin() + static_cast<std::ptrdiff_t>(index));
            ClearBlinkReferencesForRemovedType(page, removedNormalizedName);

            if (removedWasDefault && !page.blinkTypes.empty())
            {
                page.defaultBlinkTypeName = page.blinkTypes.front().name;
                page.normalizedDefaultBlinkTypeName = page.blinkTypes.front().normalizedName;
            }

            RefreshPageBlinkStateForEditor(page);

            std::string status = "Blink type '" + removedName + "' removed from page '" + page.name + "'.";
            if (clearedBindingCount > 0)
            {
                status += " Cleared " + std::to_string(clearedBindingCount) + " explicit binding(s).";
            }
            RebuildStatus(status, false);

            ImGui::PopID();
            break;
        }
        ShowItemTooltip("Delete this blink definition and clear page reticles that referenced it.");

        ImGui::PopID();
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

        if (AccentButton("Copy selection"))
        {
            CopySelectedPageReticles();
        }
        ShowItemTooltip("Copy all selected page reticle instances.");

        ImGui::SameLine();
        if (ImGui::Button("Cut selection"))
        {
            CutSelectedPageReticles();
            return;
        }
        ShowItemTooltip("Copy all selected page reticle instances, then remove them from the page.");

        ImGui::SameLine();
        if (ImGui::Button("Delete from page"))
        {
            DeleteSelection();
            return;
        }
        ShowItemTooltip("Delete all selected reticles from the active page.");

        ImGui::SameLine();
        ImGui::BeginDisabled(pageReticleClipboard_.empty());
        if (ImGui::Button("Paste copies"))
        {
            PasteCopiedPageReticles();
            ImGui::EndDisabled();
            return;
        }
        ShowItemTooltip("Paste copied page reticles onto the active page.");
        ImGui::EndDisabled();

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
    ShowItemTooltip("Replace the current selection with one reusable reticle template staged in the shared library.");
    tutorial_->DrawHalo(
        "page_reticle_extract",
        "Click Extract as reticle...",
        "Open the extraction workflow to review how page content can become one reusable library template.");

        ImGui::TextDisabled("Shortcuts: Ctrl+C, Ctrl+X, Ctrl+V, Suppr, Esc");
        ImGui::TextDisabled("Drag one selected reticle in the preview to move the whole group.");
        ImGui::TextDisabled("Direct property editing stays available when a single reticle is selected.");
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
    ImGui::Text("Reticle id: %s", reticle->id.c_str());
    if (!reticle->sourceTemplateId.empty())
    {
        ImGui::TextDisabled("Template: %s", reticle->sourceTemplateId.c_str());
    }
    ImGui::TextDisabled("Move inside the frame, rotate with the blue handle, scale with the corner handles.");
    ImGui::Separator();

    const int reticleIndex = selection_.pageReticleIndex;
    const int lastReticleIndex = static_cast<int>(page->staticReticles.size()) - 1;
    auto moveSelectedReticleTo = [&](int targetIndex, const char* action)
    {
        if (reticleIndex < 0 ||
            reticleIndex >= static_cast<int>(page->staticReticles.size()) ||
            targetIndex < 0 ||
            targetIndex >= static_cast<int>(page->staticReticles.size()) ||
            targetIndex == reticleIndex)
        {
            return;
        }

        PushUndoSnapshot();
        const std::string movedReticleId = reticle->id;

        auto movedReticle =
            std::move(page->staticReticles[static_cast<std::size_t>(reticleIndex)]);
        page->staticReticles.erase(page->staticReticles.begin() + reticleIndex);

        const int insertionIndex =
            std::clamp(targetIndex, 0, static_cast<int>(page->staticReticles.size()));

        page->staticReticles.insert(page->staticReticles.begin() + insertionIndex, std::move(movedReticle));
        SelectPageReticle(selection_.pageIndex, insertionIndex);
        RebuildStatus("Reticle '" + movedReticleId + "' moved " + action + " on page '" + page->name + "'.", false);
    };

    if (ImGui::Button("Delete from page"))
    {
        DeleteSelection();
        return;
    }
    ShowItemTooltip("Delete this reticle instance from the active page.");

    ImGui::SameLine();
    if (AccentButton("Copy"))
    {
        CopySelectedPageReticles();
    }
    ShowItemTooltip("Copy this page reticle instance.");

    ImGui::SameLine();
    if (ImGui::Button("Cut"))
    {
        CutSelectedPageReticles();
        return;
    }
    ShowItemTooltip("Copy this page reticle instance, then remove it from the page.");

    ImGui::SameLine();
    ImGui::BeginDisabled(pageReticleClipboard_.empty());
    if (ImGui::Button("Paste copies"))
    {
        PasteCopiedPageReticles();
        ImGui::EndDisabled();
        return;
    }
    ShowItemTooltip("Paste copied page reticles onto the active page.");
    ImGui::EndDisabled();

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

    if (!reticle->sourceTemplateId.empty() &&
        loaded_.document.reticleLibrary.find(reticle->sourceTemplateId) != loaded_.document.reticleLibrary.end() &&
        ImGui::Button("Edit source template"))
    {
        SelectLibraryReticle(reticle->sourceTemplateId);
        RebuildStatus("Editing template '" + reticle->sourceTemplateId + "' in the reticle studio.", false);
        return;
    }
    if (!reticle->sourceTemplateId.empty() && loaded_.document.reticleLibrary.find(reticle->sourceTemplateId) != loaded_.document.reticleLibrary.end())
    {
        ShowItemTooltip("Open the shared template that this page reticle instance was created from.");
    }

    if (!reticle->sourceTemplateId.empty() && loaded_.document.reticleLibrary.find(reticle->sourceTemplateId) != loaded_.document.reticleLibrary.end())
    {
        ImGui::SameLine();
    }

    ImGui::TextDisabled("Shortcut: Suppr");
    ImGui::TextDisabled("Cut / copy / paste: Ctrl+X / Ctrl+C / Ctrl+V");
    ImGui::TextDisabled("Esc clears the current page-reticle selection.");
    ImGui::TextDisabled("Draw order: %d / %d", reticleIndex + 1, std::max(1, static_cast<int>(page->staticReticles.size())));

    const std::string currentLayerLabel = reticle->layerId.empty() ? std::string {"<none>"} : reticle->layerId;
    if (ImGui::BeginCombo("Editor layer", currentLayerLabel.c_str()))
    {
        const bool noLayerSelected = reticle->layerId.empty();
        if (ImGui::Selectable("<none>", noLayerSelected))
        {
            PushUndoSnapshot();
            reticle->layerId.clear();
        }
        if (noLayerSelected)
        {
            ImGui::SetItemDefaultFocus();
        }

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
        SelectPage(selection_.pageIndex);
        RebuildStatus("Layer editor opened for page '" + page->name + "'.", false);
        return;
    }
    ShowItemTooltip("Open the page inspector to edit the available editor-only layers.");
    ImGui::TextDisabled("Hidden layers stay editable in the inspector but are not rendered in the editor preview.");

    const bool canMoveBackward = reticleIndex > 0;
    const bool canMoveForward = reticleIndex >= 0 && reticleIndex < lastReticleIndex;

    ImGui::BeginDisabled(!canMoveBackward);
    if (ImGui::Button("Send to back", ImVec2(130.0f, 0.0f)))
    {
        moveSelectedReticleTo(0, "to the back");
        ImGui::EndDisabled();
        return;
    }
    ShowItemTooltip("Move this reticle to the first draw-order slot on the page.");
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!canMoveBackward);
    if (ImGui::Button("Step back", ImVec2(110.0f, 0.0f)))
    {
        moveSelectedReticleTo(reticleIndex - 1, "backward");
        ImGui::EndDisabled();
        return;
    }
    ShowItemTooltip("Move this reticle one step earlier in the page draw order.");
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!canMoveForward);
    if (ImGui::Button("Step forward", ImVec2(130.0f, 0.0f)))
    {
        moveSelectedReticleTo(reticleIndex + 1, "forward");
        ImGui::EndDisabled();
        return;
    }
    ShowItemTooltip("Move this reticle one step later in the page draw order.");
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!canMoveForward);
    if (ImGui::Button("Bring to front", ImVec2(130.0f, 0.0f)))
    {
        moveSelectedReticleTo(lastReticleIndex, "to the front");
        ImGui::EndDisabled();
        return;
    }
    ShowItemTooltip("Move this reticle to the last draw-order slot on the page.");
    ImGui::EndDisabled();

    ImGui::Separator();

    {
        bool visible = reticle->visible;
        if (ImGui::Checkbox("Visible", &visible))
        {
            PushUndoSnapshot();
            reticle->visible = visible;
        }
        ShowItemTooltip("Toggle whether this page reticle instance is rendered.");
    }

    {
        bool drawOnTop = reticle->drawOnTop;
        if (ImGui::Checkbox("Draw on top", &drawOnTop))
        {
            PushUndoSnapshot();
            reticle->drawOnTop = drawOnTop;
        }
        ShowItemTooltip("Render this page reticle after regular page reticles while keeping the strobe on top.");
    }

    DrawPageReticleBlinkInspector(*page, *reticle);

    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.72f, 0.86f, 0.95f, 1.0f), "Clipping");
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
                    ApplyPageReticleClipping(selection_.pageReticleIndex, reticle->clipping.mode, option.primitiveId);
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
                    ApplyPageReticleClipping(selection_.pageReticleIndex, mode, primitiveId);
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
            std::array<char, 128> format {};
            CopyTextBuffer(format, time->format);
            const std::string formatLabel = "Time format##" + primitive.id;
            const bool formatChanged = ImGui::InputText(formatLabel.c_str(), format.data(), format.size());
            ShowItemTooltip("Override the strftime-style format used by this time primitive.");
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            if (formatChanged)
            {
                time->format = format.data();
            }

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
    if (const auto fileIt = files_.templateFiles.find(reticle->id); fileIt != files_.templateFiles.end())
    {
        ImGui::TextDisabled("File: %s", fileIt->second.filename().string().c_str());
    }
    ImGui::TextDisabled("Drag this reticle from the library tree to the page preview, or edit it directly in the reticle studio.");

    const bool canAddToPage = ActivePage() != nullptr;
    if (!canAddToPage)
    {
        ImGui::BeginDisabled();
    }
    if (AccentButton("Add to active page"))
    {
        const bool tutorialAddMatched = tutorial_->MatchesTarget("library_add_to_page");
        const mfd::PageDefinition* page = ActivePage();
        const mfd::Vec2 dropPosition = page == nullptr ? mfd::Vec2 {} : pagePreviewView_.center;
        std::string tutorialError;
        if (tutorialAddMatched && !tutorial_->ValidateAddToPage(page, *reticle, tutorialError))
        {
            RebuildStatus(tutorialError, true);
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
    ShowItemTooltip("Instantiate this template on the active page at the current editor camera center.");
    tutorial_->DrawHalo(
        "library_add_to_page",
        "Click Add to active page",
        tutorial_->LibraryAddToPageHaloReason().data());
    if (!canAddToPage)
    {
        ImGui::EndDisabled();
    }

    ImGui::SameLine();
    if (ImGui::Button("Rename reticle globally..."))
    {
        OpenReticleRenamePopup(reticle->id);
        return;
    }
    ShowItemTooltip("Rename this shared reticle template safely across the current asset tree and every page that references it.");

    ImGui::SameLine();
    if (ImGui::Button("Delete library reticle"))
    {
        DeleteSelectedLibraryReticle();
        return;
    }
    ShowItemTooltip("Delete this shared reticle template from the library.");

    ImGui::Separator();

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

    ImGui::Separator();
    ImGui::TextDisabled("Primitives");
    ImGui::TextDisabled("Click a primitive below or directly in the studio preview to focus and edit it.");

    ImGui::BeginChild("PrimitiveCatalog", ImVec2(0.0f, 210.0f), true);
    for (int index = 0; index < static_cast<int>(reticle->primitives.size()); ++index)
    {
        const auto& primitive = reticle->primitives[static_cast<std::size_t>(index)];
        const bool selected = selection_.kind == SelectionKind::LibraryPrimitive &&
                              selection_.libraryReticleId == reticle->id &&
                              selection_.primitiveIndex == index;
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

    if (ImGui::BeginCombo("Add primitive", PrimitiveTypeLabel(kPrimitiveTypes[static_cast<std::size_t>(newLibraryReticleDraft_.primitiveTypeIndex)]).c_str()))
    {
        for (int index = 0; index < static_cast<int>(kPrimitiveTypes.size()); ++index)
        {
            if (ImGui::Selectable(PrimitiveTypeLabel(kPrimitiveTypes[static_cast<std::size_t>(index)]).c_str(),
                                  newLibraryReticleDraft_.primitiveTypeIndex == index))
            {
                newLibraryReticleDraft_.primitiveTypeIndex = index;
            }
        }
        ImGui::EndCombo();
    }
    ShowItemTooltip("Choose the primitive type that will be appended to this reticle.");

    if (AccentButton("Append primitive"))
    {
        const bool tutorialAppendMatched = tutorial_->MatchesTarget("library_append_primitive");
        const mfd::PrimitiveType primitiveType =
            kPrimitiveTypes[static_cast<std::size_t>(newLibraryReticleDraft_.primitiveTypeIndex)];
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
        tutorial_->LibraryAppendPrimitiveHaloReason().data());

    ImGui::SameLine();
    if (ImGui::Button("Remove selected primitive"))
    {
        mfd::Primitive* primitive = SelectedLibraryPrimitive();
        if (primitive != nullptr)
        {
            PushUndoSnapshot();
            reticle->primitives.erase(reticle->primitives.begin() + selection_.primitiveIndex);
            selection_.kind = SelectionKind::LibraryReticle;
            selection_.primitiveIndex = -1;
        }
    }
    ShowItemTooltip("Delete the currently selected primitive from this reticle.");

}

void EditorApplication::DrawLibraryPrimitiveInspector()
{
    mfd::Primitive* primitive = SelectedLibraryPrimitive();
    if (primitive == nullptr)
    {
        ImGui::TextDisabled("Select a primitive inside a library reticle.");
        return;
    }

    const ScopedImGuiId scopedId("LibraryPrimitiveInspector");

    ImGui::TextColored(ImVec4(0.33f, 0.86f, 0.78f, 1.0f), "Primitive");
    ImGui::TextDisabled("Green handle moves the primitive. Orange handles edit geometry directly in the studio.");
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
        const bool tutorialProgressFillSelected =
            tutorial_->IsExposedPrimitiveTutorialSelection(selection_.libraryReticleId, primitive->id);
        if (ImGui::Checkbox("Exposed", &exposed))
        {
            PushUndoSnapshot();
            primitive->exposed = exposed;

            if (exposed && tutorial_->MatchesTarget("primitive_exposed_checkbox") && tutorialProgressFillSelected)
            {
                tutorial_->CompleteStep();
            }
        }
        ShowItemTooltip("Expose this primitive through the generated client API so runtime code can drive it directly.");
        if (tutorialProgressFillSelected)
        {
            tutorial_->DrawHalo(
                "primitive_exposed_checkbox",
                "Enable Exposed",
                "Expose the fill rectangle so the generated API can animate the progress bar without raw ids.");
        }
    }

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

    ImVec4 fill = ToImGuiColor(primitive->style.fillColor);
    if (ImGui::ColorEdit4("Fill", &fill.x))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        primitive->style.fillColor = ToColorRgba(fill);
    }
    ShowItemTooltip("Fill color used when this primitive supports filled rendering.");

    {
        bool filled = primitive->style.filled;
        if (ImGui::Checkbox("Filled", &filled))
        {
            PushUndoSnapshot();
            primitive->style.filled = filled;
        }
        ShowItemTooltip("Toggle filled rendering for primitives that support it.");
    }

    auto editPointArray = [&](const char* label, mfd::Vec2* value)
    {
        if (ImGui::DragFloat2(label, &value->x, 0.01f, -1.0f, 1.0f, "%.3f"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
        }
    };

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
        std::array<char, 128> buffer {};
        CopyTextBuffer(buffer, time->format);
        const bool changed = ImGui::InputText("Format", buffer.data(), buffer.size());
        ShowItemTooltip("strftime-style format string used by this time primitive.");
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        if (changed)
        {
            time->format = buffer.data();
        }

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
        editPointArray("Start", &line->start);
        editPointArray("End", &line->end);
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
        editPointArray("Point A", &triangle->points[0]);
        editPointArray("Point B", &triangle->points[1]);
        editPointArray("Point C", &triangle->points[2]);
        return;
    }

    if (auto* polyline = std::get_if<mfd::PolylineGeometry>(&primitive->geometry))
    {
        bool closed = polyline->closed;
        if (ImGui::Checkbox("Closed", &closed))
        {
            PushUndoSnapshot();
            polyline->closed = closed;
        }
        ShowItemTooltip("Close the polyline by linking the last point back to the first.");

        for (int index = 0; index < static_cast<int>(polyline->points.size()); ++index)
        {
            const std::string label = "Point " + std::to_string(index + 1);
            editPointArray(label.c_str(), &polyline->points[static_cast<std::size_t>(index)]);
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
            editPointArray(label.c_str(), &bezier->controlPoints[static_cast<std::size_t>(index)]);
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
