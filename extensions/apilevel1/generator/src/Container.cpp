/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

#include "mfd/client/Container.h"

#include <cmath>
#include <utility>

namespace mfd::client
{
Container::Container(const std::string_view page,
                     const std::string_view container,
                     const TransportId pageId,
                     const TransportId containerId,
                     const bool visible,
                     const bool enabled,
                     const Vec2 scrollOffset)
    : target_ {std::string(page), std::string(container), pageId, containerId},
      authoredVisible_(visible),
      authoredEnabled_(enabled),
      authoredScrollOffset_(scrollOffset),
      visible_(visible),
      enabled_(enabled),
      scrollOffset_(scrollOffset)
{
}

void Container::SetVisible(const bool visible) noexcept
{
    if (visible_ != visible)
    {
        visible_ = visible;
        dirtyFields_ |= VisibilityDirty;
    }
}

void Container::SetEnabled(const bool enabled) noexcept
{
    if (enabled_ != enabled)
    {
        enabled_ = enabled;
        dirtyFields_ |= EnabledDirty;
    }
}

void Container::SetScrollOffset(const Vec2 offset) noexcept
{
    if (!std::isfinite(offset.x) || !std::isfinite(offset.y) || offset.x < 0.0f || offset.y < 0.0f ||
        (scrollOffset_.x == offset.x && scrollOffset_.y == offset.y))
    {
        return;
    }
    scrollOffset_ = offset;
    dirtyFields_ |= ScrollDirty;
}

bool Container::GetVisible() const noexcept { return visible_; }
bool Container::GetEnabled() const noexcept { return enabled_; }
Vec2 Container::GetScrollOffset() const noexcept { return scrollOffset_; }
void Container::ClearDirty() noexcept { dirtyFields_ = 0U; }

void Container::ResetToAuthored() noexcept
{
    SetVisible(authoredVisible_);
    SetEnabled(authoredEnabled_);
    SetScrollOffset(authoredScrollOffset_);
}

bool Container::AppendCommands(std::vector<UserCommand>& commands)
{
    if (dirtyFields_ == 0U)
    {
        return false;
    }

    api1::UpdateContainerCommand command;
    command.target = target_;
    if ((dirtyFields_ & VisibilityDirty) != 0U) command.patch.visible = visible_;
    if ((dirtyFields_ & EnabledDirty) != 0U) command.patch.enabled = enabled_;
    if ((dirtyFields_ & ScrollDirty) != 0U) command.patch.scrollOffset = scrollOffset_;
    commands.emplace_back(std::move(command));
    ClearDirty();
    return true;
}
} // namespace mfd::client
