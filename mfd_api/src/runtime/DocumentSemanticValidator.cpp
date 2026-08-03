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
#include "mfd/model/RuntimeBudgets.h"

namespace mfd
{
namespace
{
constexpr const char* kLibraryPageName = "<reticleLibrary>";

struct DocumentResourceUsage
{
    std::size_t reticles = 0;
    std::size_t layers = 0;
    std::size_t primitives = 0;
    std::size_t visiblePrimitives = 0;
    std::size_t stringBytes = 0;
};

bool ExceedsResourceBudget(const DocumentResourceUsage& usage) noexcept
{
    return usage.reticles > runtime_validation::kMaxDocumentReticles ||
           usage.layers > runtime_validation::kMaxDocumentLayers ||
           usage.primitives > runtime_validation::kMaxDocumentPrimitives ||
           usage.visiblePrimitives > runtime_validation::kMaxDocumentVisiblePrimitives ||
           usage.stringBytes > runtime_validation::kMaxDocumentStringBytes;
}

void AddBounded(std::size_t& total, const std::size_t amount, const std::size_t maximum) noexcept
{
    if (total > maximum || amount > maximum - total)
    {
        total = maximum + 1U;
        return;
    }

    total += amount;
}

void CountString(DocumentResourceUsage& usage, const std::string_view value) noexcept
{
    AddBounded(usage.stringBytes, value.size(), runtime_validation::kMaxDocumentStringBytes);
}

void CountPath(DocumentResourceUsage& usage, const std::filesystem::path& value) noexcept
{
    const auto& nativeValue = value.native();
    AddBounded(
        usage.stringBytes,
        nativeValue.size() * sizeof(std::filesystem::path::value_type),
        runtime_validation::kMaxDocumentStringBytes);
}

void CountPrimitive(DocumentResourceUsage& usage, const Primitive& primitive, const bool reticleVisible)
{
    AddBounded(usage.primitives, 1U, runtime_validation::kMaxDocumentPrimitives);
    if (reticleVisible && primitive.style.visible)
    {
        AddBounded(usage.visiblePrimitives, 1U, runtime_validation::kMaxDocumentVisiblePrimitives);
    }

    CountString(usage, primitive.id);
    if (const auto* text = std::get_if<TextGeometry>(&primitive.geometry))
    {
        CountString(usage, text->text);
    }
    else if (const auto* time = std::get_if<TimeGeometry>(&primitive.geometry))
    {
        CountString(usage, time->format);
    }
    else if (const auto* image = std::get_if<ImageGeometry>(&primitive.geometry))
    {
        CountPath(usage, image->file);
    }
}

void CountReticle(DocumentResourceUsage& usage, const ReticleGroup& reticle)
{
    AddBounded(usage.reticles, 1U, runtime_validation::kMaxDocumentReticles);
    CountString(usage, reticle.id);
    CountString(usage, reticle.sourceTemplateId);
    CountString(usage, reticle.layerId);
    CountString(usage, reticle.info.label);
    CountString(usage, reticle.info.category);
    CountString(usage, reticle.blink.typeName);
    CountString(usage, reticle.blink.normalizedTypeName);
    CountString(usage, reticle.clipping.primitiveId);
    if (ExceedsResourceBudget(usage))
    {
        return;
    }

    for (const auto& metadataEntry : reticle.info.metadata)
    {
        CountString(usage, metadataEntry.first);
        CountString(usage, metadataEntry.second);
        if (ExceedsResourceBudget(usage))
        {
            return;
        }
    }

    for (const Primitive& primitive : reticle.primitives)
    {
        CountPrimitive(usage, primitive, reticle.visible);
        if (ExceedsResourceBudget(usage))
        {
            return;
        }
    }
}

DocumentResourceUsage MeasureDocumentResources(const MfdDocument& document)
{
    DocumentResourceUsage usage;
    CountPath(usage, document.sourceFile);
    CountPath(usage, document.reticleLibraryFolder);
    if (ExceedsResourceBudget(usage))
    {
        return usage;
    }

    for (const auto& libraryEntry : document.reticleLibrary)
    {
        CountString(usage, libraryEntry.first);
        CountReticle(usage, libraryEntry.second);
        if (ExceedsResourceBudget(usage))
        {
            return usage;
        }
    }

    for (const PageDefinition& page : document.pages)
    {
        CountString(usage, page.name);
        CountString(usage, page.normalizedName);
        CountString(usage, page.title);
        CountString(usage, page.defaultBlinkTypeName);
        CountString(usage, page.normalizedDefaultBlinkTypeName);
        CountString(usage, page.activeStrobeName);
        CountString(usage, page.normalizedActiveStrobeName);
        if (ExceedsResourceBudget(usage))
        {
            return usage;
        }

        AddBounded(usage.layers, page.layers.size(), runtime_validation::kMaxDocumentLayers);
        AddBounded(usage.layers, page.editor.layers.size(), runtime_validation::kMaxDocumentLayers);
        if (ExceedsResourceBudget(usage))
        {
            return usage;
        }
        for (const PageLayerDefinition& layer : page.layers)
        {
            CountString(usage, layer.id);
            if (ExceedsResourceBudget(usage))
            {
                return usage;
            }
        }
        for (const EditorLayerDefinition& layer : page.editor.layers)
        {
            CountString(usage, layer.id);
            if (ExceedsResourceBudget(usage))
            {
                return usage;
            }
        }
        for (const PageBlinkDefinition& blink : page.blinkTypes)
        {
            CountString(usage, blink.name);
            CountString(usage, blink.normalizedName);
            if (ExceedsResourceBudget(usage))
            {
                return usage;
            }
        }
        for (const DynamicReticleLayerBinding& binding : page.dynamicReticleBindings)
        {
            CountString(usage, binding.templateId);
            CountString(usage, binding.layerId);
            if (ExceedsResourceBudget(usage))
            {
                return usage;
            }
        }
        for (const ReticleGroup& reticle : page.staticReticles)
        {
            CountReticle(usage, reticle);
            if (ExceedsResourceBudget(usage))
            {
                return usage;
            }
        }
        for (const PageStrobeDefinition& strobe : page.strobes)
        {
            CountString(usage, strobe.name);
            CountString(usage, strobe.normalizedName);
            CountReticle(usage, strobe.reticle);
            if (ExceedsResourceBudget(usage))
            {
                return usage;
            }
        }
    }

    return usage;
}

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
    if (document.pages.size() > runtime_validation::kMaxDocumentPages)
    {
        AddDiagnostic(
            diagnostics,
            "MFD019",
            "MFD019: document exceeds the page budget of " +
                std::to_string(runtime_validation::kMaxDocumentPages) + ".",
            {},
            {},
            {});
    }
    if (document.reticleLibrary.size() > runtime_validation::kMaxDocumentTemplates)
    {
        AddDiagnostic(
            diagnostics,
            "MFD020",
            "MFD020: document exceeds the reticle template budget of " +
                std::to_string(runtime_validation::kMaxDocumentTemplates) + ".",
            kLibraryPageName,
            {},
            {});
    }

