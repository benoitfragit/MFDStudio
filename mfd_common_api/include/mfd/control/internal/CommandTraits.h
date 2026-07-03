#pragma once

#include "mfd/control/CommandTypes.h"

/**
 * @file
 * @brief Central per-command identity accessors shared by the command pipeline stages.
 *
 * "Which fields carry the page identity of command X" and "which fields carry its
 * dynamic template identity" are rules needed by several pipeline stages (generated
 * identifier detection, UDP coalescing, transactional rollback footprint). This header
 * is their single source of truth, expressed as explicit overloads with no generic
 * fallback: a new `UserCommand` alternative must add its overloads here before any
 * consuming stage compiles again.
 *
 * @note The client transport normalization and runtime identifier resolution stages
 * intentionally keep direct field access: they mutate the identity fields in place and
 * pair them with stage-specific lookups, so routing them through these read-only views
 * would add indirection without removing duplication.
 */
namespace mfd::detail
{
/** @brief Read-only view of the page identity carried by one command. */
struct CommandPageBinding
{
    /** @brief Authored page name field, or `nullptr` when the command has no page scope. */
    const std::string* pageName = nullptr;
    /** @brief Generated page transport id, `0` when absent. */
    TransportId pageId = 0;
};

/** @brief Read-only view of the dynamic template identity carried by one command. */
struct CommandTemplateBinding
{
    /** @brief Authored template id field. */
    const std::string* templateId = nullptr;
    /** @brief Generated template transport id, `0` when absent. */
    TransportId templateTransportId = 0;
};

inline CommandPageBinding PageBindingOf(const ActivatePageCommand& command) noexcept
{
    return {&command.page, command.pageId};
}

inline CommandPageBinding PageBindingOf(const SetPageViewCommand& command) noexcept
{
    return {&command.page, command.pageId};
}

inline CommandPageBinding PageBindingOf(const UpdateWindowDisplayCommand&) noexcept
{
    return {};
}

inline CommandPageBinding PageBindingOf(const UpdateReticleCommand& command) noexcept
{
    return {&command.target.page, command.target.pageId};
}

inline CommandPageBinding PageBindingOf(const UpdateStrobeCommand& command) noexcept
{
    return {&command.page, command.pageId};
}

inline CommandPageBinding PageBindingOf(const UpsertDynamicReticleCommand& command) noexcept
{
    return {&command.target.page, command.target.pageId};
}

inline CommandPageBinding PageBindingOf(const UpsertDynamicReticlesCommand& command) noexcept
{
    return {&command.page, command.pageId};
}

inline CommandPageBinding PageBindingOf(const SetDynamicReticleSetVisibilityCommand& command) noexcept
{
    return {&command.page, command.pageId};
}

inline CommandPageBinding PageBindingOf(const SetDynamicReticleSetStrobeMagnetEnabledCommand& command) noexcept
{
    return {&command.page, command.pageId};
}

inline CommandPageBinding PageBindingOf(const RemoveDynamicReticleCommand& command) noexcept
{
    return {&command.target.page, command.target.pageId};
}

inline CommandPageBinding PageBindingOf(const ResetWindowCommand&) noexcept
{
    return {};
}

inline CommandTemplateBinding TemplateBindingOf(const UpsertDynamicReticleCommand& command) noexcept
{
    return {&command.templateId, command.templateTransportId};
}

inline CommandTemplateBinding TemplateBindingOf(const UpsertDynamicReticlesCommand& command) noexcept
{
    return {&command.templateId, command.templateTransportId};
}

inline CommandTemplateBinding TemplateBindingOf(const SetDynamicReticleSetVisibilityCommand& command) noexcept
{
    return {&command.templateId, command.templateTransportId};
}

inline CommandTemplateBinding TemplateBindingOf(const SetDynamicReticleSetStrobeMagnetEnabledCommand& command) noexcept
{
    return {&command.templateId, command.templateTransportId};
}
} // namespace mfd::detail
