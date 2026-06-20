/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Sidebar filter token parsing and validation-problem summary used by the navigation panel.
 *
 * @details The sidebar keeps a plain free-text filter by default. Optional leading scope tokens
 * (`page:`, `reticle:`, `problem:`) let the user aim the same filter at one section without turning
 * the panel into a query engine. The problem summary folds the existing diagnostics into per-page and
 * per-reticle presence sets so headers can show counts and rows can show badges from a single source
 * of truth (the same `contextId` strings the Problems panel already navigates by).
 */

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "EditorProblemNavigation.h"

namespace editor
{
/** @brief Section a sidebar filter is aimed at once an optional scope token is parsed. */
enum class SidebarFilterScope
{
    All,
    Pages,
    Reticles,
    Problems
};

/** @brief A parsed sidebar filter: the resolved scope plus the trimmed free-text query. */
struct SidebarFilterQuery
{
    SidebarFilterScope scope = SidebarFilterScope::All;
    std::string text;
};

namespace detail
{
[[nodiscard]] inline bool IsAsciiSpace(const char value) noexcept
{
    return value == ' ' || value == '\t' || value == '\n' || value == '\r' || value == '\f' || value == '\v';
}

[[nodiscard]] inline char LowerAsciiToken(const char value) noexcept
{
    return (value >= 'A' && value <= 'Z') ? static_cast<char>(value - 'A' + 'a') : value;
}

[[nodiscard]] inline std::string_view TrimAsciiSpace(std::string_view text) noexcept
{
    std::size_t begin = 0;
    while (begin < text.size() && IsAsciiSpace(text[begin]))
    {
        ++begin;
    }

    std::size_t end = text.size();
    while (end > begin && IsAsciiSpace(text[end - 1]))
    {
        --end;
    }

    return text.substr(begin, end - begin);
}

/**
 * @brief Consumes a case-insensitive scope prefix from @p text when present.
 * @param text Filter text scanned in place; advanced past the prefix on a match.
 * @param lowerPrefix Scope prefix in lowercase ASCII (for example `"page:"`).
 * @return `true` when the prefix matched and was consumed, `false` otherwise.
 */
[[nodiscard]] inline bool ConsumeScopePrefix(std::string_view& text, const std::string_view lowerPrefix) noexcept
{
    if (text.size() < lowerPrefix.size())
    {
        return false;
    }

    for (std::size_t index = 0; index < lowerPrefix.size(); ++index)
    {
        if (LowerAsciiToken(text[index]) != lowerPrefix[index])
        {
            return false;
        }
    }

    text.remove_prefix(lowerPrefix.size());
    return true;
}
} // namespace detail

/**
 * @brief Parses the raw sidebar filter buffer into a scope plus a trimmed query.
 * @param raw Raw filter text as typed by the user.
 * @return The resolved scope and the remaining free-text query (empty when only a token was typed).
 * @note Without a recognized leading token the scope stays @ref SidebarFilterScope::All so the panel
 *       keeps its simple default behavior.
 */
[[nodiscard]] inline SidebarFilterQuery ParseSidebarFilter(const std::string_view raw)
{
    std::string_view text = detail::TrimAsciiSpace(raw);

    SidebarFilterQuery query;
    if (detail::ConsumeScopePrefix(text, "page:"))
    {
        query.scope = SidebarFilterScope::Pages;
    }
    else if (detail::ConsumeScopePrefix(text, "reticle:"))
    {
        query.scope = SidebarFilterScope::Reticles;
    }
    else if (detail::ConsumeScopePrefix(text, "problem:"))
    {
        query.scope = SidebarFilterScope::Problems;
    }

    query.text = std::string(detail::TrimAsciiSpace(text));
    return query;
}

/** @brief Returns whether the resolved scope keeps the authored Pages section visible. */
[[nodiscard]] inline bool SidebarFilterShowsPages(const SidebarFilterScope scope) noexcept
{
    return scope != SidebarFilterScope::Reticles;
}

/** @brief Returns whether the resolved scope keeps the Reticle library section visible. */
[[nodiscard]] inline bool SidebarFilterShowsReticles(const SidebarFilterScope scope) noexcept
{
    return scope != SidebarFilterScope::Pages;
}

/** @brief Returns whether a parsed filter actually narrows the lists (a scope token or query text). */
[[nodiscard]] inline bool SidebarFilterIsActive(const SidebarFilterQuery& filter) noexcept
{
    return filter.scope != SidebarFilterScope::All || !filter.text.empty();
}

/** @brief Per-entity validation-problem presence folded from the navigable diagnostics list. */
struct SidebarProblemSummary
{
    std::size_t total = 0;
    std::unordered_set<int> pages;
    std::unordered_set<std::string> libraryReticles;
};

/**
 * @brief Folds the diagnostics list into per-page and per-library-reticle presence sets.
 * @param problems Validation diagnostics carrying the same `contextId` strings the Problems panel uses.
 * @param normalizedPageIds Normalized page identifiers indexed like the authored pages, in the exact
 *                          form embedded in problem contexts (see @ref MatchProblemPageIndex).
 * @return A summary with the total count and the pages / library reticles that carry at least one problem.
 * @note Page-scoped contexts (including page reticles) resolve to a page index; top-level `reticle/<id>`
 *       contexts resolve to a library reticle. Non-entity contexts (`"window"`) only feed the total.
 */
[[nodiscard]] inline SidebarProblemSummary SummarizeSidebarProblems(
    const std::vector<PagePreviewProblem>& problems,
    const std::vector<std::string>& normalizedPageIds)
{
    constexpr std::string_view kReticlePrefix = "reticle/";

    SidebarProblemSummary summary;
    summary.total = problems.size();
    for (const PagePreviewProblem& problem : problems)
    {
        const int pageIndex = MatchProblemPageIndex(normalizedPageIds, problem.contextId);
        if (pageIndex >= 0)
        {
            summary.pages.insert(pageIndex);
            continue;
        }

        if (problem.contextId.size() > kReticlePrefix.size() &&
            problem.contextId.compare(0, kReticlePrefix.size(), kReticlePrefix) == 0)
        {
            summary.libraryReticles.insert(problem.contextId.substr(kReticlePrefix.size()));
        }
    }

    return summary;
}

/** @brief Returns whether the page at @p pageIndex carries at least one validation problem. */
[[nodiscard]] inline bool SidebarProblemSummaryHasPage(const SidebarProblemSummary& summary, const int pageIndex)
{
    return summary.pages.find(pageIndex) != summary.pages.end();
}

/** @brief Returns whether the library reticle with normalized id @p normalizedId carries a problem. */
[[nodiscard]] inline bool SidebarProblemSummaryHasReticle(const SidebarProblemSummary& summary,
                                                          const std::string_view normalizedId)
{
    return summary.libraryReticles.find(std::string(normalizedId)) != summary.libraryReticles.end();
}
} // namespace editor
