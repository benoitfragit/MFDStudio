/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Shared helper functions used by editor automation services and tests.
 */

#include "EditorAutomationDocumentUtils.h"

#include <algorithm>
#include <cctype>
#include <unordered_set>

#include "mfd/model/PageName.h"
#include "mfd/model/Reticle.h"

namespace editor::automation
{
namespace
{
constexpr std::string_view kWindowPrefix = "window:";
constexpr std::string_view kPagePrefix = "page:";
constexpr std::string_view kReticleAssetPrefix = "reticle-asset:";
constexpr std::string_view kPageReticlePrefix = "page-reticle:";
constexpr std::string_view kPrimitiveAssetPrefix = "primitive:reticle-asset:";
constexpr std::string_view kPrimitivePageReticlePrefix = "primitive:page-reticle:";
constexpr std::string_view kLayerPrefix = "layer:";

std::string NormalizeIdentifier(const std::string_view value)
{
    return mfd::NormalizePageName(value);
}

bool MatchesNormalized(const std::string_view lhs, const std::string_view rhs)
{
    return NormalizeIdentifier(lhs) == NormalizeIdentifier(rhs);
}

std::string PrimitiveStableKey(const mfd::Primitive& primitive, const int primitiveIndex)
{
    if (!primitive.id.empty())
    {
        return NormalizeIdentifier(primitive.id);
    }

    return "index:" + std::to_string(primitiveIndex);
}

std::filesystem::path NormalizePath(const std::filesystem::path& path)
{
    return path.empty() ? std::filesystem::path {} : path.lexically_normal();
}

std::string Trimmed(const std::string_view value)
{
    std::string copy(value);
    copy.erase(copy.begin(),
               std::find_if(copy.begin(),
                            copy.end(),
                            [](const unsigned char character)
                            {
                                return !std::isspace(character);
                            }));
    copy.erase(std::find_if(copy.rbegin(),
                            copy.rend(),
                            [](const unsigned char character)
                            {
                                return !std::isspace(character);
                            })
                   .base(),
               copy.end());
    return copy;
}

std::optional<std::string> StripPrefix(const std::string_view value, const std::string_view prefix)
{
    if (value.size() < prefix.size() || value.compare(0, prefix.size(), prefix) != 0)
    {
        return std::nullopt;
    }

    return std::string(value.substr(prefix.size()));
}

std::optional<std::pair<std::string, std::string>> SplitTwoPartId(const std::string_view value, const std::string_view prefix)
{
    const std::optional<std::string> stripped = StripPrefix(value, prefix);
    if (!stripped.has_value())
    {
        return std::nullopt;
    }

    const std::size_t separator = stripped->find('/');
    if (separator == std::string::npos)
    {
        return std::nullopt;
    }

    return std::pair<std::string, std::string> {
        stripped->substr(0, separator),
        stripped->substr(separator + 1U)};
}

std::optional<std::tuple<std::string, std::string, std::string>> SplitThreePartId(const std::string_view value,
                                                                                   const std::string_view prefix)
{
    const std::optional<std::string> stripped = StripPrefix(value, prefix);
    if (!stripped.has_value())
    {
        return std::nullopt;
    }

    const std::size_t separatorA = stripped->find('/');
    if (separatorA == std::string::npos)
    {
        return std::nullopt;
    }

    const std::size_t separatorB = stripped->find('/', separatorA + 1U);
    if (separatorB == std::string::npos)
    {
        return std::nullopt;
    }

    return std::tuple<std::string, std::string, std::string> {
        stripped->substr(0, separatorA),
        stripped->substr(separatorA + 1U, separatorB - separatorA - 1U),
        stripped->substr(separatorB + 1U)};
}

void AppendPrimitiveDiagnostics(const std::vector<mfd::Primitive>& primitives,
                                const std::string_view ownerId,
                                ValidationReport& report)
{
    std::unordered_set<std::string> primitiveIds;
    for (int primitiveIndex = 0; primitiveIndex < static_cast<int>(primitives.size()); ++primitiveIndex)
    {
        const mfd::Primitive& primitive = primitives[static_cast<std::size_t>(primitiveIndex)];
        if (primitive.id.empty())
        {
            continue;
        }

        const std::string normalizedPrimitiveId = NormalizeIdentifier(primitive.id);
        if (!primitiveIds.insert(normalizedPrimitiveId).second)
        {
            report.valid = false;
            report.diagnostics.push_back(ValidationDiagnostic {
                AutomationErrorCode::ValidationFailed,
                std::string(ownerId),
                "Primitive ids must stay unique inside one reticle."});
        }
    }
}
} // namespace

WindowId MakeWindowId(const mfd::LoadedWindowConfiguration& loaded)
{
    const std::filesystem::path windowFile = NormalizePath(loaded.window.sourceFile);
    return WindowId {std::string(kWindowPrefix) + (windowFile.empty() ? std::string {"untitled"} : windowFile.generic_u8string())};
}

PageId MakePageId(const mfd::PageDefinition& page)
{
    return PageId {std::string(kPagePrefix) + NormalizeIdentifier(page.normalizedName.empty() ? page.name : page.normalizedName)};
}

ReticleAssetId MakeReticleAssetId(const std::string_view templateId)
{
    return ReticleAssetId {std::string(kReticleAssetPrefix) + NormalizeIdentifier(templateId)};
}

PageReticleInstanceId MakePageReticleInstanceId(const mfd::PageDefinition& page, const mfd::ReticleGroup& reticle)
{
    return PageReticleInstanceId {std::string(kPageReticlePrefix) + NormalizeIdentifier(page.normalizedName.empty() ? page.name : page.normalizedName) +
                                  "/" + NormalizeIdentifier(reticle.id)};
}

PrimitiveId MakeReticleAssetPrimitiveId(const std::string_view templateId, const mfd::Primitive& primitive, const int primitiveIndex)
{
    return PrimitiveId {std::string(kPrimitiveAssetPrefix) + NormalizeIdentifier(templateId) + "/" +
                        PrimitiveStableKey(primitive, primitiveIndex)};
}

PrimitiveId MakePageReticlePrimitiveId(const mfd::PageDefinition& page,
                                       const mfd::ReticleGroup& reticle,
                                       const mfd::Primitive& primitive,
                                       const int primitiveIndex)
{
    return PrimitiveId {std::string(kPrimitivePageReticlePrefix) +
                        NormalizeIdentifier(page.normalizedName.empty() ? page.name : page.normalizedName) + "/" +
                        NormalizeIdentifier(reticle.id) + "/" + PrimitiveStableKey(primitive, primitiveIndex)};
}

LayerId MakeLayerId(const mfd::PageDefinition& page, const mfd::EditorLayerDefinition& layer)
{
    return LayerId {std::string(kLayerPrefix) + NormalizeIdentifier(page.normalizedName.empty() ? page.name : page.normalizedName) +
                    "/" + NormalizeIdentifier(layer.id)};
}

mfd::PageDefinition* FindPageById(mfd::LoadedWindowConfiguration& loaded, const PageId& pageId, int* pageIndex)
{
    return const_cast<mfd::PageDefinition*>(
        FindPageById(static_cast<const mfd::LoadedWindowConfiguration&>(loaded), pageId, pageIndex));
}

const mfd::PageDefinition* FindPageById(const mfd::LoadedWindowConfiguration& loaded, const PageId& pageId, int* pageIndex)
{
    const std::optional<std::string> stripped = StripPrefix(pageId.value, kPagePrefix);
    if (!stripped.has_value())
    {
        return nullptr;
    }

    for (int index = 0; index < static_cast<int>(loaded.document.pages.size()); ++index)
    {
        const mfd::PageDefinition& page = loaded.document.pages[static_cast<std::size_t>(index)];
        const std::string normalizedName = page.normalizedName.empty() ? NormalizeIdentifier(page.name) : page.normalizedName;
        if (normalizedName == *stripped)
        {
            if (pageIndex != nullptr)
            {
                *pageIndex = index;
            }
            return &page;
        }
    }

    return nullptr;
}

mfd::ReticleGroup* FindReticleAssetById(mfd::LoadedWindowConfiguration& loaded, const ReticleAssetId& reticleId)
{
    return const_cast<mfd::ReticleGroup*>(
        FindReticleAssetById(static_cast<const mfd::LoadedWindowConfiguration&>(loaded), reticleId));
}

const mfd::ReticleGroup* FindReticleAssetById(const mfd::LoadedWindowConfiguration& loaded, const ReticleAssetId& reticleId)
{
    const std::optional<std::string> stripped = StripPrefix(reticleId.value, kReticleAssetPrefix);
    if (!stripped.has_value())
    {
        return nullptr;
    }

    for (const auto& [templateId, reticle] : loaded.document.reticleLibrary)
    {
        if (NormalizeIdentifier(templateId) == *stripped)
        {
            return &reticle;
        }
    }

    return nullptr;
}

mfd::ReticleGroup* FindPageReticleById(mfd::LoadedWindowConfiguration& loaded,
                                       const PageReticleInstanceId& reticleId,
                                       mfd::PageDefinition** page,
                                       int* pageIndex,
                                       int* reticleIndex)
{
    return const_cast<mfd::ReticleGroup*>(
        FindPageReticleById(static_cast<const mfd::LoadedWindowConfiguration&>(loaded),
                            reticleId,
                            const_cast<const mfd::PageDefinition**>(page),
                            pageIndex,
                            reticleIndex));
}

const mfd::ReticleGroup* FindPageReticleById(const mfd::LoadedWindowConfiguration& loaded,
                                             const PageReticleInstanceId& reticleId,
                                             const mfd::PageDefinition** page,
                                             int* pageIndex,
                                             int* reticleIndex)
{
    const std::optional<std::pair<std::string, std::string>> parts = SplitTwoPartId(reticleId.value, kPageReticlePrefix);
    if (!parts.has_value())
    {
        return nullptr;
    }

    for (int candidatePageIndex = 0; candidatePageIndex < static_cast<int>(loaded.document.pages.size()); ++candidatePageIndex)
    {
        const mfd::PageDefinition& candidatePage = loaded.document.pages[static_cast<std::size_t>(candidatePageIndex)];
        const std::string normalizedPageId =
            candidatePage.normalizedName.empty() ? NormalizeIdentifier(candidatePage.name) : candidatePage.normalizedName;
        if (normalizedPageId != parts->first)
        {
            continue;
        }

        for (int candidateReticleIndex = 0; candidateReticleIndex < static_cast<int>(candidatePage.staticReticles.size());
             ++candidateReticleIndex)
        {
            const mfd::ReticleGroup& candidateReticle =
                candidatePage.staticReticles[static_cast<std::size_t>(candidateReticleIndex)];
            if (!MatchesNormalized(candidateReticle.id, parts->second))
            {
                continue;
            }

            if (page != nullptr)
            {
                *page = &candidatePage;
            }
            if (pageIndex != nullptr)
            {
                *pageIndex = candidatePageIndex;
            }
            if (reticleIndex != nullptr)
            {
                *reticleIndex = candidateReticleIndex;
            }
            return &candidateReticle;
        }
    }

    return nullptr;
}

mfd::EditorLayerDefinition* FindLayerById(mfd::LoadedWindowConfiguration& loaded,
                                          const PageId& pageId,
                                          const LayerId& layerId,
                                          int* layerIndex)
{
    return const_cast<mfd::EditorLayerDefinition*>(
        FindLayerById(static_cast<const mfd::LoadedWindowConfiguration&>(loaded), pageId, layerId, layerIndex));
}

const mfd::EditorLayerDefinition* FindLayerById(const mfd::LoadedWindowConfiguration& loaded,
                                                const PageId& pageId,
                                                const LayerId& layerId,
                                                int* layerIndex)
{
    const mfd::PageDefinition* page = FindPageById(loaded, pageId, nullptr);
    if (page == nullptr)
    {
        return nullptr;
    }

    const std::optional<std::pair<std::string, std::string>> parts = SplitTwoPartId(layerId.value, kLayerPrefix);
    if (!parts.has_value())
    {
        return nullptr;
    }

    const std::string normalizedPageId = page->normalizedName.empty() ? NormalizeIdentifier(page->name) : page->normalizedName;
    if (normalizedPageId != parts->first)
    {
        return nullptr;
    }

    for (int index = 0; index < static_cast<int>(page->editor.layers.size()); ++index)
    {
        const mfd::EditorLayerDefinition& layer = page->editor.layers[static_cast<std::size_t>(index)];
        if (MatchesNormalized(layer.id, parts->second))
        {
            if (layerIndex != nullptr)
            {
                *layerIndex = index;
            }
            return &layer;
        }
    }

    return nullptr;
}

void BootstrapEditorLayersForPage(mfd::PageDefinition& page)
{
    if (!page.editor.layers.empty())
    {
        return;
    }

    page.editor.layers.push_back(mfd::EditorLayerDefinition {"layer", true});
    for (auto& reticle : page.staticReticles)
    {
        if (reticle.editor.layerId.empty())
        {
            reticle.editor.layerId = page.editor.layers.front().id;
        }
    }
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

std::string MakeUniqueReticleId(const std::vector<mfd::ReticleGroup>& groups, const std::string_view baseId)
{
    std::string candidate = Trimmed(baseId);
    if (candidate.empty())
    {
        candidate = "reticle";
    }

    const std::string normalizedBase = NormalizeIdentifier(candidate);
    int suffix = 1;
    auto exists = [&groups](const std::string_view candidateId)
    {
        return std::any_of(groups.begin(),
                           groups.end(),
                           [candidateId](const mfd::ReticleGroup& reticle)
                           {
                               return MatchesNormalized(reticle.id, candidateId);
                           });
    };

    while (exists(candidate))
    {
        candidate = normalizedBase + "_" + std::to_string(suffix++);
    }

    return candidate;
}

mfd::ReticleGroup MakePrimitiveReticle(std::string id, const mfd::PrimitiveType primitiveType)
{
    mfd::ReticleGroup reticle;
    reticle.id = std::move(id);

    mfd::Primitive primitive;
    primitive.id = "primitive_01";
    primitive.type = primitiveType;

    switch (primitiveType)
    {
    case mfd::PrimitiveType::Text:
        primitive.geometry = mfd::TextGeometry {"TXT", 0.06f, mfd::kDefaultTextLetterSpacing};
        break;
    case mfd::PrimitiveType::Time:
        primitive.geometry = mfd::TimeGeometry {"%H:%M:%S", false, 0.06f, mfd::kDefaultTextLetterSpacing};
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

DocumentSnapshot BuildDocumentSnapshot(const mfd::LoadedWindowConfiguration& loaded,
                                       const editor::EditorFileLayout& files,
                                       const AutomationUiState& uiState,
                                       const std::optional<AutomationSessionId>& activeSessionId)
{
    DocumentSnapshot snapshot;
    snapshot.hasOpenWindow = !loaded.window.sourceFile.empty();
    snapshot.windowId = MakeWindowId(loaded);
    snapshot.window = loaded.window;
    snapshot.fileLayout = files;
    snapshot.uiState = uiState;
    snapshot.sessionActive = activeSessionId.has_value();
    if (activeSessionId.has_value())
    {
        snapshot.sessionId = *activeSessionId;
    }

    for (int pageIndex = 0; pageIndex < static_cast<int>(loaded.document.pages.size()); ++pageIndex)
    {
        const mfd::PageDefinition& page = loaded.document.pages[static_cast<std::size_t>(pageIndex)];
        PageSnapshot pageSnapshot;
        pageSnapshot.id = MakePageId(page);
        pageSnapshot.index = pageIndex;
        pageSnapshot.sourceFile = pageIndex < static_cast<int>(files.pageFiles.size()) ? files.pageFiles[static_cast<std::size_t>(pageIndex)]
                                                                                       : std::filesystem::path {};
        pageSnapshot.page = page;

        for (int layerIndex = 0; layerIndex < static_cast<int>(page.editor.layers.size()); ++layerIndex)
        {
            const mfd::EditorLayerDefinition& layer = page.editor.layers[static_cast<std::size_t>(layerIndex)];
            pageSnapshot.layers.push_back(LayerSnapshot {MakeLayerId(page, layer), layerIndex, layer});
        }

        for (int reticleIndex = 0; reticleIndex < static_cast<int>(page.staticReticles.size()); ++reticleIndex)
        {
            const mfd::ReticleGroup& reticle = page.staticReticles[static_cast<std::size_t>(reticleIndex)];
            PageReticleInstanceSnapshot reticleSnapshot;
            reticleSnapshot.id = MakePageReticleInstanceId(page, reticle);
            reticleSnapshot.index = reticleIndex;
            reticleSnapshot.reticle = reticle;
            for (int primitiveIndex = 0; primitiveIndex < static_cast<int>(reticle.primitives.size()); ++primitiveIndex)
            {
                const mfd::Primitive& primitive = reticle.primitives[static_cast<std::size_t>(primitiveIndex)];
                reticleSnapshot.primitives.push_back(
                    PrimitiveSnapshot {MakePageReticlePrimitiveId(page, reticle, primitive, primitiveIndex),
                                       AutomationPrimitiveOwnerKind::PageReticleInstance,
                                       reticleSnapshot.id.value,
                                       primitiveIndex,
                                       primitive});
            }
            pageSnapshot.reticles.push_back(std::move(reticleSnapshot));
        }

        snapshot.pages.push_back(std::move(pageSnapshot));
    }

    for (const auto& [templateId, reticle] : loaded.document.reticleLibrary)
    {
        ReticleAssetSnapshot reticleSnapshot;
        reticleSnapshot.id = MakeReticleAssetId(templateId);
        if (const auto iterator = files.templateFiles.find(templateId); iterator != files.templateFiles.end())
        {
            reticleSnapshot.sourceFile = iterator->second;
        }
        reticleSnapshot.reticle = reticle;
        for (int primitiveIndex = 0; primitiveIndex < static_cast<int>(reticle.primitives.size()); ++primitiveIndex)
        {
            const mfd::Primitive& primitive = reticle.primitives[static_cast<std::size_t>(primitiveIndex)];
            reticleSnapshot.primitives.push_back(
                PrimitiveSnapshot {MakeReticleAssetPrimitiveId(templateId, primitive, primitiveIndex),
                                   AutomationPrimitiveOwnerKind::ReticleAsset,
                                   reticleSnapshot.id.value,
                                   primitiveIndex,
                                   primitive});
        }
        snapshot.reticleAssets.push_back(std::move(reticleSnapshot));
    }

    std::sort(snapshot.reticleAssets.begin(),
              snapshot.reticleAssets.end(),
              [](const ReticleAssetSnapshot& lhs, const ReticleAssetSnapshot& rhs)
              {
                  return lhs.id.value < rhs.id.value;
              });
    return snapshot;
}

ValidationReport ValidateAutomationState(const mfd::LoadedWindowConfiguration& loaded, const editor::EditorFileLayout& files)
{
    ValidationReport report;

    if (!loaded.document.pages.empty() && files.pageFiles.size() != loaded.document.pages.size())
    {
        report.valid = false;
        report.diagnostics.push_back(ValidationDiagnostic {
            AutomationErrorCode::ValidationFailed,
            MakeWindowId(loaded).value,
            "Page file layout count must match the number of authored pages."});
    }

    std::unordered_set<std::string> pageNames;
    for (int pageIndex = 0; pageIndex < static_cast<int>(loaded.document.pages.size()); ++pageIndex)
    {
        const mfd::PageDefinition& page = loaded.document.pages[static_cast<std::size_t>(pageIndex)];
        const PageId pageId = MakePageId(page);
        const std::string normalizedPageId = page.normalizedName.empty() ? NormalizeIdentifier(page.name) : page.normalizedName;
        if (page.name.empty())
        {
            report.valid = false;
            report.diagnostics.push_back(
                ValidationDiagnostic {AutomationErrorCode::ValidationFailed, pageId.value, "Page name cannot be empty."});
        }

        if (!pageNames.insert(normalizedPageId).second)
        {
            report.valid = false;
            report.diagnostics.push_back(
                ValidationDiagnostic {AutomationErrorCode::ValidationFailed, pageId.value, "Page ids must stay unique."});
        }

        std::unordered_set<std::string> layerIds;
        for (const mfd::EditorLayerDefinition& layer : page.editor.layers)
        {
            if (layer.id.empty())
            {
                report.valid = false;
                report.diagnostics.push_back(
                    ValidationDiagnostic {AutomationErrorCode::ValidationFailed, pageId.value, "Editor layer ids cannot be empty."});
                continue;
            }

            if (!layerIds.insert(NormalizeIdentifier(layer.id)).second)
            {
                report.valid = false;
                report.diagnostics.push_back(
                    ValidationDiagnostic {AutomationErrorCode::ValidationFailed, pageId.value, "Editor layer ids must stay unique inside one page."});
            }
        }

        std::unordered_set<std::string> reticleIds;
        for (const mfd::ReticleGroup& reticle : page.staticReticles)
        {
            const PageReticleInstanceId reticleId = MakePageReticleInstanceId(page, reticle);
            if (reticle.id.empty())
            {
                report.valid = false;
                report.diagnostics.push_back(
                    ValidationDiagnostic {AutomationErrorCode::ValidationFailed, pageId.value, "Page reticle ids cannot be empty."});
            }

            if (!reticleIds.insert(NormalizeIdentifier(reticle.id)).second)
            {
                report.valid = false;
                report.diagnostics.push_back(
                    ValidationDiagnostic {AutomationErrorCode::ValidationFailed, reticleId.value, "Page reticle ids must stay unique inside one page."});
            }

            if (!reticle.sourceTemplateId.empty() &&
                loaded.document.reticleLibrary.find(reticle.sourceTemplateId) == loaded.document.reticleLibrary.end())
            {
                report.valid = false;
                report.diagnostics.push_back(ValidationDiagnostic {
                    AutomationErrorCode::ValidationFailed,
                    reticleId.value,
                    "Page reticle source template must resolve inside the loaded reticle library."});
            }

            AppendPrimitiveDiagnostics(reticle.primitives, reticleId.value, report);
        }
    }

    for (const auto& [templateId, reticle] : loaded.document.reticleLibrary)
    {
        const ReticleAssetId reticleId = MakeReticleAssetId(templateId);
        if (!MatchesNormalized(templateId, reticle.id))
        {
            report.valid = false;
            report.diagnostics.push_back(ValidationDiagnostic {
                AutomationErrorCode::ValidationFailed,
                reticleId.value,
                "Reticle-library map key and reticle id must stay aligned."});
        }

        if (const auto iterator = files.templateFiles.find(templateId); iterator == files.templateFiles.end())
        {
            report.valid = false;
            report.diagnostics.push_back(
                ValidationDiagnostic {AutomationErrorCode::ValidationFailed, reticleId.value, "Missing template file path for reticle asset."});
        }

        AppendPrimitiveDiagnostics(reticle.primitives, reticleId.value, report);
    }

    return report;
}

bool IsExecStagingPath(const std::filesystem::path& path)
{
    for (const auto& part : path)
    {
        if (part == "_Exec")
        {
            return true;
        }
    }

    return false;
}

int ResolvePrimitiveIndex(const std::vector<mfd::Primitive>& primitives, const PrimitiveSelector& primitiveSelector)
{
    if (!primitiveSelector.primitiveId.empty())
    {
        for (int index = 0; index < static_cast<int>(primitives.size()); ++index)
        {
            const mfd::Primitive& primitive = primitives[static_cast<std::size_t>(index)];
            if (MatchesNormalized(primitive.id, primitiveSelector.primitiveId))
            {
                return index;
            }
        }
    }

    if (primitiveSelector.primitiveIndex >= 0 &&
        primitiveSelector.primitiveIndex < static_cast<int>(primitives.size()))
    {
        return primitiveSelector.primitiveIndex;
    }

    return -1;
}
} // namespace editor::automation
