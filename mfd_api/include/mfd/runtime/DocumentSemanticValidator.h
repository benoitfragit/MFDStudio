/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Semantic validation for loaded MFD documents.
 */

#include <string>
#include <vector>

#include "mfd/MfdExport.h"
#include "mfd/model/PageDefinition.h"

namespace mfd
{
/**
 * @brief One semantic validation diagnostic produced for an MFD document.
 */
struct SemanticValidationDiagnostic
{
    /** @brief Stable diagnostic code, for example `MFD014`. */
    std::string code;
    /** @brief Human-readable diagnostic message. */
    std::string message;
    /** @brief Page owning the diagnostic, when applicable. */
    std::string pageName;
    /** @brief Reticle owning the diagnostic, when applicable. */
    std::string reticleId;
    /** @brief Primitive owning or referenced by the diagnostic, when applicable. */
    std::string primitiveId;
};

/**
 * @brief Validates cross-reference and semantic constraints that require a parsed document.
 *
 * @note This validator intentionally stays outside the renderer. Clipping
 * diagnostics do not change the existing stencil erase/mask rendering behavior.
 */
class MFD_API DocumentSemanticValidator
{
public:
    /**
     * @brief Validates a document and returns all diagnostics.
     * @param document Document to validate.
     * @return Flat list of semantic diagnostics.
     */
    [[nodiscard]] std::vector<SemanticValidationDiagnostic> Validate(const MfdDocument& document) const;
};

/**
 * @brief Throws a runtime error when semantic validation reports diagnostics.
 * @param document Document to validate.
 *
 * @throws std::runtime_error Contains the first diagnostic and a diagnostic count.
 */
MFD_API void ThrowIfDocumentSemanticsInvalid(const MfdDocument& document);
} // namespace mfd
