#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "mfd/control/CommandClient.h"

/**
 * @file
 * @brief Lightweight handwritten UI facade used by `mfd_mockup_minimal`.
 *
 * This layer intentionally stays local to the example. It hides most page,
 * reticle and blink string identifiers behind typed helper instances while
 * still emitting the exact same public `mfd::UserCommand` objects as the
 * low-level client API.
 */

namespace mockup_ui
{
/**
 * @brief Example-only namespace exposing a typed facade over the cockpit page.
 */

/**
 * @brief Strongly named page-managed blink type.
 *
 * Instances are owned by the page helper so reticles can select a blink mode
 * without repeating string literals in the simulation loop.
 */
class BlinkType
{
public:
    /** @brief Creates a named blink type wrapper. */
    explicit BlinkType(std::string_view name);

    /** @brief Returns the page-local blink type name used by the runtime patch. */
    const std::string& Name() const noexcept;

private:
    std::string name_;
};

/**
 * @brief Assignable blink slot attached to one reticle helper.
 *
 * The slot mirrors the page-managed blink model:
 *
 * - assign a `BlinkType` instance to start blinking with that type
 * - assign `nullptr` to stop blinking on that reticle
 *
 * Typical usage:
 *
 * @code{.cpp}
 * cockpit.hudMachBox.Blink = cockpit.overspeed;
 * cockpit.hudMachBox.Blink = nullptr;
 * @endcode
 */
class ReticleBlink
{
public:
    /** @brief Binds the slot to the patch stored by one reticle helper. */
    ReticleBlink(mfd::ReticlePatch& patch, bool* dirty) noexcept;

    /** @brief Enables blinking with the given page-managed blink type. */
    ReticleBlink& operator=(const BlinkType& blinkType);
    /** @brief Stops blinking for the owning reticle. */
    ReticleBlink& operator=(std::nullptr_t);

    /** @brief Enables or disables blinking without changing the stored blink type. */
    void SetEnabled(bool enabled);
    /** @brief Enables the given blink type when `enabled` is true, otherwise stops blinking. */
    void Set(bool enabled, const BlinkType& blinkType);
    /** @brief Enables blinking with the given page-managed blink type. */
    void Use(const BlinkType& blinkType);
    /** @brief Stops blinking and clears the reticle-specific type override. */
    void Disable();
    /** @brief Clears the reticle-specific blink type while keeping the enable flag unchanged. */
    void ClearType();

private:
    void MarkDirty() noexcept;

    mfd::ReticlePatch* patch_ = nullptr;
    bool* dirty_ = nullptr;
};

/**
 * @brief Helper wrapping one static reticle of a page.
 *
 * The helper owns the desired patch state for that reticle across frames.
 * Each simulation cycle starts with `Reset()`, then the caller modifies the
 * helper directly. At the end of the cycle, `AppendCommands()` emits one
 * `UpdateReticleCommand` only if the resulting patch changed compared to the
 * last batch sent to the client.
 */
class Reticle
{
public:
    /** @brief Creates a reticle helper bound to one page name and reticle id. */
    Reticle(std::string_view pageName, std::string_view reticleId);

    /** @brief Clears the per-cycle dirty flag before the next simulation update. */
    void Reset() noexcept;

    /** @brief Overrides reticle visibility. */
    void SetVisible(bool visible);
    /** @brief Enables or disables blinking. */
    void SetBlinkEnabled(bool enabled);
    /** @brief Enables the given blink type when `enabled` is true, otherwise stops blinking. */
    void SetBlink(bool enabled, const BlinkType& blinkType);
    /** @brief Enables the given page-managed blink type. */
    void SetBlinkType(const BlinkType& blinkType);
    /** @brief Clears the reticle-specific blink type override. */
    void ClearBlinkType();
    /** @brief Overrides reticle logical position. */
    void SetPosition(mfd::Vec2 position);
    /** @brief Overrides reticle rotation in degrees. */
    void SetRotationDegrees(float rotationDegrees);
    /** @brief Overrides reticle stroke color. */
    void SetColor(mfd::ColorRgba color);
    /** @brief Overrides reticle stroke thickness. */
    void SetThickness(float thickness);
    /** @brief Overrides the first text primitive value. */
    void SetText(std::string value);
    /** @brief Overrides one named text primitive value. */
    void SetText(std::string_view primitiveId, std::string value);
    /** @brief Overrides the first text primitive letter spacing. */
    void SetLetterSpacing(float letterSpacing);
    /** @brief Overrides letter spacing for one named text primitive. */
    void SetLetterSpacing(std::string_view primitiveId, float letterSpacing);

