/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation of the internal runtime-debug state model.
 */

#include "mfd/window/debug/RuntimeDebugState.hpp"

#include <functional>
#include <utility>

namespace mfd::window::debug
{
namespace
{
template <typename Value>
void HashCombine(std::size_t& seed, const Value& value) noexcept
{
    seed ^= std::hash<Value> {}(value) + 0x9e3779b9U + (seed << 6U) + (seed >> 2U);
}
} // namespace

std::size_t ReticleKeyHash::operator()(const ReticleKey& key) const noexcept
{
    std::size_t seed = 0U;
    HashCombine(seed, key.pageName);
    HashCombine(seed, key.reticleId);
    HashCombine(seed, static_cast<std::size_t>(key.kind));
    return seed;
}

std::size_t DynamicTemplateKeyHash::operator()(const DynamicTemplateKey& key) const noexcept
{
    std::size_t seed = 0U;
    HashCombine(seed, key.pageName);
    HashCombine(seed, key.templateId);
    return seed;
}

void RuntimeDebugState::Activate()
{
    active_ = true;
}

void RuntimeDebugState::Deactivate()
{
    active_ = false;
    ResetInteractiveState();
}

bool RuntimeDebugState::Active() const noexcept
{
    return active_;
}

void RuntimeDebugState::ResetInteractiveState()
{
    pageBypassed_ = false;
    forcedActivePage_.clear();
    selectedPage_.clear();
    selectedReticle_.reset();
    strobeBypasses_.clear();
    reticleBypasses_.clear();
    testPanelStatus_.clear();
}

void RuntimeDebugState::ResetObservedRuntimeState()
{
    dynamicTemplateVisibility_.clear();
    transport_ = {};
    lastCommandTrafficAt_.reset();
}

void RuntimeDebugState::SelectPage(std::string pageName)
{
    selectedPage_ = std::move(pageName);
}

const std::string& RuntimeDebugState::SelectedPage() const noexcept
{
    return selectedPage_;
}

void RuntimeDebugState::SelectReticle(ReticleKey key)
{
    selectedPage_ = key.pageName;
    selectedReticle_ = std::move(key);
}

void RuntimeDebugState::ClearReticleSelection()
{
    selectedReticle_.reset();
}

const std::optional<ReticleKey>& RuntimeDebugState::SelectedReticle() const noexcept
{
    return selectedReticle_;
}

void RuntimeDebugState::EnablePageBypass(std::string pageName)
{
    pageBypassed_ = true;
    forcedActivePage_ = std::move(pageName);
    selectedPage_ = forcedActivePage_;
}

void RuntimeDebugState::DisablePageBypass()
{
    pageBypassed_ = false;
    forcedActivePage_.clear();
}

bool RuntimeDebugState::PageBypassed() const noexcept
{
    return pageBypassed_;
}

const std::string& RuntimeDebugState::ForcedActivePage() const noexcept
{
    return forcedActivePage_;
}

bool RuntimeDebugState::ReticleBypassed(const ReticleKey& key) const noexcept
{
    return reticleBypasses_.find(key) != reticleBypasses_.end();
}

bool RuntimeDebugState::StrobeBypassed(const std::string_view pageName) const noexcept
{
    return strobeBypasses_.find(std::string(pageName)) != strobeBypasses_.end();
}

const std::string* RuntimeDebugState::ForcedActiveStrobe(const std::string_view pageName) const noexcept
{
    const auto iterator = strobeBypasses_.find(std::string(pageName));
    return iterator == strobeBypasses_.end() ? nullptr : &iterator->second;
}

void RuntimeDebugState::EnableStrobeBypass(std::string pageName, std::string strobeName)
{
    if (pageName.empty() || strobeName.empty())
    {
        return;
    }

    selectedPage_ = pageName;
    strobeBypasses_.insert_or_assign(std::move(pageName), std::move(strobeName));
}

void RuntimeDebugState::DisableStrobeBypass(const std::string_view pageName)
{
    strobeBypasses_.erase(std::string(pageName));
}

void RuntimeDebugState::ReleaseAllStrobeBypasses()
{
    strobeBypasses_.clear();
}

std::vector<StrobeBypassState> RuntimeDebugState::BypassedStrobes() const
{
    std::vector<StrobeBypassState> bypasses;
    bypasses.reserve(strobeBypasses_.size());

    for (const auto& [pageName, strobeName] : strobeBypasses_)
    {
        bypasses.push_back(StrobeBypassState {pageName, strobeName});
    }

    return bypasses;
}

const ReticleBypassState* RuntimeDebugState::FindReticleBypass(const ReticleKey& key) const noexcept
{
    const auto iterator = reticleBypasses_.find(key);
    return iterator == reticleBypasses_.end() ? nullptr : &iterator->second;
}

ReticleBypassState* RuntimeDebugState::FindReticleBypass(const ReticleKey& key) noexcept
{
    const auto iterator = reticleBypasses_.find(key);
    return iterator == reticleBypasses_.end() ? nullptr : &iterator->second;
}

ReticleBypassState& RuntimeDebugState::EnsureReticleBypass(ReticleKey key, const ReticleGroup& seed)
{
    auto [iterator, inserted] = reticleBypasses_.try_emplace(key);
    if (inserted)
    {
        iterator->second.key = std::move(key);
        iterator->second.draft = seed;
    }

    return iterator->second;
}

void RuntimeDebugState::ReleaseReticleBypass(const ReticleKey& key)
{
    reticleBypasses_.erase(key);
}

void RuntimeDebugState::ReleaseAllReticleBypasses()
{
    reticleBypasses_.clear();
}

std::vector<ReticleKey> RuntimeDebugState::BypassedReticles() const
{
    std::vector<ReticleKey> keys;
    keys.reserve(reticleBypasses_.size());

    for (const auto& entry : reticleBypasses_)
    {
        keys.push_back(entry.first);
    }

    return keys;
}

bool RuntimeDebugState::HasInteractiveOverrides() const noexcept
{
    return pageBypassed_ || !strobeBypasses_.empty() || !reticleBypasses_.empty();
}

void RuntimeDebugState::SetDynamicTemplateVisibility(std::string pageName, std::string templateId, const bool visible)
{
    dynamicTemplateVisibility_[DynamicTemplateKey {std::move(pageName), std::move(templateId)}] = visible;
}

void RuntimeDebugState::ClearDynamicTemplateVisibility()
{
    dynamicTemplateVisibility_.clear();
}

std::optional<bool> RuntimeDebugState::DynamicTemplateVisibility(const std::string_view pageName,
                                                                 const std::string_view templateId) const
{
    const auto iterator = dynamicTemplateVisibility_.find(DynamicTemplateKey {std::string(pageName), std::string(templateId)});
    if (iterator == dynamicTemplateVisibility_.end())
    {
        return std::nullopt;
    }

    return iterator->second;
}

std::vector<std::pair<DynamicTemplateKey, bool>> RuntimeDebugState::DynamicTemplateVisibilityEntries() const
{
    std::vector<std::pair<DynamicTemplateKey, bool>> entries;
    entries.reserve(dynamicTemplateVisibility_.size());

    for (const auto& entry : dynamicTemplateVisibility_)
    {
        entries.push_back(entry);
    }

    return entries;
}

void RuntimeDebugState::UpdateTransportState(const bool commandConfigured,
                                             const bool commandReady,
                                             const bool feedbackConfigured,
                                             const bool feedbackReady,
                                             const UdpRuntimeBridgeMetrics& metrics,
                                             std::string commandStatus,
                                             std::string feedbackStatus)
{
    transport_.commandConfigured = commandConfigured;
    transport_.commandReady = commandReady;
    transport_.feedbackConfigured = feedbackConfigured;
    transport_.feedbackReady = feedbackReady;
    transport_.metrics = metrics;
    transport_.commandStatus = std::move(commandStatus);
    transport_.feedbackStatus = std::move(feedbackStatus);
}

void RuntimeDebugState::NoteCommandTraffic(const std::size_t batchCount,
                                           const std::size_t commandCount,
                                           const Clock::time_point now)
{
    if (batchCount == 0U && commandCount == 0U)
    {
        return;
    }

    transport_.observedCommandTraffic = true;
    transport_.observedBatchCount += batchCount;
    transport_.observedCommandCount += commandCount;
    lastCommandTrafficAt_ = now;
}

const TransportState& RuntimeDebugState::Transport() const noexcept
{
    return transport_;
}

double RuntimeDebugState::SecondsSinceLastCommandTraffic(const Clock::time_point now) const noexcept
{
    if (!lastCommandTrafficAt_.has_value())
    {
        return -1.0;
    }

    return std::chrono::duration<double>(now - *lastCommandTrafficAt_).count();
}

void RuntimeDebugState::SetTestPanelStatus(std::string message)
{
    testPanelStatus_ = std::move(message);
}

const std::string& RuntimeDebugState::TestPanelStatus() const noexcept
{
    return testPanelStatus_;
}

bool operator==(const ReticleKey& lhs, const ReticleKey& rhs) noexcept
{
    return lhs.pageName == rhs.pageName && lhs.reticleId == rhs.reticleId && lhs.kind == rhs.kind;
}

bool operator==(const DynamicTemplateKey& lhs, const DynamicTemplateKey& rhs) noexcept
{
    return lhs.pageName == rhs.pageName && lhs.templateId == rhs.templateId;
}
} // namespace mfd::window::debug
