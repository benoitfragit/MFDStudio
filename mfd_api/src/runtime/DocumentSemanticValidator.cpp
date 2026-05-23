/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation of semantic validation for loaded MFD documents.
 */

#include "mfd/runtime/DocumentSemanticValidator.h"

#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "mfd/model/PageName.h"
#include "mfd/model/Reticle.h"

namespace mfd
{
namespace
{
constexpr const char* kLibraryPageName = "<reticleLibrary>";

void AddDiagnostic(std::vector<SemanticValidationDiagnostic>& diagnostics,
                   std::string code,
                   std::string message,
                   std::string pageName,
                   std::string reticleId,
                   std::string primitiveId)
{
    SemanticValidationDiagnostic diagnostic;
    diagnostic.code = std::move(code);
    diagnostic.message = std::move(message);
    diagnostic.pageName = std::move(pageName);
    diagnostic.reticleId = std::move(reticleId);
    diagnostic.primitiveId = std::move(primitiveId);
    diagnostics.push_back(std::move(diagnostic));
}

std::string ReticleLabel(const ReticleGroup& reticle)
{
    return reticle.id.empty() ? std::string {"<unnamed>"} : reticle.id;
}

void ValidatePrimitiveIds(const std::string_view pageName,
                          const ReticleGroup& reticle,
                          std::vector<SemanticValidationDiagnostic>& diagnostics)
{
    std::unordered_set<std::string> primitiveIds;
    for (const Primitive& primitive : reticle.primitives)
    {
        if (primitive.id.empty())
        {
            continue;
        }

        if (!primitiveIds.insert(primitive.id).second)
        {
            AddDiagnostic(
                diagnostics,
                "MFD017",
                "MFD017: reticle \"" + ReticleLabel(reticle) + "\" contains duplicate primitive id \"" +
                    primitive.id + "\".",
                std::string(pageName),
                reticle.id,
                primitive.id);
        }
    }
}

void ValidateReticleClipping(const std::string_view pageName,
                             const ReticleGroup& reticle,
                             std::vector<SemanticValidationDiagnostic>& diagnostics)
{
    if (reticle.clipping.mode == ReticleClipMode::None)
    {
        return;
    }

    if (reticle.clipping.primitiveId.empty())
    {
        AddDiagnostic(
            diagnostics,
            "MFD016",
            "MFD016: reticle \"" + ReticleLabel(reticle) + "\" enables clipping but has no primitive id.",
            std::string(pageName),
            reticle.id,
            {});
        return;
    }

    const Primitive* primitive = FindPrimitive(reticle, reticle.clipping.primitiveId);
    if (primitive == nullptr)
    {
        AddDiagnostic(
            diagnostics,
            "MFD014",
            "MFD014: reticle \"" + ReticleLabel(reticle) + "\" references unknown clipping primitive \"" +
                reticle.clipping.primitiveId + "\".",
            std::string(pageName),
            reticle.id,
            reticle.clipping.primitiveId);
        return;
    }

    if (!SupportsReticleClipPrimitive(*primitive))
    {
        AddDiagnostic(
            diagnostics,
            "MFD015",
            "MFD015: primitive \"" + primitive->id + "\" cannot be used as a clipping mask.",
            std::string(pageName),
            reticle.id,
            primitive->id);
    }
}

void ValidateReticle(const std::string_view pageName,
                     const ReticleGroup& reticle,
                     std::vector<SemanticValidationDiagnostic>& diagnostics)
{
    ValidatePrimitiveIds(pageName, reticle, diagnostics);
    ValidateReticleClipping(pageName, reticle, diagnostics);
}

void ValidatePageReticleIds(const PageDefinition& page, std::vector<SemanticValidationDiagnostic>& diagnostics)
{
    std::unordered_set<std::string> normalizedIds;
    for (const ReticleGroup& reticle : page.staticReticles)
    {
        const std::string normalizedId = NormalizePageName(reticle.id);
        if (normalizedId.empty())
        {
            continue;
        }

        if (!normalizedIds.insert(normalizedId).second)
        {
            AddDiagnostic(
                diagnostics,
                "MFD018",
                "MFD018: page \"" + page.name + "\" contains duplicate reticle id \"" + reticle.id +
                    "\" after normalization.",
                page.name,
                reticle.id,
                {});
        }
    }

    for (const PageStrobeDefinition& strobe : page.strobes)
    {
        const std::string normalizedId = NormalizePageName(strobe.reticle.id);
        if (!normalizedId.empty() && !normalizedIds.insert(normalizedId).second)
        {
            AddDiagnostic(
                diagnostics,
                "MFD018",
                "MFD018: page \"" + page.name + "\" contains duplicate reticle id \"" +
                    strobe.reticle.id + "\" after normalization.",
                page.name,
                strobe.reticle.id,
                {});
        }
    }
}

void ValidatePage(const PageDefinition& page, std::vector<SemanticValidationDiagnostic>& diagnostics)
{
    ValidatePageReticleIds(page, diagnostics);

    for (const ReticleGroup& reticle : page.staticReticles)
    {
        ValidateReticle(page.name, reticle, diagnostics);
    }

    for (const PageStrobeDefinition& strobe : page.strobes)
    {
        ValidateReticle(page.name, strobe.reticle, diagnostics);
    }
}

void ValidateReticleLibrary(const ReticleLibrary& library, std::vector<SemanticValidationDiagnostic>& diagnostics)
{
    std::unordered_set<std::string> normalizedIds;
    for (const auto& entry : library)
    {
        const ReticleGroup& reticle = entry.second;
        const std::string normalizedId = NormalizePageName(reticle.id);
        if (!normalizedId.empty() && !normalizedIds.insert(normalizedId).second)
        {
            AddDiagnostic(
                diagnostics,
                "MFD018",
                "MFD018: reticle library contains duplicate reticle id \"" + reticle.id +
                    "\" after normalization.",
                kLibraryPageName,
                reticle.id,
                {});
        }

        ValidateReticle(kLibraryPageName, reticle, diagnostics);
    }
}
} // namespace

std::vector<SemanticValidationDiagnostic> DocumentSemanticValidator::Validate(const MfdDocument& document) const
{
    std::vector<SemanticValidationDiagnostic> diagnostics;
    ValidateReticleLibrary(document.reticleLibrary, diagnostics);

    for (const PageDefinition& page : document.pages)
    {
        ValidatePage(page, diagnostics);
    }

    return diagnostics;
}

void ThrowIfDocumentSemanticsInvalid(const MfdDocument& document)
{
    const std::vector<SemanticValidationDiagnostic> diagnostics = DocumentSemanticValidator {}.Validate(document);
    if (diagnostics.empty())
    {
        return;
    }

    std::string message = diagnostics.front().message;
    if (diagnostics.size() > 1U)
    {
        message += " (" + std::to_string(diagnostics.size()) + " semantic diagnostics total)";
    }

    throw std::runtime_error(message);
}
} // namespace mfd
