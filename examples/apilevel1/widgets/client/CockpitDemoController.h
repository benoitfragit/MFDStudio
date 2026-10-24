#pragma once

/**
 * @file
 * @brief Deterministic low-frequency avionics simulation for the widgets example.
 */

#include "WidgetsExampleUi.h"

#include <cstdint>

namespace widgets_example
{
/**
 * @brief Drives the showcase instruments and confirms user input without owning transport I/O.
 *
 * The controller publishes at a bounded 10 Hz cadence. The main loop remains responsible for
 * feedback polling and batched command transmission.
 * @note Tank quantities and BINGO are published in kilograms; flow uses kilograms per minute.
 *       Each of the three tanks has a 2000 kg capacity. Refuelling stops automatically at capacity.
 */
class CockpitDemoController final
{
public:
    /**
     * @brief Seeds meaningful cockpit values before the first command batch is sent.
     * @param ui Generated typed user interface updated by the simulation.
     */
    void Initialize(WidgetsExampleUi& ui);

    /**
     * @brief Advances the deterministic simulation and stages at most one update per instrument.
     * @param ui Generated typed user interface receiving staged values.
     * @param elapsedSeconds Finite elapsed time since the previous call, clamped defensively.
     * @return `true` when a 10 Hz publication was staged and should be flushed as one batch.
     */
    [[nodiscard]] bool Advance(WidgetsExampleUi& ui, double elapsedSeconds);

    /**
     * @brief Confirms one exposed widget proposal and applies its cockpit-side effect.
     * @param ui Generated typed user interface receiving confirmation commands.
     * @param feedback Typed runtime feedback event.
     * @return `true` when the event targeted an exposed showcase widget.
     */
    [[nodiscard]] bool HandleFeedback(WidgetsExampleUi& ui,
                                      const mfd::api1::WidgetFeedback& feedback);

private:
    void BeginBuiltInTest(WidgetsExampleUi& ui, std::uint64_t requestId);
    void AdvanceFuelSystem(double elapsedSeconds) noexcept;
    void PublishInstruments(WidgetsExampleUi& ui);

    double publicationAccumulatorSeconds_ = 0.0;
    double builtInTestRemainingSeconds_ = 0.0;
    double leftTankPercent_ = 64.0;
    double centerTankPercent_ = 54.0;
    double rightTankPercent_ = 58.0;
    double bingoPercent_ = 35.0;
    double transferRatePercent_ = 45.0;
    double transferFlowPercent_ = 0.0;
    bool initialized_ = false;
    bool publishedBuiltInTestActive_ = false;
    bool centerPumpsEnabled_ = true;
    bool centerValveOpen_ = true;
    bool refuelActive_ = false;
    bool leftValveOpen_ = true;
    bool rightValveOpen_ = false;
};
} // namespace widgets_example
