#include "CockpitDemoController.h"

#include <algorithm>
#include <cmath>

namespace widgets_example
{
namespace
{
constexpr double kPublicationPeriodSeconds = 0.1;
constexpr double kBuiltInTestDurationSeconds = 5.0;
constexpr double kMaximumTransferPercentPerSecond = 1.2;
constexpr double kMainTankConsumptionPercentPerSecond = 0.08;
constexpr double kTankCapacityKg = 2000.0;
constexpr double kTotalCapacityKg = 3.0 * kTankCapacityKg;
constexpr double kKgPerTankPercent = kTankCapacityKg / 100.0;
constexpr double kKgPerTotalPercent = kTotalCapacityKg / 100.0;
constexpr double kMaximumTransferKgPerMinute =
    2.0 * kMaximumTransferPercentPerSecond * kKgPerTankPercent * 60.0;
constexpr double kRefuelPercentPerSecond = 8.0;

bool IsProposal(const mfd::api1::WidgetEvent event) noexcept
{
    return event == mfd::api1::WidgetEvent::Activated ||
           event == mfd::api1::WidgetEvent::CheckedChangeRequested ||
           event == mfd::api1::WidgetEvent::SelectionChangeRequested ||
           event == mfd::api1::WidgetEvent::ValueChangeRequested;
}
} // namespace

void CockpitDemoController::Initialize(WidgetsExampleUi& ui)
{
    if (initialized_)
    {
        return;
    }

    WidgetsMockupPage& page = ui.Widgets();
    page.LeftTankWidget().SetValue(leftTankPercent_ * kKgPerTankPercent);
    page.ProgressBarWidget().SetValue(centerTankPercent_ * kKgPerTankPercent);
    page.RightTankWidget().SetValue(rightTankPercent_ * kKgPerTankPercent);
    page.LeftValveWidget().SetChecked(leftValveOpen_);
    page.RightValveWidget().SetChecked(rightValveOpen_);
    page.RootToggleWidget().SetChecked(centerValveOpen_);
    page.SliderWidget().SetValue(bingoPercent_ * kKgPerTotalPercent);
    page.KnobWidget().SetValue(transferRatePercent_);
    page.GaugeWidget().SetValue((leftTankPercent_ + centerTankPercent_ + rightTankPercent_) * kKgPerTankPercent);
    page.NumericDisplayWidget().SetValue(0.0);
    page.IndicatorWidget().SetWarning(false);
    page.IndicatorWidget().SetText("FUEL OK");
    page.LabelWidget().SetText("FUEL SYSTEM NORMAL");
    initialized_ = true;
}

bool CockpitDemoController::Advance(WidgetsExampleUi& ui, const double elapsedSeconds)
{
    if (!initialized_ || !std::isfinite(elapsedSeconds) || elapsedSeconds < 0.0)
    {
        return false;
    }

    const double boundedElapsed = std::min(elapsedSeconds, 0.25);
    publicationAccumulatorSeconds_ += boundedElapsed;
    builtInTestRemainingSeconds_ = std::max(0.0, builtInTestRemainingSeconds_ - boundedElapsed);
    AdvanceFuelSystem(boundedElapsed);
    if (publicationAccumulatorSeconds_ < kPublicationPeriodSeconds)
    {
        return false;
    }

    publicationAccumulatorSeconds_ = std::fmod(publicationAccumulatorSeconds_, kPublicationPeriodSeconds);
    PublishInstruments(ui);
    return true;
}

bool CockpitDemoController::HandleFeedback(WidgetsExampleUi& ui,
                                           const mfd::api1::WidgetFeedback& feedback)
{
    if (!IsProposal(feedback.event))
    {
        return false;
    }

    WidgetsMockupPage& page = ui.Widgets();
    if (page.RootActionWidget().Matches(feedback))
    {
        BeginBuiltInTest(ui, feedback.requestId);
    }
    else if (page.RefuelWidget().Matches(feedback))
    {
        refuelActive_ = !refuelActive_;
        page.RefuelWidget().SetText(refuelActive_ ? "STOP FILL" : "REFUEL");
        page.RefuelWidget().Acknowledge(feedback.requestId);
    }
    else if (page.ToggleAWidget().Matches(feedback))
    {
        page.ToggleAWidget().SetChecked(feedback.checked);
        page.ToggleAWidget().Acknowledge(feedback.requestId);
    }
    else if (page.ToggleBWidget().Matches(feedback))
    {
        page.ToggleBWidget().SetChecked(feedback.checked);
        page.ToggleBWidget().Acknowledge(feedback.requestId);
    }
    else if (page.CheckboxWidget().Matches(feedback))
    {
        centerPumpsEnabled_ = feedback.checked;
        page.CheckboxWidget().SetChecked(feedback.checked);
        page.CheckboxWidget().Acknowledge(feedback.requestId);
    }
    else if (page.RadioWidget().Matches(feedback))
    {
        page.RadioWidget().SetChecked(feedback.checked);
        page.RadioWidget().Acknowledge(feedback.requestId);
    }
    else if (page.RootToggleWidget().Matches(feedback))
    {
        centerValveOpen_ = feedback.checked;
        page.RootToggleWidget().SetChecked(centerValveOpen_);
        page.RootToggleWidget().Acknowledge(feedback.requestId);
    }
    else if (page.LeftValveWidget().Matches(feedback))
    {
        leftValveOpen_ = feedback.checked;
        page.LeftValveWidget().SetChecked(leftValveOpen_);
        page.LeftValveWidget().Acknowledge(feedback.requestId);
    }
    else if (page.RightValveWidget().Matches(feedback))
    {
        rightValveOpen_ = feedback.checked;
        page.RightValveWidget().SetChecked(rightValveOpen_);
        page.RightValveWidget().Acknowledge(feedback.requestId);
    }
    else if (page.SliderWidget().Matches(feedback))
    {
        if (!std::isfinite(feedback.value))
        {
            page.SliderWidget().Acknowledge(feedback.requestId);
            return true;
        }
        bingoPercent_ = std::clamp(feedback.value / kKgPerTotalPercent, 0.0, 100.0);
        page.SliderWidget().SetValue(bingoPercent_ * kKgPerTotalPercent);
        page.SliderWidget().Acknowledge(feedback.requestId);
    }
    else if (page.KnobWidget().Matches(feedback))
    {
        if (!std::isfinite(feedback.value))
        {
            page.KnobWidget().Acknowledge(feedback.requestId);
            return true;
        }
        transferRatePercent_ = std::clamp(feedback.value, 0.0, 100.0);
        page.KnobWidget().SetValue(transferRatePercent_);
        page.KnobWidget().Acknowledge(feedback.requestId);
    }
    else
    {
        return false;
    }
    return true;
}

void CockpitDemoController::BeginBuiltInTest(WidgetsExampleUi& ui,
                                             const std::uint64_t requestId)
{
    builtInTestRemainingSeconds_ = kBuiltInTestDurationSeconds;
    WidgetsMockupPage& page = ui.Widgets();
    page.RootActionWidget().Acknowledge(requestId);
    page.RootActionWidget().SetText("TEST ACTIVE");
    page.IndicatorWidget().SetWarning(true);
    page.LabelWidget().SetText("FUEL TEST IN PROGRESS");
    publishedBuiltInTestActive_ = true;
}

void CockpitDemoController::AdvanceFuelSystem(const double elapsedSeconds) noexcept
{
    transferFlowPercent_ = 0.0;
    if (refuelActive_)
    {
        const double supplied = kRefuelPercentPerSecond * elapsedSeconds;
        leftTankPercent_ = std::min(100.0, leftTankPercent_ + supplied);
        centerTankPercent_ = std::min(100.0, centerTankPercent_ + supplied);
        rightTankPercent_ = std::min(100.0, rightTankPercent_ + supplied);
        refuelActive_ = leftTankPercent_ < 100.0 || centerTankPercent_ < 100.0 || rightTankPercent_ < 100.0;
        return;
    }
    leftTankPercent_ = std::max(0.0, leftTankPercent_ -
                                        kMainTankConsumptionPercentPerSecond * elapsedSeconds);
    rightTankPercent_ = std::max(0.0, rightTankPercent_ -
                                         kMainTankConsumptionPercentPerSecond * elapsedSeconds);

    if (!centerPumpsEnabled_ || !centerValveOpen_ || centerTankPercent_ <= 0.0)
    {
        return;
    }

    const double transferPerValve = kMaximumTransferPercentPerSecond *
                                      transferRatePercent_ * 0.01 * elapsedSeconds;
    const double leftDemand = leftValveOpen_
                                  ? std::min(transferPerValve, 100.0 - leftTankPercent_)
                                  : 0.0;
    const double rightDemand = rightValveOpen_
                                   ? std::min(transferPerValve, 100.0 - rightTankPercent_)
                                   : 0.0;
    const double totalDemand = leftDemand + rightDemand;
    if (totalDemand <= 0.0)
    {
        return;
    }

    const double availableRatio = std::min(1.0, centerTankPercent_ / totalDemand);
    const double leftTransfer = leftDemand * availableRatio;
    const double rightTransfer = rightDemand * availableRatio;
    leftTankPercent_ += leftTransfer;
    rightTankPercent_ += rightTransfer;
    centerTankPercent_ = std::max(0.0, centerTankPercent_ - leftTransfer - rightTransfer);
    const double maximumTransferred = 2.0 * kMaximumTransferPercentPerSecond * elapsedSeconds;
    if (maximumTransferred > 0.0)
    {
        transferFlowPercent_ = 100.0 * (leftTransfer + rightTransfer) / maximumTransferred;
    }
}

void CockpitDemoController::PublishInstruments(WidgetsExampleUi& ui)
{
    WidgetsMockupPage& page = ui.Widgets();
    const double totalFuelPercent = (leftTankPercent_ + centerTankPercent_ + rightTankPercent_) / 3.0;
    page.LeftTankWidget().SetValue(std::clamp(leftTankPercent_, 0.0, 100.0) * kKgPerTankPercent);
    page.ProgressBarWidget().SetValue(std::clamp(centerTankPercent_, 0.0, 100.0) * kKgPerTankPercent);
    page.RightTankWidget().SetValue(std::clamp(rightTankPercent_, 0.0, 100.0) * kKgPerTankPercent);
    page.GaugeWidget().SetValue(std::clamp(totalFuelPercent, 0.0, 100.0) * kKgPerTotalPercent);
    page.NumericDisplayWidget().SetValue(std::clamp(transferFlowPercent_, 0.0, 100.0) *
                                       kMaximumTransferKgPerMinute / 100.0);
    page.RefuelWidget().SetText(refuelActive_ ? "STOP FILL" : "REFUEL");

    const bool testActive = builtInTestRemainingSeconds_ > 0.0;
    const bool bingoReached = totalFuelPercent <= bingoPercent_;
    page.IndicatorWidget().SetWarning(testActive || bingoReached);
    page.IndicatorWidget().SetText(bingoReached ? "BINGO" : "FUEL OK");
    const char* advisory = "FUEL SYSTEM NORMAL";
    if (testActive)
    {
        advisory = "FUEL TEST IN PROGRESS";
    }
    else if (refuelActive_)
    {
        advisory = "REFUEL IN PROGRESS";
    }
    else if (bingoReached)
    {
        advisory = "BINGO FUEL";
    }
    page.LabelWidget().SetText(advisory);
    if (testActive != publishedBuiltInTestActive_)
    {
        page.RootActionWidget().SetText(testActive ? "TEST ACTIVE" : "XFER TEST");
        publishedBuiltInTestActive_ = testActive;
    }
}
} // namespace widgets_example
