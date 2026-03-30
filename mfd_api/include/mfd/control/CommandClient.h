/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief High-level client API used to send user commands to a running MFD window.
 */

#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "mfd/MfdExport.h"
#include "mfd/control/CommandTransport.h"
#include "mfd/control/CommandTypes.h"
#include "mfd/ipc/ExchangeChannel.h"

namespace mfd
{
/**
 * @brief High-level client API used to send user commands to a running window.
 *
 * @note `CommandClient` uses the UDP command transport configured by the target
 * window JSON.
 */
class MFD_API CommandClient
{
public:
    CommandClient() = default;

    /**
     * @brief Creates a client from an already constructed exchange channel.
     * @param channel Concrete transport channel.
     */
    explicit CommandClient(std::unique_ptr<IExchangeChannel> channel);

    /**
     * @brief Creates a client from a window transport configuration.
     * @param config Window transport configuration.
     */
    CommandClient(const WindowCommandTransportConfig& config);

    /**
     * @brief Creates a client using a UDP transport.
     * @param config UDP transport configuration.
     */
    CommandClient(const WindowUdpCommandTransport& config);

    /**
     * @brief Indicates whether the underlying transport is ready.
     * @return `true` when the client can send commands.
     */
    bool IsReady() const noexcept;

    /**
     * @brief Returns the last transport or serialization error.
     * @return Error string, or an empty string when the client is healthy.
     */
    std::string LastError() const;

    /**
     * @brief Sends a typed user command.
     * @param command Command to send.
     * @return `true` if the command payload was sent successfully.
     */
    bool Send(const UserCommand& command);

    /**
     * @brief Sends several commands as one or more UDP batches.
     * @param commands Commands to send during the same external update cycle.
     * @param sequence Optional cycle identifier copied in every generated batch.
     * @return `true` if every batch payload was sent successfully.
     *
     * @note The client automatically splits the command list into several UDP
     * datagrams when the serialized payload would exceed the configured UDP
     * packet size.
     */
    bool SendBatch(std::span<const UserCommand> commands, std::uint32_t sequence = 0);

    /**
     * @brief Sends a pre-built command batch.
     * @param batch Batch to send.
     * @return `true` if every generated UDP payload was sent successfully.
     */
    bool SendBatch(const CommandBatch& batch);

    /**
     * @brief Returns the maximum serialized UDP payload targeted by this client.
     * @return Maximum payload size in bytes.
     */
    std::size_t MaxPayloadBytes() const noexcept;

    /** @brief Activates a page by name. */
    bool ActivatePage(std::string_view page);

    /** @brief Updates the view center and zoom of a page. */
    bool SetPageView(std::string_view page, Vec2 center, float zoom);

    /** @brief Sends a generic whole-window display patch. */
    bool UpdateWindowDisplay(const WindowDisplayPatch& patch);
    /** @brief Enables or disables whole-window color inversion. */
    bool SetWindowColorInverted(bool invertColors);
    /** @brief Sets the whole-window brightness factor in the `[0, 1]` range. */
    bool SetWindowBrightness(float brightness);
    /** @brief Enables or disables whole-window blackout. */
    bool SetWindowDisabled(bool disabled);

    /** @brief Sends a generic reticle patch. */
    bool UpdateReticle(std::string_view page, std::string_view reticle, const ReticlePatch& patch);
    /** @brief Sets reticle visibility. */
    bool SetReticleVisible(std::string_view page, std::string_view reticle, bool visible);
    /** @brief Enables or disables reticle blinking. */
    bool SetReticleBlinkEnabled(std::string_view page, std::string_view reticle, bool enabled);
    /** @brief Sets the page blink type used by a reticle and enables blinking. */
    bool SetReticleBlinkType(std::string_view page, std::string_view reticle, std::string_view blinkType);
    /** @brief Clears the reticle-specific blink type and falls back to the page default when blinking is enabled. */
    bool ClearReticleBlinkType(std::string_view page, std::string_view reticle);
    /** @brief Sets reticle position. */
    bool SetReticlePosition(std::string_view page, std::string_view reticle, Vec2 position);
    /** @brief Sets reticle rotation in degrees. */
    bool SetReticleRotation(std::string_view page, std::string_view reticle, float rotationDegrees);
    /** @brief Sets the reticle stroke color override. */
    bool SetReticleColor(std::string_view page, std::string_view reticle, ColorRgba color);
    /** @brief Sets the reticle stroke thickness override. */
    bool SetReticleThickness(std::string_view page, std::string_view reticle, float thickness);
    /** @brief Sets the first text primitive value found in the reticle. */
    bool SetReticleText(std::string_view page, std::string_view reticle, std::string text);
    /** @brief Sets the text of a named text primitive inside a reticle. */
    bool SetReticleText(std::string_view page,
                        std::string_view reticle,
                        std::string_view primitiveId,
                        std::string text);
    /** @brief Sets the first text primitive letter spacing found in the reticle. */
    bool SetReticleLetterSpacing(std::string_view page, std::string_view reticle, float letterSpacing);
    /** @brief Sets the letter spacing of a named text primitive inside a reticle. */
    bool SetReticleLetterSpacing(std::string_view page,
                                 std::string_view reticle,
                                 std::string_view primitiveId,
                                 float letterSpacing);

    /** @brief Enables or disables the strobe of a page. */
    bool SetStrobeActive(std::string_view page, bool active);
    /** @brief Moves the strobe of a page. */
    bool SetStrobePosition(std::string_view page, Vec2 position);

    /**
     * @brief Creates or updates a dynamic reticle from a template.
     * @param page Target page.
     * @param reticle Public id of the dynamic reticle.
     * @param templateId Reticle template used to instantiate the dynamic reticle.
     * @param patch Optional patch applied after instantiation.
     * @return `true` if the command was sent successfully.
     */
    bool UpsertDynamicReticle(std::string_view page,
                              std::string_view reticle,
                              std::string_view templateId,
                              const ReticlePatch& patch = {});

    /**
     * @brief Creates or updates many dynamic reticles sharing the same page and template.
     * @param page Target page.
     * @param templateId Reticle template used to instantiate missing reticles.
     * @param reticles Dynamic reticle updates to apply.
     * @return `true` if the command payload was sent successfully.
     *
     * @note The client automatically splits this bulk command into several UDP
     * Protocol Buffers payloads when the serialized packet would exceed the
     * configured `maxPacketSize`.
     */
    bool UpsertDynamicReticles(std::string_view page,
                               std::string_view templateId,
                               std::span<const DynamicReticleState> reticles);

    /**
     * @brief Removes a dynamic reticle from a page.
     * @param page Target page.
     * @param reticle Dynamic reticle id.
     * @return `true` if the command was sent successfully.
     */
    bool RemoveDynamicReticle(std::string_view page, std::string_view reticle);

private:
    bool SendPayload(std::string_view payload);
    bool SendBatchedPayloads(const CommandBatch& batch);

    std::unique_ptr<IExchangeChannel> channel_ {};
    std::size_t maxPayloadBytes_ = 4096;
    std::string lastError_ {};
};
} // namespace mfd
