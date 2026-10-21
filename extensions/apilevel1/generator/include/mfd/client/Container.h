/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/** @file @brief Generated-client state handle for one exposed LEVEL 1 container. */

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "mfd/client/ClientExport.h"
#include "mfd/control/CommandTypes.h"

namespace mfd::client
{
/** @brief Accumulates delta commands for one generated container target. */
class MFD_CLIENT_API Container final
{
public:
    /**
     * @brief Constructs a generated container handle and its authored baseline.
     * @param page Authored page name.
     * @param container Authored page-local container identifier.
     * @param pageId Stable generated page transport identifier.
     * @param containerId Stable generated container transport identifier.
     * @param visible Authored visibility.
     * @param enabled Authored input-enabled state.
     * @param scrollOffset Authored scroll offset.
     */
    Container(std::string_view page,
              std::string_view container,
              TransportId pageId,
              TransportId containerId,
              bool visible,
              bool enabled,
              Vec2 scrollOffset);

    /** @brief Sets desired subtree visibility. */
    void SetVisible(bool visible) noexcept;
    /** @brief Sets desired subtree input availability. */
    void SetEnabled(bool enabled) noexcept;
    /** @brief Sets desired two-axis scroll offset. */
    void SetScrollOffset(Vec2 offset) noexcept;
    /** @brief Returns the desired visibility. */
    [[nodiscard]] bool GetVisible() const noexcept;
    /** @brief Returns the desired input-enabled state. */
    [[nodiscard]] bool GetEnabled() const noexcept;
    /** @brief Returns the desired scroll offset. */
    [[nodiscard]] Vec2 GetScrollOffset() const noexcept;
    /** @brief Discards pending deltas while retaining desired values. */
    void ClearDirty() noexcept;
    /** @brief Restores authored values and marks changed fields for transmission. */
    void ResetToAuthored() noexcept;
    /**
     * @brief Appends one partial update when at least one field is dirty.
     * @param commands Destination command list.
     * @return `true` when a command was appended.
     */
    bool AppendCommands(std::vector<UserCommand>& commands);

private:
    enum DirtyField : std::uint8_t
    {
        VisibilityDirty = 1U,
        EnabledDirty = 2U,
        ScrollDirty = 4U
    };

    api1::ContainerHandle target_ {};
    bool authoredVisible_ = true;
    bool authoredEnabled_ = true;
    Vec2 authoredScrollOffset_ {};
    bool visible_ = true;
    bool enabled_ = true;
    Vec2 scrollOffset_ {};
    std::uint8_t dirtyFields_ = 0U;
};
} // namespace mfd::client
