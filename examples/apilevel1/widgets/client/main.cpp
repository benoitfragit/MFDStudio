#include "CockpitDemoController.h"
#include "WidgetsExampleUi.h"

#include "mfd/control/FeedbackTransport.h"
#include "mfd/io/JsonLoader.h"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace
{
bool IsProposal(const mfd::api1::WidgetEvent event) noexcept
{
    return event == mfd::api1::WidgetEvent::Activated ||
           event == mfd::api1::WidgetEvent::CheckedChangeRequested ||
           event == mfd::api1::WidgetEvent::SelectionChangeRequested ||
           event == mfd::api1::WidgetEvent::ValueChangeRequested;
}

void AppendPrivateConfirmation(std::vector<mfd::UserCommand>& commands,
                               const mfd::api1::WidgetFeedback& feedback)
{
    if (!IsProposal(feedback.event))
    {
        return;
    }

    mfd::api1::UpdateWidgetCommand confirmation;
    confirmation.target.pageId = feedback.pageId;
    confirmation.target.widgetTransportId = feedback.widgetId;
    confirmation.patch.requestId = feedback.requestId;
    if (feedback.event == mfd::api1::WidgetEvent::ValueChangeRequested)
    {
        confirmation.patch.value = feedback.value;
    }
    if (feedback.event == mfd::api1::WidgetEvent::CheckedChangeRequested ||
        feedback.event == mfd::api1::WidgetEvent::SelectionChangeRequested)
    {
        confirmation.patch.checked = feedback.checked;
    }
    commands.emplace_back(std::move(confirmation));
}

void SendPendingCommands(widgets_example::WidgetsExampleUi& ui,
                         mfd::CommandClient& client,
                         std::vector<mfd::UserCommand> privateConfirmations)
{
    std::vector<mfd::UserCommand> commands = ui.BuildBatch();
    commands.reserve(commands.size() + privateConfirmations.size());
    for (mfd::UserCommand& confirmation : privateConfirmations)
    {
        commands.push_back(std::move(confirmation));
    }
    if (!commands.empty() && !client.SendBatch(commands))
    {
        throw std::runtime_error(client.LastError());
    }
}

int RunCockpit()
{
    const auto loaded = mfd::JsonLoader {}.LoadWindowConfiguration("assets/windows/widgets_window.json");
    if (!loaded.generatedTransportMap || !loaded.window.commandTransports.udp ||
        !loaded.window.feedbackTransports.udp)
    {
        throw std::runtime_error("This example requires its generated map and UDP commands/feedback.");
    }

    mfd::CommandClient client(loaded.window.commandTransports, loaded.generatedTransportMap);
    auto feedback = mfd::CreateFeedbackReceiverChannel(*loaded.window.feedbackTransports.udp);
    if (!feedback->IsReady())
    {
        throw std::runtime_error(feedback->LastError());
    }

    widgets_example::WidgetsExampleUi ui;
    if (!ui.SendStartup(client, {}, {}))
    {
        throw std::runtime_error(client.LastError());
    }

    widgets_example::CockpitDemoController cockpit;
    cockpit.Initialize(ui);
    SendPendingCommands(ui, client, {});
    std::cout << "Live fuel-management cockpit active: instruments publish at 10 Hz. "
                 "Use XFER TEST, crossfeed valves, BINGO and transfer-rate controls.\n";

    const auto end = std::chrono::steady_clock::now() + std::chrono::minutes(10);
    auto previousUpdate = std::chrono::steady_clock::now();
    while (!ui.WindowReportedClosing() && std::chrono::steady_clock::now() < end)
    {
        std::string error;
        ui.PollFeedback(*feedback, 128U, &error);
        if (!error.empty())
        {
            throw std::runtime_error(error);
        }

        std::vector<mfd::UserCommand> privateConfirmations;
        const std::vector<mfd::api1::WidgetFeedback> events = ui.TakeWidgetEvents();
        privateConfirmations.reserve(events.size());
        for (const mfd::api1::WidgetFeedback& event : events)
        {
            if (!cockpit.HandleFeedback(ui, event))
            {
                AppendPrivateConfirmation(privateConfirmations, event);
            }
        }

        const auto now = std::chrono::steady_clock::now();
        const double elapsedSeconds = std::chrono::duration<double>(now - previousUpdate).count();
        previousUpdate = now;
        const bool instrumentsUpdated = cockpit.Advance(ui, elapsedSeconds);
        if (instrumentsUpdated || !events.empty())
        {
            SendPendingCommands(ui, client, std::move(privateConfirmations));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return 0;
}
} // namespace

int main()
{
    try
    {
        return RunCockpit();
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