    /**
     * @brief Emits one static reticle command when the desired state changed.
     * @param commands Output batch receiving typed user commands.
     * @return `true` when one command was appended.
     */
    bool AppendCommands(std::vector<mfd::UserCommand>& commands);

protected:
    const mfd::ReticlePatch& DesiredPatch() const noexcept;

private:
    std::string pageName_;
    std::string reticleId_;
    mfd::ReticlePatch desiredPatch_ {};
    mfd::ReticlePatch lastSentPatch_ {};
    bool dirty_ = false;

public:
    /** @brief Assignable blink slot used as `reticle.Blink = page.overspeed;`. */
    ReticleBlink Blink;
};

/**
 * @brief Static reticle helper bound to one named text primitive.
 *
 * This is the small convenience wrapper used for label/value boxes so the
 * simulation loop can set a value directly without repeating the primitive id.
 */
class TextReticle final : public Reticle
{
public:
    /** @brief Creates a text reticle helper bound to one primitive id. */
    TextReticle(std::string_view pageName, std::string_view reticleId, std::string_view primitiveId);

    /** @brief Updates the bound text primitive value. */
    void SetValue(std::string value);

private:
    std::string primitiveId_;
};

/**
 * @brief Helper wrapping one dynamic reticle instance created from a template.
 *
 * The public API intentionally mirrors `Reticle` so static and dynamic updates
 * look the same from the simulation loop.
 */
class DynamicReticle
{
public:
    /** @brief Creates a helper for one public dynamic reticle id. */
    explicit DynamicReticle(std::string_view reticleId);

    /** @brief Clears the per-cycle seen flag before the next simulation update. */
    void Reset() noexcept;

    /** @brief Returns the public dynamic reticle id. */
    const std::string& Id() const noexcept;

    /** @brief Overrides reticle visibility. */
    void SetVisible(bool visible);
    /** @brief Enables or disables blinking. */
    void SetBlinkEnabled(bool enabled);
    /** @brief Enables the given blink type when `enabled` is true, otherwise stops blinking. */
    void SetBlink(bool enabled, const BlinkType& blinkType);
    /** @brief Enables the given page-managed blink type. */
    void SetBlinkType(const BlinkType& blinkType);
    /** @brief Clears the reticle-specific blink type override. */
    void ClearBlinkType();
    /** @brief Overrides reticle logical position. */
    void SetPosition(mfd::Vec2 position);
    /** @brief Overrides reticle rotation in degrees. */
    void SetRotationDegrees(float rotationDegrees);
    /** @brief Overrides reticle stroke color. */
    void SetColor(mfd::ColorRgba color);
    /** @brief Overrides reticle stroke thickness. */
    void SetThickness(float thickness);
    /** @brief Overrides the first text primitive value. */
    void SetText(std::string value);
    /** @brief Overrides one named text primitive value. */
    void SetText(std::string_view primitiveId, std::string value);
    /** @brief Overrides the first text primitive letter spacing. */
    void SetLetterSpacing(float letterSpacing);
    /** @brief Overrides letter spacing for one named text primitive. */
    void SetLetterSpacing(std::string_view primitiveId, float letterSpacing);

private:
    friend class DynamicReticleSet;