    if (!diagnostics.empty())
    {
        return diagnostics;
    }

    const DocumentResourceUsage usage = MeasureDocumentResources(document);
    if (usage.reticles > runtime_validation::kMaxDocumentReticles)
    {
        AddDiagnostic(
            diagnostics,
            "MFD021",
            "MFD021: document exceeds the aggregate reticle budget of " +
                std::to_string(runtime_validation::kMaxDocumentReticles) + ".",
            {},
            {},
            {});
    }
    if (usage.layers > runtime_validation::kMaxDocumentLayers)
    {
        AddDiagnostic(
            diagnostics,
            "MFD022",
            "MFD022: document exceeds the aggregate page layer budget of " +
                std::to_string(runtime_validation::kMaxDocumentLayers) + ".",
            {},
            {},
            {});
    }
    if (usage.primitives > runtime_validation::kMaxDocumentPrimitives)
    {
        AddDiagnostic(
            diagnostics,
            "MFD023",
            "MFD023: document exceeds the aggregate primitive budget of " +
                std::to_string(runtime_validation::kMaxDocumentPrimitives) + ".",
            {},
            {},
            {});
    }
    if (usage.visiblePrimitives > runtime_validation::kMaxDocumentVisiblePrimitives)
    {
        AddDiagnostic(
            diagnostics,
            "MFD024",
            "MFD024: document exceeds the initially visible primitive budget of " +
                std::to_string(runtime_validation::kMaxDocumentVisiblePrimitives) + ".",
            {},
            {},
            {});
    }
    if (usage.stringBytes > runtime_validation::kMaxDocumentStringBytes)
    {
        AddDiagnostic(
            diagnostics,
            "MFD025",
            "MFD025: document exceeds the aggregate authored string budget of " +
                std::to_string(runtime_validation::kMaxDocumentStringBytes) + " bytes.",
            {},
            {},
            {});
    }

    if (!diagnostics.empty())
    {
        return diagnostics;
    }

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
