#pragma once
/** @file @brief Typed generated widget setters and explicit request acknowledgement. */
#include "mfd/client/ClientExport.h"
#include "mfd/control/CommandTypes.h"
#include <string_view>
namespace mfd::client
{
class MFD_CLIENT_API Widget
{
public:
    Widget(std::string_view page, std::string_view id, TransportId pageId, TransportId widgetId,
           bool visible, bool enabled, bool checked, bool warning, double value, std::string text,
           double minimum, double maximum);
    void SetVisible(bool value) noexcept;
    void SetEnabled(bool value) noexcept;
    void SetWarning(bool value) noexcept;
    void SetText(std::string value);
    /** @brief Acknowledge without changing confirmed state to reject a proposal. */
    void Acknowledge(std::uint64_t requestId) noexcept;
    [[nodiscard]] bool Matches(const api1::WidgetFeedback& feedback) const noexcept;
    [[nodiscard]] const api1::WidgetHandle& Target() const noexcept;
    void ClearDirty() noexcept;
    void ResetToAuthored();
    bool AppendCommands(std::vector<UserCommand>& commands);
protected:
    void SetCheckedState(bool value) noexcept;
    void SetNumericValue(double value);
private:
    api1::WidgetHandle target_;
    api1::WidgetStatePatch authored_;
    api1::WidgetStatePatch pending_;
    double minimum_;
    double maximum_;
};
class MFD_CLIENT_API CheckedWidget final : public Widget
{
public:
    using Widget::Widget;
    void SetChecked(bool value) noexcept;
};
class MFD_CLIENT_API NumericWidget final : public Widget
{
public:
    using Widget::Widget;
    void SetValue(double value);
};
}
