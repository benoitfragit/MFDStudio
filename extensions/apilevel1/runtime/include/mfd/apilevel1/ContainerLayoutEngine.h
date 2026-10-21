/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/** @file @brief Shared deterministic API LEVEL 1 container layout engine. */

#include <string>
#include <string_view>
#include <vector>

#include "mfd/MfdExport.h"
#include "mfd/apilevel1/ContainerTypes.h"

namespace mfd::api1
{
/** @brief Validates and resolves recursive container hierarchy geometry. */
class MFD_API ContainerLayoutEngine final
{
public:
    /**
     * @brief Validates ownership, identifiers, geometry and recursion bounds.
     * @param containers Flat page-local container catalog.
     * @param error Receives a precise diagnostic on failure.
     * @return `true` when the catalog is safe to evaluate.
     */
    [[nodiscard]] bool Validate(const std::vector<ContainerDefinition>& containers,
                                std::string& error) const;

    /**
     * @brief Resolves every root and descendant into page-space rectangles.
     * @param containers Validated flat page-local catalog.
     * @param output Replaced with deterministic pre-order results.
     * @param error Receives a validation or evaluation diagnostic.
     * @return `true` when layout completed.
     */
    [[nodiscard]] bool Evaluate(const std::vector<ContainerDefinition>& containers,
                                std::vector<ContainerLayoutNode>& output,
                                std::string& error) const;
};

/**
 * @brief Finds one definition without normalizing or allocating its identifier.
 * @param containers Catalog to search.
 * @param id Exact stable identifier.
 * @return Matching definition or `nullptr`.
 */
[[nodiscard]] MFD_API const ContainerDefinition* FindContainer(
    const std::vector<ContainerDefinition>& containers,
    std::string_view id) noexcept;

/** @copydoc FindContainer(const std::vector<ContainerDefinition>&, std::string_view) */
[[nodiscard]] MFD_API ContainerDefinition* FindContainer(
    std::vector<ContainerDefinition>& containers,
    std::string_view id) noexcept;
} // namespace mfd::api1
