/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Validation-problem records and context resolution used to make the Problems panel navigable.
 */

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace editor
{
/**
 * @brief One validation diagnostic plus the editor entity it refers to.
 *
 * @details `contextId` mirrors the identifiers built by the problem collector: `"window"`,
 * `"page/<normalized>"`, `"page/<normalized>/reticle/<id>"`, or `"reticle/<normalized>"`. It lets the
 * Problems panel jump to the offending entity when a row is clicked.
 */
struct PagePreviewProblem
{
    std::string message;
    std::string contextId;
};

/**
 * @brief Resolves the page index referenced by a problem context.
 * @param normalizedPageIds Normalized page identifiers, indexed like the authored pages, in the exact
 *                          form embedded in the problem context (`"page/<entry>"`).
 * @param contextId Problem context to resolve.
 * @return The matching page index, or `-1` when the context is not page-scoped or no page matches.
 */
[[nodiscard]] inline int MatchProblemPageIndex(const std::vector<std::string>& normalizedPageIds,
                                               const std::string_view contextId)
{
    constexpr std::string_view kPagePrefix = "page/";
    if (contextId.size() <= kPagePrefix.size() || contextId.substr(0, kPagePrefix.size()) != kPagePrefix)
    {
        return -1;
    }

    const std::string_view rest = contextId.substr(kPagePrefix.size());
    const std::size_t slash = rest.find('/');
    const std::string_view pageSegment = slash == std::string_view::npos ? rest : rest.substr(0, slash);

    for (std::size_t index = 0; index < normalizedPageIds.size(); ++index)
    {
        if (normalizedPageIds[index] == pageSegment)
        {
            return static_cast<int>(index);
        }
    }

    return -1;
}
} // namespace editor
