#pragma once

#include <type_traits>

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

inline bool CommandUsesGeneratedIdentifiers(const UserCommand& command) noexcept
{
    return std::visit(
        [](const auto& value) noexcept -> bool
        {
            using Command = std::decay_t<decltype(value)>;

            if constexpr (std::is_same_v<Command, ActivatePageCommand> ||
                          std::is_same_v<Command, SetPageViewCommand>)
            {
                return value.pageId != 0;
            }
            else if constexpr (std::is_same_v<Command, UpdateStrobeCommand>)
            {
                return value.pageId != 0 || value.strobeId != 0;
            }
            else if constexpr (std::is_same_v<Command, UpdateReticleCommand>)
            {
                return StaticHandleUsesGeneratedIdentifiers(value.target) ||
                       PatchUsesGeneratedIdentifiers(value.patch);
            }
            else if constexpr (std::is_same_v<Command, UpsertDynamicReticleCommand>)
            {
                return DynamicHandleUsesGeneratedIdentifiers(value.target) ||
                       value.templateTransportId != 0 ||
                       PatchUsesGeneratedIdentifiers(value.patch);
            }
            else if constexpr (std::is_same_v<Command, UpsertDynamicReticlesCommand>)
            {
                if (value.pageId != 0 || value.templateTransportId != 0)
                {
                    return true;
                }

                for (const DynamicReticleState& state : value.reticles)
                {
                    if (PatchUsesGeneratedIdentifiers(state.patch))
                    {
                        return true;
                    }
                }

                return false;
            }
            else if constexpr (std::is_same_v<Command, SetDynamicReticleSetVisibilityCommand>)
            {
                return value.pageId != 0 || value.templateTransportId != 0;
            }
            else if constexpr (std::is_same_v<Command, SetDynamicReticleSetStrobeMagnetEnabledCommand>)
            {
                return value.pageId != 0 || value.templateTransportId != 0;
            }
            else if constexpr (std::is_same_v<Command, RemoveDynamicReticleCommand>)
            {
                return DynamicHandleUsesGeneratedIdentifiers(value.target);
            }
            else
            {
                return false;
            }
        },
        command);
}
} // namespace mfd::detail
