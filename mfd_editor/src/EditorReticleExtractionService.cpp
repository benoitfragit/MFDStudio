/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorReticleExtractionService.h"

/**
 * @file
 * @brief Implementation of the editor reticle-extraction planning service.
 */

#include <algorithm>
#include <cctype>
#include <cmath>
#include <functional>
#include <optional>
#include <string_view>
#include <unordered_set>

#include "mfd/model/PageName.h"

namespace editor
{
namespace
{
std::string TrimAsciiWhitespace(const std::string_view value)
{
    std::size_t first = 0U;
    while (first < value.size() &&
           std::isspace(static_cast<unsigned char>(value[first])) != 0)
    {
        ++first;
    }

    std::size_t last = value.size();
    while (last > first &&
           std::isspace(static_cast<unsigned char>(value[last - 1U])) != 0)
    {
        --last;
    }

    return std::string(value.substr(first, last - first));
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

std::string NormalizeIdentifier(const std::string_view value)
{
    return mfd::NormalizePageName(value);
}

std::filesystem::path NormalizePath(std::filesystem::path path)
{
    if (path.empty())
    {
        return {};
    }

    std::error_code error;
    if (!path.is_absolute())
    {
        path = std::filesystem::absolute(path, error);
        if (error)
        {
            error.clear();
        }
    }

    return path.lexically_normal();
}

std::string PathKey(const std::filesystem::path& path)
{
    return Lowercase(NormalizePath(path).generic_string());
}

bool PathsEqual(const std::filesystem::path& lhs, const std::filesystem::path& rhs)
{
    return PathKey(lhs) == PathKey(rhs);
}

bool PathExists(const std::filesystem::path& path)
{
    if (path.empty())
    {
        return false;
    }

    std::error_code error;
    return std::filesystem::exists(path, error);
}

std::filesystem::path EnsureJsonFilePath(std::filesystem::path path)
{
    if (path.empty())
    {
        return {};
    }

    if (path.extension().empty())
    {
        path += ".json";
    }

    return NormalizePath(std::move(path));
}

std::string SuggestUniqueTemplateId(const std::string_view baseId,
                                   const std::function<bool(const std::string_view)>& isTaken)
{
    const std::string trimmedBaseId = TrimAsciiWhitespace(baseId);
    const std::string safeBaseId = trimmedBaseId.empty() ? std::string {"extracted_reticle"} : trimmedBaseId;
    if (!isTaken(safeBaseId))
    {
        return safeBaseId;
    }

    for (int suffix = 2; suffix < 10000; ++suffix)
    {
        const std::string candidate = safeBaseId + "_" + std::to_string(suffix);
        if (!isTaken(candidate))
        {
            return candidate;
        }
    }

    return safeBaseId + "_copy";
}

std::filesystem::path SuggestUniqueJsonPath(const std::filesystem::path& preferredPath,
                                            const std::function<bool(const std::filesystem::path&)>& isTaken)
{
    const std::filesystem::path normalizedPreferred = EnsureJsonFilePath(preferredPath);
    if (normalizedPreferred.empty() || !isTaken(normalizedPreferred))
    {
        return normalizedPreferred;
    }

    const std::filesystem::path stem = normalizedPreferred.stem();
    const std::filesystem::path extension = normalizedPreferred.extension();
    const std::filesystem::path parent = normalizedPreferred.parent_path();

    for (int suffix = 2; suffix < 10000; ++suffix)
    {
        const std::filesystem::path candidate = parent / (stem.string() + "_" + std::to_string(suffix) + extension.string());
        if (!isTaken(candidate))
        {
            return NormalizePath(candidate);
        }
    }

    return normalizedPreferred;
}

std::string SuggestUniquePrimitiveId(const std::string_view baseId, std::unordered_set<std::string>& usedIds)
{
    const std::string safeBaseId = TrimAsciiWhitespace(baseId).empty() ? std::string {"primitive"} : std::string(baseId);
    auto reserveId = [&usedIds](const std::string& candidate)
    {
        const std::string normalized = NormalizeIdentifier(candidate);
        return usedIds.insert(normalized).second;
    };

    if (reserveId(safeBaseId))
    {
        return safeBaseId;
    }

    for (int suffix = 2; suffix < 10000; ++suffix)
    {
        const std::string candidate = safeBaseId + "_" + std::to_string(suffix);
        if (reserveId(candidate))
        {
            return candidate;
        }
    }

    return safeBaseId + "_copy";
}

bool AreIndicesContiguous(const std::vector<int>& indices)
{
    if (indices.empty())
    {
        return false;
    }

    for (std::size_t index = 1; index < indices.size(); ++index)
    {
        if (indices[index] != indices[index - 1U] + 1)
        {
            return false;
        }
    }

    return true;
}

std::vector<int> SortedUniqueIndices(const std::vector<int>& indices)
{
    std::vector<int> result = indices;
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

std::vector<int> RemainingReticleIndices(const std::size_t reticleCount, const std::vector<int>& removedIndices)
{
    std::vector<int> indices;
    indices.reserve(reticleCount);
    for (int index = 0; index < static_cast<int>(reticleCount); ++index)
    {
        if (!std::binary_search(removedIndices.begin(), removedIndices.end(), index))
        {
            indices.push_back(index);
        }
    }
    return indices;
}

mfd::Vec2 AverageAnchor(const mfd::PageDefinition& page, const std::vector<int>& reticleIndices)
{
    mfd::Vec2 anchor {};
    for (const int reticleIndex : reticleIndices)
    {
        anchor.x += page.staticReticles[static_cast<std::size_t>(reticleIndex)].transform.position.x;
        anchor.y += page.staticReticles[static_cast<std::size_t>(reticleIndex)].transform.position.y;
    }

    const float count = static_cast<float>(std::max<std::size_t>(1U, reticleIndices.size()));
    anchor.x /= count;
    anchor.y /= count;
    return anchor;
}

bool BlinkStateIsDefault(const mfd::ReticleBlinkState& blink)
{
    return !blink.enabled && blink.typeName.empty() && blink.normalizedTypeName.empty() && blink.durationMs == 0U;
}

std::string ReticleIdOrFallback(const mfd::ReticleGroup& reticle, const int reticleIndex)
{
    return reticle.id.empty() ? "reticle_" + std::to_string(reticleIndex + 1) : reticle.id;
}

std::optional<std::filesystem::path> FindAncestorNamed(std::filesystem::path path, const std::string_view folderName)
{
    path = NormalizePath(std::move(path));
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

std::filesystem::path ResolveExtractionAssetRoot(const mfd::LoadedWindowConfiguration& loaded,
                                                 const EditorFileLayout& files,
                                                 const int pageIndex)
{
    const auto resolveOne = [](const std::filesystem::path& candidate)
    {
        if (candidate.empty())
        {
            return std::filesystem::path {};
        }

        if (const auto assetsRoot = FindAncestorNamed(candidate, "assets"); assetsRoot.has_value())
        {
            return assetsRoot->lexically_normal();
        }

        return std::filesystem::path {};
    };

    if (const std::filesystem::path assetRoot = resolveOne(loaded.window.reticleLibraryFolder); !assetRoot.empty())
    {
        return assetRoot;
    }

    if (pageIndex >= 0 && pageIndex < static_cast<int>(files.pageFiles.size()))
    {
        if (const std::filesystem::path assetRoot = resolveOne(files.pageFiles[static_cast<std::size_t>(pageIndex)]);
            !assetRoot.empty())
        {
            return assetRoot;
        }
    }

    return resolveOne(loaded.window.sourceFile);
}

bool PathStartsWith(const std::filesystem::path& candidate, const std::filesystem::path& root)
{
    const std::filesystem::path normalizedCandidate = NormalizePath(candidate);
    const std::filesystem::path normalizedRoot = NormalizePath(root);
    if (normalizedCandidate.empty() || normalizedRoot.empty())
    {
        return false;
    }

    auto candidateIterator = normalizedCandidate.begin();
    auto rootIterator = normalizedRoot.begin();
    for (; rootIterator != normalizedRoot.end(); ++rootIterator, ++candidateIterator)
    {
        if (candidateIterator == normalizedCandidate.end() ||
            Lowercase(candidateIterator->string()) != Lowercase(rootIterator->string()))
        {
            return false;
        }
    }

    return true;
}

std::optional<std::string> ValidateSelectionCompatibility(const mfd::PageDefinition& page,
                                                          const std::vector<int>& reticleIndices,
                                                          const std::filesystem::path& assetRoot)
{
    if (!AreIndicesContiguous(reticleIndices))
    {
        return std::string {"Smart extraction currently requires one contiguous page-reticle block."};
    }

    std::optional<bool> sharedDrawOnTop;
    std::optional<std::string> sharedLayerId;
    for (const int reticleIndex : reticleIndices)
    {
        if (reticleIndex < 0 || reticleIndex >= static_cast<int>(page.staticReticles.size()))
        {
            return std::string {"One selected page reticle no longer exists."};
        }

        const mfd::ReticleGroup& reticle = page.staticReticles[static_cast<std::size_t>(reticleIndex)];
        if (!reticle.visible)
        {
            return std::string {"Smart extraction currently expects visible page reticles only."};
        }
        if (reticle.clipping.mode != mfd::ReticleClipMode::None)
        {
            return std::string {"Smart extraction does not support clipped page reticles yet."};
        }
        if (!BlinkStateIsDefault(reticle.blink))
        {
            return std::string {"Smart extraction does not support page-reticle blink bindings yet."};
        }
        if (reticle.primitives.empty())
        {
            return std::string {"The selected page reticles do not expose any primitive to extract."};
        }

        for (const mfd::Primitive& primitive : reticle.primitives)
        {
            if (primitive.type != mfd::PrimitiveType::Image)
            {
                continue;
            }

            const auto* image = std::get_if<mfd::ImageGeometry>(&primitive.geometry);
            if (image == nullptr)
            {
                return std::string {"Smart extraction found one invalid image primitive payload."};
            }

            if (image->file.empty() || image->file.is_relative() || assetRoot.empty())
            {
                continue;
            }

            if (!PathStartsWith(image->file, assetRoot))
            {
                return std::string {"Smart extraction does not copy image dependencies outside the current assets root yet."};
            }
        }

        if (!sharedDrawOnTop.has_value())
        {
            sharedDrawOnTop = reticle.drawOnTop;
        }
        else if (*sharedDrawOnTop != reticle.drawOnTop)
        {
            return std::string {"Smart extraction cannot merge page reticles that mix regular and draw-on-top ordering."};
        }

        if (!sharedLayerId.has_value())
        {
            sharedLayerId = reticle.editor.layerId;
        }
        else if (*sharedLayerId != reticle.editor.layerId)
        {
            return std::string {"Smart extraction currently requires the selected page reticles to stay on the same editor layer."};
        }
    }

    return std::nullopt;
}

std::string ResolveRequestedTemplateId(const mfd::PageDefinition& page,
                                       const std::vector<int>& reticleIndices,
                                       const std::string_view requestedTemplateId)
{
    const std::string trimmedRequestedId = TrimAsciiWhitespace(requestedTemplateId);
    if (!trimmedRequestedId.empty())
    {
        return trimmedRequestedId;
    }

    if (!reticleIndices.empty())
    {
        const mfd::ReticleGroup& firstReticle = page.staticReticles[static_cast<std::size_t>(reticleIndices.front())];
        if (!firstReticle.sourceTemplateId.empty())
        {
            return firstReticle.sourceTemplateId + "_extract";
        }
        if (!firstReticle.id.empty())
        {
            return firstReticle.id + "_extract";
        }
    }

    return "extracted_reticle";
}

mfd::Primitive FlattenPrimitive(const mfd::ReticleGroup& reticle,
                               const mfd::Primitive& primitive,
                               const mfd::Vec2 anchor)
{
    mfd::Primitive flattened = primitive;
    flattened.transform = mfd::CombineTransforms(reticle.transform, primitive.transform);
    flattened.transform.position = flattened.transform.position - anchor;
    flattened.style = mfd::MergeStyle(flattened.style, reticle.overrides);
    return flattened;
}
} // namespace

ReticleExtractionPlan ReticleExtractionService::BuildPlan(const mfd::LoadedWindowConfiguration& loaded,
                                                          const EditorFileLayout& files,
                                                          const ReticleExtractionRequest& request) const
{
    ReticleExtractionPlan plan;
    plan.pageIndex = request.pageIndex;
    plan.requestedTemplateId = TrimAsciiWhitespace(request.requestedTemplateId);
    plan.reticleIndices = SortedUniqueIndices(request.reticleIndices);

    if (request.pageIndex < 0 || request.pageIndex >= static_cast<int>(loaded.document.pages.size()))
    {
        plan.error = "Select one page before extracting a reusable reticle.";
        return plan;
    }

    if (plan.reticleIndices.empty())
    {
        plan.error = "Select one or more page reticles before extracting them as a reusable reticle.";
        return plan;
    }

    if (loaded.window.reticleLibraryFolder.empty())
    {
        plan.error = "The current window does not expose a reticle library folder yet.";
        return plan;
    }

    const mfd::PageDefinition& page = loaded.document.pages[static_cast<std::size_t>(request.pageIndex)];
    const std::filesystem::path assetRoot = ResolveExtractionAssetRoot(loaded, files, request.pageIndex);
    if (const std::optional<std::string> compatibilityError = ValidateSelectionCompatibility(page, plan.reticleIndices, assetRoot);
        compatibilityError.has_value())
    {
        plan.error = *compatibilityError;
        return plan;
    }

    const std::string baseTemplateId = ResolveRequestedTemplateId(page, plan.reticleIndices, plan.requestedTemplateId);
    const auto isTemplateIdTaken = [&loaded](const std::string_view candidate)
    {
        return std::any_of(loaded.document.reticleLibrary.begin(),
                           loaded.document.reticleLibrary.end(),
                           [candidate](const auto& entry)
                           {
                               return mfd::PageNamesEqual(entry.first, candidate);
                           });
    };

    plan.targetTemplateId = SuggestUniqueTemplateId(baseTemplateId, isTemplateIdTaken);
    plan.templateIdAdjusted = !mfd::PageNamesEqual(plan.targetTemplateId, baseTemplateId) || plan.targetTemplateId != baseTemplateId;

    std::filesystem::path preferredTemplateFile =
        request.requestedTemplateFile.empty()
            ? DefaultTemplateFilePath(loaded.window.reticleLibraryFolder, plan.targetTemplateId)
            : request.requestedTemplateFile;
    if (preferredTemplateFile.is_relative())
    {
        preferredTemplateFile = loaded.window.reticleLibraryFolder / preferredTemplateFile;
    }
    preferredTemplateFile = EnsureJsonFilePath(preferredTemplateFile);

    const auto isTemplateFileTaken = [&files](const std::filesystem::path& candidate)
    {
        return PathExists(candidate) ||
               std::any_of(files.templateFiles.begin(),
                           files.templateFiles.end(),
                           [&candidate](const auto& entry)
                           {
                               return PathsEqual(entry.second, candidate);
                           });
    };
    plan.targetTemplateFile = SuggestUniqueJsonPath(preferredTemplateFile, isTemplateFileTaken);
    plan.templateFileAdjusted = !PathsEqual(plan.targetTemplateFile, preferredTemplateFile);

    plan.insertionIndex = plan.reticleIndices.front();
    plan.anchor = AverageAnchor(page, plan.reticleIndices);
    plan.drawOnTop = page.staticReticles[static_cast<std::size_t>(plan.reticleIndices.front())].drawOnTop;
    plan.targetLayerId = page.staticReticles[static_cast<std::size_t>(plan.reticleIndices.front())].editor.layerId;

    std::unordered_set<std::string> usedPrimitiveIds;
    for (const int reticleIndex : plan.reticleIndices)
    {
        const mfd::ReticleGroup& reticle = page.staticReticles[static_cast<std::size_t>(reticleIndex)];
        plan.sourceReticleIds.push_back(ReticleIdOrFallback(reticle, reticleIndex));

        for (const mfd::Primitive& primitive : reticle.primitives)
        {
            mfd::Primitive flattened = FlattenPrimitive(reticle, primitive, plan.anchor);
            flattened.id = SuggestUniquePrimitiveId(
                flattened.id.empty() ? ReticleIdOrFallback(reticle, reticleIndex) + "_primitive" : flattened.id,
                usedPrimitiveIds);
            plan.extractedTemplate.primitives.push_back(std::move(flattened));
            ++plan.extractedPrimitiveCount;
        }
    }

    if (plan.extractedPrimitiveCount == 0U)
    {
        plan.error = "The selected page reticles do not expose any primitive to extract.";
        plan.extractedTemplate.primitives.clear();
        return plan;
    }

    plan.extractedTemplate.id = plan.targetTemplateId;
    plan.extractedTemplate.sourceTemplateId.clear();
    plan.extractedTemplate.visible = true;
    plan.extractedTemplate.drawOnTop = plan.drawOnTop;
    plan.extractedTemplate.transform = {};
    plan.extractedTemplate.overrides = {};
    plan.extractedTemplate.editor = {};
    plan.extractedTemplate.clipping = {};
    plan.extractedTemplate.blink = {};

    const std::vector<int> remainingIndices = RemainingReticleIndices(page.staticReticles.size(), plan.reticleIndices);
    auto isReplacementIdTaken = [&page, &remainingIndices](const std::string_view candidate)
    {
        return std::any_of(remainingIndices.begin(),
                           remainingIndices.end(),
                           [&page, candidate](const int reticleIndex)
                           {
                               return mfd::PageNamesEqual(page.staticReticles[static_cast<std::size_t>(reticleIndex)].id, candidate);
                           });
    };
    plan.replacementInstanceId = SuggestUniqueTemplateId(plan.targetTemplateId, isReplacementIdTaken);
    plan.replacementInstance = mfd::InstantiateReticle(
        plan.extractedTemplate,
        plan.replacementInstanceId,
        mfd::Transform2D {plan.anchor, 0.0f, {1.0f, 1.0f}},
        {});
    plan.replacementInstance.editor.layerId = plan.targetLayerId;
    plan.replacementInstance.drawOnTop = plan.drawOnTop;
    plan.canExecute = true;
    return plan;
}

bool ReticleExtractionService::Execute(const ReticleExtractionPlan& plan,
                                       mfd::LoadedWindowConfiguration& loaded,
                                       EditorFileLayout& files,
                                       ReticleExtractionResult* result,
                                       std::string* error) const
{
    if (!plan.canExecute)
    {
        if (error != nullptr)
        {
            *error = plan.error.empty() ? "The reticle extraction plan is not executable." : plan.error;
        }
        return false;
    }

    if (plan.pageIndex < 0 || plan.pageIndex >= static_cast<int>(loaded.document.pages.size()))
    {
        if (error != nullptr)
        {
            *error = "The selected page is no longer available.";
        }
        return false;
    }

    mfd::PageDefinition& page = loaded.document.pages[static_cast<std::size_t>(plan.pageIndex)];
    if (plan.reticleIndices.empty() ||
        plan.reticleIndices.back() >= static_cast<int>(page.staticReticles.size()))
    {
        if (error != nullptr)
        {
            *error = "One selected page reticle is no longer available.";
        }
        return false;
    }

    loaded.document.reticleLibrary[plan.targetTemplateId] = plan.extractedTemplate;
    files.templateFiles[plan.targetTemplateId] = NormalizePath(plan.targetTemplateFile);

    for (auto iterator = plan.reticleIndices.rbegin(); iterator != plan.reticleIndices.rend(); ++iterator)
    {
        page.staticReticles.erase(page.staticReticles.begin() + *iterator);
    }

    const int clampedInsertionIndex = std::clamp(plan.insertionIndex, 0, static_cast<int>(page.staticReticles.size()));
    page.staticReticles.insert(page.staticReticles.begin() + clampedInsertionIndex, plan.replacementInstance);

    if (result != nullptr)
    {
        result->templateId = plan.targetTemplateId;
        result->templateFile = NormalizePath(plan.targetTemplateFile);
        result->insertedReticleIndex = clampedInsertionIndex;
        result->extractedPrimitiveCount = plan.extractedPrimitiveCount;
    }

    if (error != nullptr)
    {
        error->clear();
    }
    return true;
}
} // namespace editor