    const mfd::ReticlePatch& DesiredPatch() const noexcept;

    std::string reticleId_;
    mfd::ReticlePatch desiredPatch_ {};
    mfd::ReticlePatch lastSentPatch_ {};
    bool dirty_ = false;
    bool seenThisCycle_ = false;
    bool published_ = false;

public:
    /** @brief Assignable blink slot used as `reticle.Blink = page.threat;`. */
    ReticleBlink Blink;
};

/**
 * @brief Bulk manager for dynamic reticles sharing one page and one template.
 *
 * Each cycle starts with `Reset()`. The caller then touches only the reticles
 * that should exist for that frame through `Upsert()`. At batch build time the
 * helper:
 *
 * - emits one `UpsertDynamicReticlesCommand` for new or changed instances
 * - emits `RemoveDynamicReticleCommand` for previously published instances
 *   that were not touched during the current cycle
 */
class DynamicReticleSet
{
public:
    /** @brief Creates a set bound to one page name and one template id. */
    DynamicReticleSet(std::string_view pageName, std::string_view templateId);

    /** @brief Starts a new simulation cycle for the whole dynamic set. */
    void Reset() noexcept;
    /**
     * @brief Returns the helper for one dynamic reticle, creating it on demand.
     * @param reticleId Public dynamic reticle id.
     * @return Dynamic reticle helper to mutate for the current cycle.
     */
    DynamicReticle& Upsert(std::string_view reticleId);
    /**
     * @brief Emits create, update and remove commands for the current cycle.
     * @param commands Output batch receiving typed user commands.
     * @return Number of dynamic reticles that caused command emission.
     */
    std::size_t AppendCommands(std::vector<mfd::UserCommand>& commands);
    /**
     * @brief Removes every currently published dynamic reticle from the page.
     * @param commands Output batch receiving typed user commands.
     * @return Number of removed dynamic reticles.
     */
    std::size_t AppendRemovalCommands(std::vector<mfd::UserCommand>& commands);

private:
    DynamicReticle* Find(std::string_view reticleId) noexcept;

    std::string pageName_;
    std::string templateId_;
    std::vector<std::unique_ptr<DynamicReticle>> reticles_ {};
};

/**
 * @brief Helper wrapping whole-window display state for the example client.
 */
class WindowDisplay
{
public:
    /** @brief Clears the per-cycle dirty flag before the next simulation update. */
    void Reset() noexcept;

    /** @brief Enables or disables whole-window color inversion. */
    void SetColorInverted(bool invertColors);
    /** @brief Sets whole-window brightness in the `[0, 1]` range. */
    void SetBrightness(float brightness);
    /** @brief Enables or disables whole-window blackout. */
    void SetDisabled(bool disabled);

    /**
     * @brief Emits one window display command when the state changed.
     * @param commands Output batch receiving typed user commands.
     * @return `true` when one command was appended.
     */
    bool AppendCommands(std::vector<mfd::UserCommand>& commands);

private:
    mfd::WindowDisplayPatch desiredPatch_ {};
    mfd::WindowDisplayPatch lastSentPatch_ {};
    bool dirty_ = false;
};

/**
 * @brief Typed facade over the cockpit composite page used by the minimal mockup.
 *
 * This helper centralizes three kinds of identifiers that would otherwise stay
 * as string literals in the simulation loop:
 *
 * - the page name
 * - the static reticle ids
 * - the page-managed blink types
 *
 * The public members are intentionally exposed as direct instances so the loop
 * can stay terse and readable:
 *
 * @code{.cpp}
 * cockpit.hudMachBox.SetValue(machText);
 * cockpit.hudMachBox.Blink = cockpit.overspeed;
 * cockpit.hudMachBox.Blink = nullptr;
 * @endcode
 */
class CockpitMockupPage
{
public:
    /** @brief Returns the authored cockpit page name. */
    static constexpr std::string_view Name() noexcept
    {
        return "Cockpit";
    }

