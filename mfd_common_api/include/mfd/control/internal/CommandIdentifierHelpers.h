#pragma once

#include <variant>

#include "mfd/control/CommandTypes.h"

namespace mfd::detail
{
inline bool PatchUsesGeneratedIdentifiers(const ReticlePatch& patch) noexcept
{
    return patch.blinkTypeId.has_value() ||
           !patch.textsById.empty() ||
           !patch.letterSpacingsById.empty() ||
           !patch.primitivePatchesById.empty();
}

inline bool StaticHandleUsesGeneratedIdentifiers(const StaticReticleHandle& handle) noexcept
{
    return handle.pageId != 0 || handle.reticleId != 0;
}

inline bool DynamicHandleUsesGeneratedIdentifiers(const DynamicReticleHandle& handle) noexcept
{
    return handle.pageId != 0;
}

/**
 * @brief Detects generated transport identifiers with one explicit overload per command type.
 *
 * @note This visitor intentionally has no generic fallback: adding a new `UserCommand`
 * alternative must fail to compile here until its generated-identifier rule is written.
 */
struct CommandGeneratedIdentifierProbe
{
    bool operator()(const ActivatePageCommand& command) const noexcept
    {
        return command.pageId != 0;
    }

    bool operator()(const SetPageViewCommand& command) const noexcept
    {
        return command.pageId != 0;
    }

    bool operator()(const UpdateWindowDisplayCommand&) const noexcept
    {
        return false;
    }

    bool operator()(const UpdateReticleCommand& command) const noexcept
    {
        return StaticHandleUsesGeneratedIdentifiers(command.target) ||
               PatchUsesGeneratedIdentifiers(command.patch);
    }

    bool operator()(const UpdateStrobeCommand& command) const noexcept
    {
        return command.pageId != 0 || command.strobeId != 0;
    }

    bool operator()(const UpsertDynamicReticleCommand& command) const noexcept
    {
        return DynamicHandleUsesGeneratedIdentifiers(command.target) ||
               command.templateTransportId != 0 ||
               PatchUsesGeneratedIdentifiers(command.patch);
    }

    bool operator()(const UpsertDynamicReticlesCommand& command) const noexcept
    {
        if (command.pageId != 0 || command.templateTransportId != 0)
        {
            return true;
        }

        for (const DynamicReticleState& state : command.reticles)
        {
            if (PatchUsesGeneratedIdentifiers(state.patch))
            {
                return true;
            }
        }

        return false;
    }

    bool operator()(const SetDynamicReticleSetVisibilityCommand& command) const noexcept
    {
        return command.pageId != 0 || command.templateTransportId != 0;
    }

    bool operator()(const SetDynamicReticleSetStrobeMagnetEnabledCommand& command) const noexcept
    {
        return command.pageId != 0 || command.templateTransportId != 0;
    }

    bool operator()(const RemoveDynamicReticleCommand& command) const noexcept
    {
        return DynamicHandleUsesGeneratedIdentifiers(command.target);
    }

    bool operator()(const ResetWindowCommand&) const noexcept
    {
        return false;
    }
};

inline bool CommandUsesGeneratedIdentifiers(const UserCommand& command) noexcept
{
    return std::visit(CommandGeneratedIdentifierProbe {}, command);
}
} // namespace mfd::detail
