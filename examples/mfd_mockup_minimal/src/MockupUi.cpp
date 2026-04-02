#include "MockupUi.h"

#include <algorithm>
#include <optional>
#include <utility>

namespace mockup_ui
{
namespace
{
bool Equal(const mfd::Vec2& lhs, const mfd::Vec2& rhs) noexcept
{
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

bool Equal(const mfd::ColorRgba& lhs, const mfd::ColorRgba& rhs) noexcept
{
    return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
}

template <typename T>
bool EqualOptional(const std::optional<T>& lhs, const std::optional<T>& rhs)
{
    return lhs == rhs;
}

template <>
bool EqualOptional(const std::optional<mfd::Vec2>& lhs, const std::optional<mfd::Vec2>& rhs)
{
    if (lhs.has_value() != rhs.has_value())
    {
        return false;
    }

    return !lhs.has_value() || Equal(*lhs, *rhs);
}

template <>
bool EqualOptional(const std::optional<mfd::ColorRgba>& lhs, const std::optional<mfd::ColorRgba>& rhs)
{
    if (lhs.has_value() != rhs.has_value())
    {
        return false;
    }

    return !lhs.has_value() || Equal(*lhs, *rhs);
}

bool Equal(const mfd::ReticlePatch& lhs, const mfd::ReticlePatch& rhs)
{
    return EqualOptional(lhs.visible, rhs.visible) &&
           EqualOptional(lhs.blinkEnabled, rhs.blinkEnabled) &&
           EqualOptional(lhs.blinkType, rhs.blinkType) &&
           EqualOptional(lhs.position, rhs.position) &&
           EqualOptional(lhs.rotationDegrees, rhs.rotationDegrees) &&
           EqualOptional(lhs.color, rhs.color) &&
           EqualOptional(lhs.thickness, rhs.thickness) &&
           EqualOptional(lhs.text, rhs.text) &&
           lhs.texts == rhs.texts &&
           EqualOptional(lhs.letterSpacing, rhs.letterSpacing) &&
           lhs.letterSpacings == rhs.letterSpacings;
}

bool Equal(const mfd::WindowDisplayPatch& lhs, const mfd::WindowDisplayPatch& rhs)
{
    return EqualOptional(lhs.invertColors, rhs.invertColors) &&
           EqualOptional(lhs.brightness, rhs.brightness) &&
           EqualOptional(lhs.disabled, rhs.disabled);
}

void PatchSetVisible(mfd::ReticlePatch& patch, const bool visible)
{
    patch.visible = visible;
}

void PatchSetBlinkEnabled(mfd::ReticlePatch& patch, const bool enabled)
{
    patch.blinkEnabled = enabled;
}

void PatchSetBlink(mfd::ReticlePatch& patch, const bool enabled, const std::string_view blinkType)
{
    patch.blinkEnabled = enabled;
    patch.blinkType = std::string(blinkType);
}

void PatchSetBlinkType(mfd::ReticlePatch& patch, const std::string_view blinkType)
{
    patch.blinkEnabled = true;
    patch.blinkType = std::string(blinkType);
}

void PatchClearBlinkType(mfd::ReticlePatch& patch)
{
    patch.blinkType = std::string {};
}

void PatchSetPosition(mfd::ReticlePatch& patch, const mfd::Vec2 position)
{
    patch.position = position;
}

void PatchSetRotationDegrees(mfd::ReticlePatch& patch, const float rotationDegrees)
{
    patch.rotationDegrees = rotationDegrees;
}

void PatchSetColor(mfd::ReticlePatch& patch, const mfd::ColorRgba color)
{
    patch.color = color;
}

void PatchSetThickness(mfd::ReticlePatch& patch, const float thickness)
{
    patch.thickness = thickness;
}

void PatchSetText(mfd::ReticlePatch& patch, std::string value)
{
    patch.text = std::move(value);
}

void PatchSetText(mfd::ReticlePatch& patch, const std::string_view primitiveId, std::string value)
{
    patch.texts[std::string(primitiveId)] = std::move(value);
}

void PatchSetLetterSpacing(mfd::ReticlePatch& patch, const float letterSpacing)
{
    patch.letterSpacing = letterSpacing;
}

void PatchSetLetterSpacing(mfd::ReticlePatch& patch,
                           const std::string_view primitiveId,
                           const float letterSpacing)
{
    patch.letterSpacings[std::string(primitiveId)] = letterSpacing;
}
} // namespace

BlinkType::BlinkType(const std::string_view name) :
    name_(name)
{
}

const std::string& BlinkType::Name() const noexcept
{
    return name_;
}

ReticleBlink::ReticleBlink(mfd::ReticlePatch& patch, bool* dirty) noexcept :
    patch_(&patch),
    dirty_(dirty)
{
}

ReticleBlink& ReticleBlink::operator=(const BlinkType& blinkType)
{
    Use(blinkType);
    return *this;
}

ReticleBlink& ReticleBlink::operator=(std::nullptr_t)
{
    Disable();
    return *this;
}

void ReticleBlink::SetEnabled(const bool enabled)
{
    PatchSetBlinkEnabled(*patch_, enabled);
    MarkDirty();
}

void ReticleBlink::Set(const bool enabled, const BlinkType& blinkType)
{
    if (enabled)
    {
        PatchSetBlink(*patch_, true, blinkType.Name());
    }
    else
    {
        PatchSetBlink(*patch_, false, {});
    }

    MarkDirty();
}

void ReticleBlink::Use(const BlinkType& blinkType)
{
    PatchSetBlinkType(*patch_, blinkType.Name());
    MarkDirty();
}

void ReticleBlink::Disable()
{
    PatchSetBlink(*patch_, false, {});
    MarkDirty();
}

void ReticleBlink::ClearType()
{
    PatchClearBlinkType(*patch_);
    MarkDirty();
}

void ReticleBlink::MarkDirty() noexcept
{
    if (dirty_ != nullptr)
    {
        *dirty_ = true;
    }
}

Reticle::Reticle(const std::string_view pageName, const std::string_view reticleId) :
    pageName_(pageName),
    reticleId_(reticleId),
    Blink(desiredPatch_, &dirty_)
{
}

void Reticle::Reset() noexcept
{
    dirty_ = false;
}

void Reticle::SetVisible(const bool visible)
{
    PatchSetVisible(desiredPatch_, visible);
    dirty_ = true;
}

void Reticle::SetBlinkEnabled(const bool enabled)
{
    Blink.SetEnabled(enabled);
}

void Reticle::SetBlink(const bool enabled, const BlinkType& blinkType)
{
    Blink.Set(enabled, blinkType);
}

void Reticle::SetBlinkType(const BlinkType& blinkType)
{
    Blink.Use(blinkType);
}

void Reticle::ClearBlinkType()
{
    Blink.ClearType();
}

void Reticle::SetPosition(const mfd::Vec2 position)
{
    PatchSetPosition(desiredPatch_, position);
    dirty_ = true;
}

void Reticle::SetRotationDegrees(const float rotationDegrees)
{
    PatchSetRotationDegrees(desiredPatch_, rotationDegrees);
    dirty_ = true;
}

void Reticle::SetColor(const mfd::ColorRgba color)
{
    PatchSetColor(desiredPatch_, color);
    dirty_ = true;
}

void Reticle::SetThickness(const float thickness)
{
    PatchSetThickness(desiredPatch_, thickness);
    dirty_ = true;
}

void Reticle::SetText(std::string value)
{
    PatchSetText(desiredPatch_, std::move(value));
    dirty_ = true;
}

void Reticle::SetText(const std::string_view primitiveId, std::string value)
{
    PatchSetText(desiredPatch_, primitiveId, std::move(value));
    dirty_ = true;
}

void Reticle::SetLetterSpacing(const float letterSpacing)
{
    PatchSetLetterSpacing(desiredPatch_, letterSpacing);
    dirty_ = true;
}

void Reticle::SetLetterSpacing(const std::string_view primitiveId, const float letterSpacing)
{
    PatchSetLetterSpacing(desiredPatch_, primitiveId, letterSpacing);
    dirty_ = true;
}

bool Reticle::AppendCommands(std::vector<mfd::UserCommand>& commands)
{
    if (!dirty_)
    {
        return false;
    }

    if (Equal(desiredPatch_, lastSentPatch_))
    {
        return false;
    }

    commands.emplace_back(mfd::UpdateReticleCommand {
        mfd::ReticleHandle {pageName_, reticleId_},
        desiredPatch_});
    lastSentPatch_ = desiredPatch_;
    return true;
}

const mfd::ReticlePatch& Reticle::DesiredPatch() const noexcept
{
    return desiredPatch_;
}

TextReticle::TextReticle(const std::string_view pageName,
                         const std::string_view reticleId,
                         const std::string_view primitiveId) :
    Reticle(pageName, reticleId),
    primitiveId_(primitiveId)
{
}

void TextReticle::SetValue(std::string value)
{
    SetText(primitiveId_, std::move(value));
}

DynamicReticle::DynamicReticle(const std::string_view reticleId) :
    reticleId_(reticleId),
    Blink(desiredPatch_, &dirty_)
{
}

void DynamicReticle::Reset() noexcept
{
    seenThisCycle_ = false;
}

const std::string& DynamicReticle::Id() const noexcept
{
    return reticleId_;
}

void DynamicReticle::SetVisible(const bool visible)
{
    PatchSetVisible(desiredPatch_, visible);
}

void DynamicReticle::SetBlinkEnabled(const bool enabled)
{
    Blink.SetEnabled(enabled);
}

void DynamicReticle::SetBlink(const bool enabled, const BlinkType& blinkType)
{
    Blink.Set(enabled, blinkType);
}

void DynamicReticle::SetBlinkType(const BlinkType& blinkType)
{
    Blink.Use(blinkType);
}

void DynamicReticle::ClearBlinkType()
{
    Blink.ClearType();
}

void DynamicReticle::SetPosition(const mfd::Vec2 position)
{
    PatchSetPosition(desiredPatch_, position);
}

void DynamicReticle::SetRotationDegrees(const float rotationDegrees)
{
    PatchSetRotationDegrees(desiredPatch_, rotationDegrees);
}

void DynamicReticle::SetColor(const mfd::ColorRgba color)
{
    PatchSetColor(desiredPatch_, color);
}

void DynamicReticle::SetThickness(const float thickness)
{
    PatchSetThickness(desiredPatch_, thickness);
}

void DynamicReticle::SetText(std::string value)
{
    PatchSetText(desiredPatch_, std::move(value));
}

void DynamicReticle::SetText(const std::string_view primitiveId, std::string value)
{
    PatchSetText(desiredPatch_, primitiveId, std::move(value));
}

void DynamicReticle::SetLetterSpacing(const float letterSpacing)
{
    PatchSetLetterSpacing(desiredPatch_, letterSpacing);
}

void DynamicReticle::SetLetterSpacing(const std::string_view primitiveId, const float letterSpacing)
{
    PatchSetLetterSpacing(desiredPatch_, primitiveId, letterSpacing);
}

const mfd::ReticlePatch& DynamicReticle::DesiredPatch() const noexcept
{
    return desiredPatch_;
}

DynamicReticleSet::DynamicReticleSet(const std::string_view pageName, const std::string_view templateId) :
    pageName_(pageName),
    templateId_(templateId)
{
}

void DynamicReticleSet::Reset() noexcept
{
    for (const auto& reticle : reticles_)
    {
        reticle->Reset();
    }
}

DynamicReticle& DynamicReticleSet::Upsert(const std::string_view reticleId)
{
    if (DynamicReticle* existing = Find(reticleId); existing != nullptr)
    {
        existing->seenThisCycle_ = true;
        return *existing;
    }

    reticles_.push_back(std::make_unique<DynamicReticle>(reticleId));
    reticles_.back()->seenThisCycle_ = true;
    return *reticles_.back();
}

std::size_t DynamicReticleSet::AppendCommands(std::vector<mfd::UserCommand>& commands)
{
    std::vector<mfd::DynamicReticleState> updates;
    updates.reserve(reticles_.size());
    std::size_t count = 0;

    for (const auto& reticle : reticles_)
    {
        if (reticle->seenThisCycle_)
        {
            if (!reticle->published_ || !Equal(reticle->desiredPatch_, reticle->lastSentPatch_))
            {
                updates.push_back(mfd::DynamicReticleState {reticle->reticleId_, reticle->desiredPatch_});
                reticle->lastSentPatch_ = reticle->desiredPatch_;
                ++count;
            }

            reticle->published_ = true;
            continue;
        }

        if (!reticle->published_)
        {
            continue;
        }

        commands.emplace_back(mfd::RemoveDynamicReticleCommand {
            mfd::ReticleHandle {pageName_, reticle->reticleId_}});
        reticle->published_ = false;
        ++count;
    }

    if (!updates.empty())
    {
        commands.emplace_back(mfd::UpsertDynamicReticlesCommand {
            pageName_,
            templateId_,
            std::move(updates)});
    }

    return count;
}

std::size_t DynamicReticleSet::AppendRemovalCommands(std::vector<mfd::UserCommand>& commands)
{
    std::size_t count = 0;

    for (const auto& reticle : reticles_)
    {
        if (!reticle->published_)
        {
            continue;
        }

        commands.emplace_back(mfd::RemoveDynamicReticleCommand {
            mfd::ReticleHandle {pageName_, reticle->reticleId_}});
        reticle->published_ = false;
        ++count;
    }

    return count;
}

DynamicReticle* DynamicReticleSet::Find(const std::string_view reticleId) noexcept
{
    const auto iterator = std::find_if(
        reticles_.begin(),
        reticles_.end(),
        [reticleId](const std::unique_ptr<DynamicReticle>& candidate)
        {
            return candidate->Id() == reticleId;
        });
    return iterator == reticles_.end() ? nullptr : iterator->get();
}

void WindowDisplay::Reset() noexcept
{
    dirty_ = false;
}

void WindowDisplay::SetColorInverted(const bool invertColors)
{
    desiredPatch_.invertColors = invertColors;
    dirty_ = true;
}

void WindowDisplay::SetBrightness(const float brightness)
{
    desiredPatch_.brightness = brightness;
    dirty_ = true;
}

void WindowDisplay::SetDisabled(const bool disabled)
{
    desiredPatch_.disabled = disabled;
    dirty_ = true;
}

bool WindowDisplay::AppendCommands(std::vector<mfd::UserCommand>& commands)
{
    if (!dirty_)
    {
        return false;
    }

    if (Equal(desiredPatch_, lastSentPatch_))
    {
        return false;
    }

    commands.emplace_back(mfd::UpdateWindowDisplayCommand {desiredPatch_});
    lastSentPatch_ = desiredPatch_;
    return true;
}

CockpitMockupPage::CockpitMockupPage() :
    adiBallSky(Name(), "adi_ball_sky"),
    adiBallGround(Name(), "adi_ball_ground"),
    adiBallHorizon(Name(), "adi_ball_horizon"),
    adiBallLadder(Name(), "adi_ball_ladder"),
    adiHeadingBox(Name(), "adi_heading_box"),
    adiHeadingCard(Name(), "adi_heading_card"),
    adiHeadingCommandBug(Name(), "adi_heading_command_bug"),
    adiPitchBox(Name(), "adi_pitch_box", "pitch_value"),
    adiRollBox(Name(), "adi_roll_box", "roll_value"),
    hudPitchLadder(Name(), "hud_pitch_ladder"),
    hudVelocityVector(Name(), "hud_velocity_vector"),
    hudSpeedBox(Name(), "hud_speed_box", "speed_value"),
    hudMachBox(Name(), "hud_mach_box", "mach_value"),
    hudHeadingBox(Name(), "hud_heading_box", "heading_value"),
    hudFpaBox(Name(), "hud_fpa_box", "fpa_value"),
    hudThrottleBox(Name(), "hud_throttle_box", "throttle_value"),
    hudRadarBox(Name(), "hud_radar_box", "radar_value"),
    radarScope(Name(), "radar_scope"),
    radarSweep(Name(), "radar_sweep"),
    radarOwnship(Name(), "radar_ownship"),
    radarHeadingBox(Name(), "radar_heading_box", "heading_value"),
    radarSpeedBox(Name(), "radar_speed_box", "speed_value"),
    radarStatusBox(Name(), "radar_status_box", "status_value"),
    radarOffOverlay(Name(), "radar_off_overlay"),
    cockpitStatus(Name(), "cockpit_status", "status_caption"),
    radarContacts(Name(), RadarTemplateId())
{
}

void CockpitMockupPage::Reset() noexcept
{
    adiBallSky.Reset();
    adiBallGround.Reset();
    adiBallHorizon.Reset();
    adiBallLadder.Reset();
    adiHeadingBox.Reset();
    adiHeadingCard.Reset();
    adiHeadingCommandBug.Reset();
    adiPitchBox.Reset();
    adiRollBox.Reset();

    hudPitchLadder.Reset();
    hudVelocityVector.Reset();
    hudSpeedBox.Reset();
    hudMachBox.Reset();
    hudHeadingBox.Reset();
    hudFpaBox.Reset();
    hudThrottleBox.Reset();
    hudRadarBox.Reset();

    radarScope.Reset();
    radarSweep.Reset();
    radarOwnship.Reset();
    radarHeadingBox.Reset();
    radarSpeedBox.Reset();
    radarStatusBox.Reset();
    radarOffOverlay.Reset();
    cockpitStatus.Reset();

    radarContacts.Reset();
}

std::size_t CockpitMockupPage::AppendCommands(std::vector<mfd::UserCommand>& commands)
{
    std::size_t count = 0;

    count += adiBallSky.AppendCommands(commands) ? 1U : 0U;
    count += adiBallGround.AppendCommands(commands) ? 1U : 0U;
    count += adiBallHorizon.AppendCommands(commands) ? 1U : 0U;
    count += adiBallLadder.AppendCommands(commands) ? 1U : 0U;
    count += adiHeadingBox.AppendCommands(commands) ? 1U : 0U;
    count += adiHeadingCard.AppendCommands(commands) ? 1U : 0U;
    count += adiHeadingCommandBug.AppendCommands(commands) ? 1U : 0U;
    count += adiPitchBox.AppendCommands(commands) ? 1U : 0U;
    count += adiRollBox.AppendCommands(commands) ? 1U : 0U;

    count += hudPitchLadder.AppendCommands(commands) ? 1U : 0U;
    count += hudVelocityVector.AppendCommands(commands) ? 1U : 0U;
    count += hudSpeedBox.AppendCommands(commands) ? 1U : 0U;
    count += hudMachBox.AppendCommands(commands) ? 1U : 0U;
    count += hudHeadingBox.AppendCommands(commands) ? 1U : 0U;
    count += hudFpaBox.AppendCommands(commands) ? 1U : 0U;
    count += hudThrottleBox.AppendCommands(commands) ? 1U : 0U;
    count += hudRadarBox.AppendCommands(commands) ? 1U : 0U;

    count += radarScope.AppendCommands(commands) ? 1U : 0U;
    count += radarSweep.AppendCommands(commands) ? 1U : 0U;
    count += radarOwnship.AppendCommands(commands) ? 1U : 0U;
    count += radarHeadingBox.AppendCommands(commands) ? 1U : 0U;
    count += radarSpeedBox.AppendCommands(commands) ? 1U : 0U;
    count += radarStatusBox.AppendCommands(commands) ? 1U : 0U;
    count += radarOffOverlay.AppendCommands(commands) ? 1U : 0U;
    count += cockpitStatus.AppendCommands(commands) ? 1U : 0U;

    count += radarContacts.AppendCommands(commands);
    return count;
}

std::size_t CockpitMockupPage::AppendShutdownCommands(std::vector<mfd::UserCommand>& commands, std::string statusText)
{
    std::size_t count = 0;

    cockpitStatus.SetValue(std::move(statusText));
    count += cockpitStatus.AppendCommands(commands) ? 1U : 0U;
    count += radarContacts.AppendRemovalCommands(commands);
    return count;
}

void CockpitMockupPage::SetStatusCaption(std::string value)
{
    cockpitStatus.SetValue(std::move(value));
}

CockpitMockupUi::CockpitMockupUi()
{
    window_.SetColorInverted(false);
    window_.SetBrightness(1.0f);
    window_.SetDisabled(false);
}

bool CockpitMockupUi::SendStartup(mfd::CommandClient& client,
                                  const mfd::PageViewState& view,
                                  std::string statusText)
{
    if (!client.ActivatePage(CockpitMockupPage::Name()))
    {
        return false;
    }

    if (!client.SetPageView(CockpitMockupPage::Name(), view.center, view.zoom))
    {
        return false;
    }

    std::vector<mfd::UserCommand> commands;
    commands.reserve(2);

    window_.AppendCommands(commands);
    cockpit_.SetStatusCaption(std::move(statusText));
    cockpit_.cockpitStatus.AppendCommands(commands);

    return commands.empty() || client.SendBatch(commands, 0);
}

void CockpitMockupUi::Reset() noexcept
{
    window_.Reset();
    cockpit_.Reset();
}

std::vector<mfd::UserCommand> CockpitMockupUi::BuildBatch()
{
    std::vector<mfd::UserCommand> commands;
    commands.reserve(28);

    window_.AppendCommands(commands);
    cockpit_.AppendCommands(commands);
    return commands;
}

std::vector<mfd::UserCommand> CockpitMockupUi::BuildShutdownBatch(std::string statusText)
{
    std::vector<mfd::UserCommand> commands;
    commands.reserve(8);
    cockpit_.AppendShutdownCommands(commands, std::move(statusText));
    return commands;
}

WindowDisplay& CockpitMockupUi::Window() noexcept
{
    return window_;
}

CockpitMockupPage& CockpitMockupUi::Cockpit() noexcept
{
    return cockpit_;
}
} // namespace mockup_ui