    /** @brief Returns the dynamic radar contact template id used by the example. */
    static constexpr std::string_view RadarTemplateId() noexcept
    {
        return "cockpit_radar_contact";
    }

    /** @brief Builds the typed cockpit page facade. */
    CockpitMockupPage();

    /** @brief Starts a new simulation cycle for every page helper. */
    void Reset() noexcept;
    /**
     * @brief Appends every changed static or dynamic cockpit command.
     * @param commands Output batch receiving typed user commands.
     * @return Number of reticle helpers that emitted commands.
     */
    std::size_t AppendCommands(std::vector<mfd::UserCommand>& commands);
    /**
     * @brief Appends the shutdown banner update and removes published contacts.
     * @param commands Output batch receiving typed user commands.
     * @param statusText Final cockpit status line shown before stopping.
     * @return Number of helpers that emitted commands.
     */
    std::size_t AppendShutdownCommands(std::vector<mfd::UserCommand>& commands, std::string statusText);

    /** @brief Updates the status line shown at the bottom of the cockpit page. */
    void SetStatusCaption(std::string value);

    /** @brief Blink type used when the HUD enters overspeed state. */
    BlinkType overspeed {"overspeed"};
    /** @brief Blink type used by threat-marked radar contacts. */
    BlinkType threat {"threat"};

    Reticle adiBallSky;
    Reticle adiBallGround;
    Reticle adiBallHorizon;
    Reticle adiBallLadder;
    Reticle adiHeadingBox;
    Reticle adiHeadingCard;
    Reticle adiHeadingCommandBug;
    TextReticle adiPitchBox;
    TextReticle adiRollBox;

    Reticle hudPitchLadder;
    Reticle hudVelocityVector;
    TextReticle hudSpeedBox;
    TextReticle hudMachBox;
    TextReticle hudHeadingBox;
    TextReticle hudFpaBox;
    TextReticle hudThrottleBox;
    TextReticle hudRadarBox;

    Reticle radarScope;
    Reticle radarSweep;
    Reticle radarOwnship;
    TextReticle radarHeadingBox;
    TextReticle radarSpeedBox;
    TextReticle radarStatusBox;
    Reticle radarOffOverlay;
    TextReticle cockpitStatus;

    DynamicReticleSet radarContacts;
};

/**
 * @brief Top-level facade used by `mfd_mockup_minimal`.
 *
 * It groups the window-level helper and the cockpit page helper, exposes a
 * one-shot startup routine, then builds one command batch per simulation tick.
 */
class CockpitMockupUi
{
public:
    /** @brief Returns the cockpit demo window JSON file used by the example. */
    static constexpr std::string_view WindowFile() noexcept
    {
        return "assets/windows/demo_pages_cockpit.json";
    }

    /** @brief Builds the top-level mockup facade with default window state. */
    CockpitMockupUi();

    /**
     * @brief Sends the one-time startup commands required by the example.
     * @param client Command client connected to the cockpit demo window.
     * @param view Authored view loaded from the cockpit page definition.
     * @param statusText Initial status line displayed before the first frame.
     * @return `true` if every startup command was sent successfully.
     */
    bool SendStartup(mfd::CommandClient& client, const mfd::PageViewState& view, std::string statusText);

    /** @brief Starts a new simulation cycle for the whole helper tree. */
    void Reset() noexcept;
    /** @brief Builds one batch containing every changed command for the current frame. */
    std::vector<mfd::UserCommand> BuildBatch();
    /** @brief Builds the final shutdown batch used when the example stops. */
    std::vector<mfd::UserCommand> BuildShutdownBatch(std::string statusText);

    /** @brief Returns the whole-window display helper. */
    WindowDisplay& Window() noexcept;
    /** @brief Returns the typed cockpit page helper. */
    CockpitMockupPage& Cockpit() noexcept;

private:
    WindowDisplay window_ {};
    CockpitMockupPage cockpit_ {};
};
} // namespace mockup_ui
