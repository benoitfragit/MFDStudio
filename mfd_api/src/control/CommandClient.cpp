/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation for CommandClient.
 */

#include "mfd/control/CommandClient.h"

#include <algorithm>
#include <cstddef>
#include <exception>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "mfd/ipc/ExchangeChannel.h"

namespace mfd
{
namespace
{
constexpr std::size_t kUdpHardPayloadLimit = 65507;
constexpr std::size_t kDefaultCommandPayloadLimit = 4096;

CommandBatch MakeBatch(const std::uint32_t sequence, std::vector<UserCommand> commands)
{
    CommandBatch batch;
    batch.sequence = sequence;
    batch.commands = std::move(commands);
    return batch;
}

std::optional<std::string> TrySerializeBatch(const CommandBatch& batch, std::string& error)
{
    try
    {
        return SerializeCommandBatch(batch);
    }
    catch (const std::exception& exception)
    {
        error = exception.what();
        return std::nullopt;
    }
    catch (...)
    {
        error = "Unknown exception while serializing a Protocol Buffers command batch";
        return std::nullopt;
    }
}

std::optional<std::vector<UserCommand>> SplitOversizedCommand(const UserCommand& command,
                                                              const std::uint32_t sequence,
                                                              const std::size_t maxPayloadBytes,
                                                              std::string& error)
{
    const auto fitsAsSingleCommand =
        [&command, sequence, maxPayloadBytes, &error]() -> bool
    {
        auto payload = TrySerializeBatch(MakeBatch(sequence, std::vector<UserCommand> {command}), error);
        return payload.has_value() && payload->size() <= maxPayloadBytes;
    };

    if (fitsAsSingleCommand())
    {
        return std::vector<UserCommand> {command};
    }

    return std::visit(
        [sequence, maxPayloadBytes, &error](const auto& value) -> std::optional<std::vector<UserCommand>>
        {
            using Command = std::decay_t<decltype(value)>;

            if constexpr (std::is_same_v<Command, UpsertDynamicReticlesCommand>)
            {
                if (value.reticles.empty())
                {
                    error = "A single command exceeds the configured UDP payload limit of " +
                            std::to_string(maxPayloadBytes) + " bytes";
                    return std::nullopt;
                }

                std::vector<UserCommand> splitCommands;
                splitCommands.reserve(value.reticles.size());

                std::size_t firstIndex = 0U;
                while (firstIndex < value.reticles.size())
                {
                    std::size_t bestCount = 0U;
                    std::size_t left = 1U;
                    std::size_t right = value.reticles.size() - firstIndex;

                    while (left <= right)
                    {
                        const std::size_t middle = left + (right - left) / 2U;

                        UpsertDynamicReticlesCommand candidate;
                        candidate.page = value.page;
                        candidate.templateId = value.templateId;
                        candidate.reticles.assign(value.reticles.begin() + static_cast<std::ptrdiff_t>(firstIndex),
                                                  value.reticles.begin() + static_cast<std::ptrdiff_t>(firstIndex + middle));

                        auto payload = TrySerializeBatch(
                            MakeBatch(sequence, std::vector<UserCommand> {UserCommand {std::move(candidate)}}),
                            error);

                        if (payload.has_value() && payload->size() <= maxPayloadBytes)
                        {
                            bestCount = middle;
                            left = middle + 1U;
                        }
                        else
                        {
                            if (!payload.has_value())
                            {
                                return std::nullopt;
                            }

                            if (middle == 0U)
                            {
                                break;
                            }

                            right = middle - 1U;
                        }
                    }

                    if (bestCount == 0U)
                    {
                        error = "One dynamic reticle update exceeds the configured UDP payload limit of " +
                                std::to_string(maxPayloadBytes) + " bytes";
                        return std::nullopt;
                    }

                    UpsertDynamicReticlesCommand chunk;
                    chunk.page = value.page;
                    chunk.templateId = value.templateId;
                    chunk.reticles.assign(value.reticles.begin() + static_cast<std::ptrdiff_t>(firstIndex),
                                          value.reticles.begin() + static_cast<std::ptrdiff_t>(firstIndex + bestCount));
                    splitCommands.emplace_back(std::move(chunk));
                    firstIndex += bestCount;
                }

                return splitCommands;
            }
            else
            {
                error = "A single command exceeds the configured UDP payload limit of " +
                        std::to_string(maxPayloadBytes) + " bytes";
                return std::nullopt;
            }
        },
        command);
}
} // namespace

CommandClient::CommandClient(std::unique_ptr<IExchangeChannel> channel)
    : channel_(std::move(channel))
{
}

CommandClient::CommandClient(const WindowCommandTransportConfig& config)
    : CommandClient(CreateCommandClientChannel(config))
{
    if (config.udp.has_value())
    {
        maxPayloadBytes_ = std::clamp(config.udp->maxPacketSize,
                                      std::size_t {1},
                                      kUdpHardPayloadLimit);
    }
}

CommandClient::CommandClient(const WindowUdpCommandTransport& config)
    : CommandClient(CreateCommandClientChannel(config))
{
    maxPayloadBytes_ = std::clamp(config.maxPacketSize,
                                  std::size_t {1},
                                  kUdpHardPayloadLimit);
}

bool CommandClient::IsReady() const noexcept
{
    return channel_ != nullptr && channel_->IsReady();
}

std::string CommandClient::LastError() const
{
    if (!lastError_.empty())
    {
        return lastError_;
    }

    return channel_ == nullptr ? std::string {} : channel_->LastError();
}

bool CommandClient::Send(const UserCommand& command)
{
    CommandBatch batch;
    batch.commands.push_back(command);
    return SendBatchedPayloads(batch);
}

bool CommandClient::SendBatch(const std::span<const UserCommand> commands, const std::uint32_t sequence)
{
    CommandBatch batch;
    batch.sequence = sequence;
    batch.commands.assign(commands.begin(), commands.end());
    return SendBatchedPayloads(batch);
}

bool CommandClient::SendBatch(const CommandBatch& batch)
{
    return SendBatchedPayloads(batch);
}

std::size_t CommandClient::MaxPayloadBytes() const noexcept
{
    return maxPayloadBytes_ == 0 ? kDefaultCommandPayloadLimit : maxPayloadBytes_;
}

bool CommandClient::ActivatePage(const std::string_view page)
{
    return Send(ActivatePageCommand {std::string(page)});
}

bool CommandClient::SetPageView(const std::string_view page, const Vec2 center, const float zoom)
{
    return Send(SetPageViewCommand {std::string(page), PageViewState {center, zoom}});
}

bool CommandClient::UpdateWindowDisplay(const WindowDisplayPatch& patch)
{
    return Send(UpdateWindowDisplayCommand {patch});
}

bool CommandClient::SetWindowColorInverted(const bool invertColors)
{
    WindowDisplayPatch patch;
    patch.invertColors = invertColors;
    return UpdateWindowDisplay(patch);
}

bool CommandClient::SetWindowBrightness(const float brightness)
{
    WindowDisplayPatch patch;
    patch.brightness = brightness;
    return UpdateWindowDisplay(patch);
}

bool CommandClient::SetWindowDisabled(const bool disabled)
{
    WindowDisplayPatch patch;
    patch.disabled = disabled;
    return UpdateWindowDisplay(patch);
}

bool CommandClient::UpdateReticle(const std::string_view page,
                                  const std::string_view reticle,
                                  const ReticlePatch& patch)
{
    return Send(UpdateReticleCommand {ReticleHandle {std::string(page), std::string(reticle)}, patch});
}

bool CommandClient::SetReticleVisible(const std::string_view page,
                                      const std::string_view reticle,
                                      const bool visible)
{
    ReticlePatch patch;
    patch.visible = visible;
    return UpdateReticle(page, reticle, patch);
}

bool CommandClient::SetReticleBlinkEnabled(const std::string_view page,
                                           const std::string_view reticle,
                                           const bool enabled)
{
    ReticlePatch patch;
    patch.blinkEnabled = enabled;
    return UpdateReticle(page, reticle, patch);
}

bool CommandClient::SetReticleBlinkType(const std::string_view page,
                                        const std::string_view reticle,
                                        const std::string_view blinkType)
{
    ReticlePatch patch;
    patch.blinkType = std::string(blinkType);
    return UpdateReticle(page, reticle, patch);
}

bool CommandClient::ClearReticleBlinkType(const std::string_view page, const std::string_view reticle)
{
    ReticlePatch patch;
    patch.blinkType = std::string {};
    return UpdateReticle(page, reticle, patch);
}

bool CommandClient::SetReticlePosition(const std::string_view page,
                                       const std::string_view reticle,
                                       const Vec2 position)
{
    ReticlePatch patch;
    patch.position = position;
    return UpdateReticle(page, reticle, patch);
}

bool CommandClient::SetReticleRotation(const std::string_view page,
                                       const std::string_view reticle,
                                       const float rotationDegrees)
{
    ReticlePatch patch;
    patch.rotationDegrees = rotationDegrees;
    return UpdateReticle(page, reticle, patch);
}

bool CommandClient::SetReticleColor(const std::string_view page,
                                    const std::string_view reticle,
                                    const ColorRgba color)
{
    ReticlePatch patch;
    patch.color = color;
    return UpdateReticle(page, reticle, patch);
}

bool CommandClient::SetReticleThickness(const std::string_view page,
                                        const std::string_view reticle,
                                        const float thickness)
{
    ReticlePatch patch;
    patch.thickness = thickness;
    return UpdateReticle(page, reticle, patch);
}

bool CommandClient::SetReticleText(const std::string_view page,
                                   const std::string_view reticle,
                                   std::string text)
{
    ReticlePatch patch;
    patch.text = std::move(text);
    return UpdateReticle(page, reticle, patch);
}

bool CommandClient::SetReticleText(const std::string_view page,
                                   const std::string_view reticle,
                                   const std::string_view primitiveId,
                                   std::string text)
{
    ReticlePatch patch;
    patch.texts.emplace(std::string(primitiveId), std::move(text));
    return UpdateReticle(page, reticle, patch);
}

bool CommandClient::SetReticleLetterSpacing(const std::string_view page,
                                            const std::string_view reticle,
                                            const float letterSpacing)
{
    ReticlePatch patch;
    patch.letterSpacing = letterSpacing;
    return UpdateReticle(page, reticle, patch);
}

bool CommandClient::SetReticleLetterSpacing(const std::string_view page,
                                            const std::string_view reticle,
                                            const std::string_view primitiveId,
                                            const float letterSpacing)
{
    ReticlePatch patch;
    patch.letterSpacings.emplace(std::string(primitiveId), letterSpacing);
    return UpdateReticle(page, reticle, patch);
}

bool CommandClient::SetStrobeActive(const std::string_view page, const bool active)
{
    return Send(UpdateStrobeCommand {std::string(page), active, std::nullopt});
}

bool CommandClient::SetStrobePosition(const std::string_view page, const Vec2 position)
{
    return Send(UpdateStrobeCommand {std::string(page), std::nullopt, position});
}

bool CommandClient::UpsertDynamicReticle(const std::string_view page,
                                         const std::string_view reticle,
                                         const std::string_view templateId,
                                         const ReticlePatch& patch)
{
    return Send(UpsertDynamicReticleCommand {
        ReticleHandle {std::string(page), std::string(reticle)},
        std::string(templateId),
        patch});
}

bool CommandClient::UpsertDynamicReticles(const std::string_view page,
                                          const std::string_view templateId,
                                          const std::span<const DynamicReticleState> reticles)
{
    UpsertDynamicReticlesCommand command;
    command.page = std::string(page);
    command.templateId = std::string(templateId);
    command.reticles.assign(reticles.begin(), reticles.end());
    return Send(command);
}

bool CommandClient::SetDynamicReticleSetVisible(const std::string_view page,
                                                const std::string_view templateId,
                                                const bool visible)
{
    return Send(SetDynamicReticleSetVisibilityCommand {
        std::string(page),
        std::string(templateId),
        visible});
}

bool CommandClient::RemoveDynamicReticle(const std::string_view page, const std::string_view reticle)
{
    return Send(RemoveDynamicReticleCommand {ReticleHandle {std::string(page), std::string(reticle)}});
}

bool CommandClient::SendPayload(const std::string_view payload)
{
    if (!IsReady())
    {
        lastError_ = channel_ == nullptr ? "No command transport is configured" : channel_->LastError();
        return false;
    }

    try
    {
        const auto* payloadBytes = reinterpret_cast<const std::byte*>(payload.data());
        const std::span<const std::byte> payloadView(payloadBytes, payload.size());

        if (!channel_->Send(payloadView))
        {
            lastError_ = channel_->LastError();
            return false;
        }
    }
    catch (const std::exception& exception)
    {
        lastError_ = exception.what();
        return false;
    }
    catch (...)
    {
        lastError_ = "Unknown exception while sending a command";
        return false;
    }

    lastError_.clear();
    return true;
}

bool CommandClient::SendBatchedPayloads(const CommandBatch& batch)
{
    if (batch.commands.empty())
    {
        lastError_.clear();
        return true;
    }

    const std::size_t maxPayloadBytes = MaxPayloadBytes();
    std::string error;
    std::vector<UserCommand> expandedCommands;
    expandedCommands.reserve(batch.commands.size());

    for (const UserCommand& command : batch.commands)
    {
        const auto splitCommands = SplitOversizedCommand(command, batch.sequence, maxPayloadBytes, error);
        if (!splitCommands.has_value())
        {
            lastError_ = std::move(error);
            return false;
        }

        expandedCommands.insert(expandedCommands.end(), splitCommands->begin(), splitCommands->end());
    }

    CommandBatch currentChunk;
    currentChunk.sequence = batch.sequence;

    auto flushCurrentChunk = [this, &currentChunk, &error]() -> bool
    {
        if (currentChunk.commands.empty())
        {
            return true;
        }

        const auto payload = TrySerializeBatch(currentChunk, error);
        if (!payload.has_value())
        {
            lastError_ = std::move(error);
            return false;
        }

        if (!SendPayload(*payload))
        {
            return false;
        }

        currentChunk.commands.clear();
        return true;
    };

    for (const UserCommand& command : expandedCommands)
    {
        CommandBatch candidate = currentChunk;
        candidate.commands.push_back(command);

        const auto payload = TrySerializeBatch(candidate, error);
        if (!payload.has_value())
        {
            lastError_ = std::move(error);
            return false;
        }

        if (payload->size() <= maxPayloadBytes)
        {
            currentChunk = std::move(candidate);
            continue;
        }

        if (!flushCurrentChunk())
        {
            return false;
        }

        CommandBatch singleCommandChunk;
        singleCommandChunk.sequence = batch.sequence;
        singleCommandChunk.commands.push_back(command);

        const auto singlePayload = TrySerializeBatch(singleCommandChunk, error);
        if (!singlePayload.has_value())
        {
            lastError_ = std::move(error);
            return false;
        }

        if (singlePayload->size() > maxPayloadBytes)
        {
            lastError_ = "A single command exceeds the configured UDP payload limit of " +
                         std::to_string(maxPayloadBytes) + " bytes";
            return false;
        }

        currentChunk = std::move(singleCommandChunk);
    }

    return flushCurrentChunk();
}
} // namespace mfd
