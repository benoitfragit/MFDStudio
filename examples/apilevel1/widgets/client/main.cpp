#include "WidgetsExampleUi.h"
#include "mfd/control/FeedbackTransport.h"
#include "mfd/io/JsonLoader.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
namespace
{
void ConfirmProposal(widgets_example::WidgetsExampleUi& ui, mfd::CommandClient& client,
                     const mfd::api1::WidgetFeedback& event)
{
    using mfd::api1::WidgetEvent;
    if (event.event != WidgetEvent::Activated && event.event != WidgetEvent::CheckedChangeRequested &&
        event.event != WidgetEvent::SelectionChangeRequested && event.event != WidgetEvent::ValueChangeRequested) return;
    auto& page = ui.Widgets();
    if (page.ToggleAWidget().Matches(event))
    { page.ToggleAWidget().SetChecked(event.checked); page.ToggleAWidget().Acknowledge(event.requestId); }
    else if (page.ToggleBWidget().Matches(event))
    { page.ToggleBWidget().SetChecked(event.checked); page.ToggleBWidget().Acknowledge(event.requestId); }
    else if (page.RootToggleWidget().Matches(event))
    { page.RootToggleWidget().SetChecked(event.checked); page.RootToggleWidget().Acknowledge(event.requestId); }
    else if (page.SliderWidget().Matches(event))
    {
        // Demonstrate rejection without an application-specific rule: the last 10% is reserved.
        if (event.value <= 90)
        {
            page.SliderWidget().SetValue(event.value);
            page.GaugeWidget().SetValue(event.value);
            page.ProgressBarWidget().SetValue(event.value);
            page.NumericDisplayWidget().SetValue(event.value);
        }
        page.SliderWidget().Acknowledge(event.requestId);
    }
    else if (page.KnobWidget().Matches(event))
    { page.KnobWidget().SetValue(event.value); page.KnobWidget().Acknowledge(event.requestId); }
    else
    {
        // Private widgets use the same typed event stream and stable numeric target, without a public handle.
        mfd::api1::UpdateWidgetCommand confirmation;
        confirmation.target.pageId = event.pageId; confirmation.target.widgetTransportId = event.widgetId;
        confirmation.patch.requestId = event.requestId;
        if (event.event == WidgetEvent::ValueChangeRequested) confirmation.patch.value = event.value;
        if (event.event == WidgetEvent::CheckedChangeRequested || event.event == WidgetEvent::SelectionChangeRequested)
            confirmation.patch.checked = event.checked;
        if (!client.Send(confirmation)) throw std::runtime_error(client.LastError());
    }
    const auto commands = ui.BuildBatch();
    if (!commands.empty() && !client.SendBatch(commands)) throw std::runtime_error(client.LastError());
    std::cout << "Handled request " << event.requestId << " for widget " << event.widgetId << '\n';
}
}
int main()
{
    try
    {
        const auto loaded = mfd::JsonLoader {}.LoadWindowConfiguration("assets/windows/widgets_window.json");
        if (!loaded.generatedTransportMap || !loaded.window.commandTransports.udp || !loaded.window.feedbackTransports.udp)
            throw std::runtime_error("This example requires its generated map and UDP commands/feedback.");
        mfd::CommandClient client(loaded.window.commandTransports, loaded.generatedTransportMap);
        auto feedback = mfd::CreateFeedbackReceiverChannel(*loaded.window.feedbackTransports.udp);
        if (!feedback->IsReady()) throw std::runtime_error(feedback->LastError());
        widgets_example::WidgetsExampleUi ui;
        if (!ui.SendStartup(client, {}, {})) throw std::runtime_error(client.LastError());
        std::cout << "Widget client active. Toggle A and B share a definition but keep independent values.\n";
        // Bounded demonstration; closing the runtime also stops this loop.
        const auto end = std::chrono::steady_clock::now() + std::chrono::minutes(10);
        while (!ui.WindowReportedClosing() && std::chrono::steady_clock::now() < end)
        {
            std::string error;
            ui.PollFeedback(*feedback, 128, &error);
            if (!error.empty()) throw std::runtime_error(error);
            for (const auto& event : ui.TakeWidgetEvents()) ConfirmProposal(ui, client, event);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
