#include "mfd/client/Widget.h"
#include <cmath>
#include <stdexcept>
#include <utility>
namespace mfd::client
{
Widget::Widget(const std::string_view page, const std::string_view id, const TransportId pageId,
               const TransportId widgetId, const bool visible, const bool enabled, const bool checked,
               const bool warning, const double value, std::string text, const double minimum, const double maximum)
    : target_ {std::string(page), std::string(id), pageId, widgetId}, minimum_(minimum), maximum_(maximum)
{
    authored_.visible = visible;
    authored_.enabled = enabled;
    authored_.checked = checked;
    authored_.warning = warning;
    authored_.value = value;
    authored_.text = std::move(text);
}
void Widget::SetVisible(const bool value) noexcept { pending_.visible = value; }
void Widget::SetEnabled(const bool value) noexcept { pending_.enabled = value; }
void Widget::SetWarning(const bool value) noexcept { pending_.warning = value; }
void Widget::SetCheckedState(const bool value) noexcept { pending_.checked = value; }
void Widget::SetNumericValue(const double value)
{
    if (!std::isfinite(value) || value < minimum_ || value > maximum_) throw std::invalid_argument("Widget value outside authored range");
    pending_.value = value;
}
void Widget::SetText(std::string value)
{
    if (value.size() > 4096U || value.find('\0') != std::string::npos) throw std::invalid_argument("Invalid widget text");
    pending_.text = std::move(value);
}
void Widget::Acknowledge(const std::uint64_t requestId) noexcept { pending_.requestId = requestId; }
bool Widget::Matches(const api1::WidgetFeedback& feedback) const noexcept
{
    return feedback.pageId == target_.pageId && feedback.widgetId == target_.widgetTransportId;
}
const api1::WidgetHandle& Widget::Target() const noexcept { return target_; }
void Widget::ClearDirty() noexcept { pending_ = {}; }
void Widget::ResetToAuthored() { pending_ = authored_; }
bool Widget::AppendCommands(std::vector<UserCommand>& commands)
{
    if (!pending_.visible && !pending_.enabled && !pending_.checked && !pending_.warning && !pending_.value && !pending_.text && !pending_.requestId)
        return false;
    commands.emplace_back(api1::UpdateWidgetCommand {target_, pending_});
    ClearDirty();
    return true;
}
void CheckedWidget::SetChecked(const bool value) noexcept { SetCheckedState(value); }
void NumericWidget::SetValue(const double value) { SetNumericValue(value); }
}
