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
#include <optional>
#include "mfd/core/ArrayView.h"
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
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
    /**
     * @brief Marker type used by generated page wrappers accepted by page helpers.
     */
    struct GeneratedPageTag
    {
    };

    CommandClient() = default;

    /**
     * @brief Creates a client from an already constructed exchange channel.
     * @param channel Concrete transport channel.
     * @param transportMap Optional generated transport map used to resolve
     * named authored identifiers before serialization.
     */
    explicit CommandClient(std::unique_ptr<IExchangeChannel> channel,
                           std::optional<GeneratedTransportMap> transportMap = std::nullopt);

    /**
     * @brief Creates a client from a window transport configuration.
     * @param config Window transport configuration.
     * @param transportMap Optional generated transport map used to resolve
     * named authored identifiers before serialization.
     */
    explicit CommandClient(const WindowCommandTransportConfig& config,
                           std::optional<GeneratedTransportMap> transportMap = std::nullopt);

    /**
     * @brief Creates a client using a UDP transport.
     * @param config UDP transport configuration.
     * @param transportMap Optional generated transport map used to resolve
     * named authored identifiers before serialization.
     */
    explicit CommandClient(const WindowUdpCommandTransport& config,
                           std::optional<GeneratedTransportMap> transportMap = std::nullopt);

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
    bool SendBatch(ArrayView<const UserCommand> commands, std::uint32_t sequence = 0);

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

    /**
     * @brief Activates a page by authored name.
     * @param page Authored page name resolved through the configured generated transport map.
     * @return `true` if the command payload was sent successfully.
     *
     * @note Requires this client to be constructed with a `GeneratedTransportMap`.
     * Prefer the generated page-wrapper overload in production code.
     */
    bool ActivatePage(std::string_view page);
    /**
     * @brief Activates a generated page wrapper by transport id and mapping hash.
     * @tparam GeneratedPage Generated page class exposing `GeneratedId()` and `MappingHash()`.
     * @param page Generated page wrapper returned by a generated UI root.
     * @return `true` if the command payload was sent successfully.
     */
    template <typename GeneratedPage,
              typename = std::enable_if_t<
                  std::is_same_v<typename GeneratedPage::MfdGeneratedPageTag, GeneratedPageTag>>>
    bool ActivatePage([[maybe_unused]] const GeneratedPage& page)
    {
        return ActivateGeneratedPage(GeneratedPage::GeneratedId(), GeneratedPage::MappingHash());
    }

    /**
     * @brief Updates the view center and zoom of a page by authored name.
     * @param page Authored page name resolved through the configured generated transport map.
     * @param center Logical point placed at the center of the viewport.
     * @param zoom Page zoom factor.
     * @return `true` if the command payload was sent successfully.
     *
     * @note Requires this client to be constructed with a `GeneratedTransportMap`.
     * Prefer the generated page-wrapper overload in production code.
     */
    bool SetPageView(std::string_view page, Vec2 center, float zoom);
    /**
     * @brief Updates the view center and zoom of a generated page wrapper by transport id and mapping hash.
     * @tparam GeneratedPage Generated page class exposing `GeneratedId()` and `MappingHash()`.
     * @param page Generated page wrapper returned by a generated UI root.
     * @param center Logical point placed at the center of the viewport.
     * @param zoom Page zoom factor.
     * @return `true` if the command payload was sent successfully.
     */
    template <typename GeneratedPage,
              typename = std::enable_if_t<
                  std::is_same_v<typename GeneratedPage::MfdGeneratedPageTag, GeneratedPageTag>>>
    bool SetPageView([[maybe_unused]] const GeneratedPage& page, Vec2 center, float zoom)
    {
        return SetGeneratedPageView(GeneratedPage::GeneratedId(), GeneratedPage::MappingHash(), center, zoom);
    }

    /** @brief Sends a generic whole-window display patch. */
    bool UpdateWindowDisplay(const WindowDisplayPatch& patch);
    /** @brief Enables or disables whole-window color inversion. */
    bool SetWindowColorInverted(bool invertColors);
    /** @brief Sets the whole-window brightness factor in the `[0, 1]` range. */
    bool SetWindowBrightness(float brightness);
    /** @brief Enables or disables whole-window blackout. */
    bool SetWindowDisabled(bool disabled);

    /** @brief Sends a generic reticle patch by authored page and reticle names.
     *  @note Requires this client to be constructed with a `GeneratedTransportMap`.
     */
    bool UpdateReticle(std::string_view page, std::string_view reticle, const ReticlePatch& patch);
    /** @brief Sets reticle visibility by authored page and reticle names.
     *  @note Requires this client to be constructed with a `GeneratedTransportMap`.
     */
    bool SetReticleVisible(std::string_view page, std::string_view reticle, bool visible);
    /** @brief Enables or disables reticle blinking by authored page and reticle names.
     *  @note Requires this client to be constructed with a `GeneratedTransportMap`.
     */
    bool SetReticleBlinkEnabled(std::string_view page, std::string_view reticle, bool enabled);
    /** @brief Sets the page blink type used by a reticle and enables blinking by authored names.
     *  @note Requires this client to be constructed with a `GeneratedTransportMap`.
     */
    bool SetReticleBlinkType(std::string_view page, std::string_view reticle, std::string_view blinkType);
    /** @brief Clears the reticle-specific blink type by authored names and falls back to the page default when blinking is enabled.
     *  @note Requires this client to be constructed with a `GeneratedTransportMap`.
     */
    bool ClearReticleBlinkType(std::string_view page, std::string_view reticle);
    /** @brief Sets reticle position by authored page and reticle names.
     *  @note Requires this client to be constructed with a `GeneratedTransportMap`.
     */
    bool SetReticlePosition(std::string_view page, std::string_view reticle, Vec2 position);
    /** @brief Sets reticle rotation in degrees by authored page and reticle names.
     *  @note Requires this client to be constructed with a `GeneratedTransportMap`.
     */
    bool SetReticleRotation(std::string_view page, std::string_view reticle, float rotationDegrees);
    /** @brief Sets the reticle stroke color override by authored page and reticle names.
     *  @note Requires this client to be constructed with a `GeneratedTransportMap`.
     */
    bool SetReticleColor(std::string_view page, std::string_view reticle, ColorRgba color);
    /** @brief Sets the reticle stroke thickness override by authored page and reticle names.
     *  @note Requires this client to be constructed with a `GeneratedTransportMap`.
     */
    bool SetReticleThickness(std::string_view page, std::string_view reticle, float thickness);
    /** @brief Sets the first text primitive value found in the reticle by authored page and reticle names.
     *  @note Requires this client to be constructed with a `GeneratedTransportMap`.
     */
    bool SetReticleText(std::string_view page, std::string_view reticle, std::string text);
    /** @brief Sets the text of a named text primitive inside a reticle by authored names.
     *  @note Requires this client to be constructed with a `GeneratedTransportMap`.
     */
    bool SetReticleText(std::string_view page,
                        std::string_view reticle,
                        std::string_view primitiveId,
                        std::string text);
    /** @brief Sets the first text primitive letter spacing found in the reticle by authored names.
     *  @note Requires this client to be constructed with a `GeneratedTransportMap`.
     */
    bool SetReticleLetterSpacing(std::string_view page, std::string_view reticle, float letterSpacing);
    /** @brief Sets the letter spacing of a named text primitive inside a reticle by authored names.
     *  @note Requires this client to be constructed with a `GeneratedTransportMap`.
     */
    bool SetReticleLetterSpacing(std::string_view page,
                                 std::string_view reticle,
                                 std::string_view primitiveId,
                                 float letterSpacing);

    /** @brief Enables or disables the strobe of a page by authored name.
     *  @note Requires this client to be constructed with a `GeneratedTransportMap`.
     */
    bool SetStrobeActive(std::string_view page, bool active);
    /** @brief Selects the active strobe of a page by authored strobe name.
     *  @note Requires this client to be constructed with a `GeneratedTransportMap`.
     */
    bool SelectStrobe(std::string_view page, std::string_view strobeName);
    /** @brief Moves the strobe of a page by authored name.
     *  @note Requires this client to be constructed with a `GeneratedTransportMap`.
     */
    bool SetStrobePosition(std::string_view page, Vec2 position);

    /**
     * @brief Creates or updates a dynamic reticle from a template.
     * @param page Target page.
     * @param reticle Public id of the dynamic reticle.
     * @param templateId Reticle template used to instantiate the dynamic reticle.
     * @param patch Optional patch applied after instantiation.
     * @return `true` if the command was sent successfully.
     *
     * @note Requires this client to be constructed with a `GeneratedTransportMap`.
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
     * configured `maxPacketSize`, while preserving `CommandBatch::mappingHash`
     * and any generated transport ids already present in the command.
     * Requires this client to be constructed with a `GeneratedTransportMap`.
     */
    bool UpsertDynamicReticles(std::string_view page,
                               std::string_view templateId,
                               ArrayView<const DynamicReticleState> reticles);
    /** @brief Enables or disables all dynamic reticles belonging to one page/template set by authored names.
     *  @note Requires this client to be constructed with a `GeneratedTransportMap`.
     */
    bool SetDynamicReticleSetVisible(std::string_view page, std::string_view templateId, bool visible);
    /** @brief Enables or disables strobe-magnet eligibility for all dynamic reticles of one page/template set.
     *  @note Dynamic sets are non-magnetized by default until this helper enables them.
     *  Requires this client to be constructed with a `GeneratedTransportMap`.
     */
    bool SetDynamicReticleSetStrobeMagnetEnabled(std::string_view page,
                                                 std::string_view templateId,
                                                 bool enabled);

    /**
     * @brief Removes a dynamic reticle from a page.
     * @param page Target page.
     * @param reticle Dynamic reticle id.
     * @return `true` if the command was sent successfully.
     *
     * @note Requires this client to be constructed with a `GeneratedTransportMap`.
     */
    bool RemoveDynamicReticle(std::string_view page, std::string_view reticle);
    /** @brief Requests a full reset of the target window runtime state. */
    bool ResetWindow();

private:
    /**
     * @brief Sends a generated page activation command carrying the generated mapping hash.
     * @param pageId Generated transport id of the target page.
     * @param mappingHash Mapping hash compiled into the generated page wrapper.
     * @return `true` if the command payload was sent successfully.
     */
    bool ActivateGeneratedPage(TransportId pageId, std::string_view mappingHash);
    /**
     * @brief Sends a generated page view command carrying the generated mapping hash.
     * @param pageId Generated transport id of the target page.
     * @param mappingHash Mapping hash compiled into the generated page wrapper.
     * @param center Logical point placed at the center of the viewport.
     * @param zoom Page zoom factor.
     * @return `true` if the command payload was sent successfully.
     */
    bool SetGeneratedPageView(TransportId pageId, std::string_view mappingHash, Vec2 center, float zoom);
    bool NormalizeBatchForTransport(const CommandBatch& sourceBatch, CommandBatch& normalizedBatch);
    void InitializeFragmentIdentity();
    bool SendPayload(std::string_view payload);
    bool SendBatchedPayloads(const CommandBatch& batch);

    std::unique_ptr<IExchangeChannel> channel_ {};
    std::optional<GeneratedTransportMap> transportMap_ {};
    std::size_t maxPayloadBytes_ = 4096;
    std::uint64_t clientId_ = 0U;
    std::uint64_t sessionEpoch_ = 0U;
    std::uint64_t nextBatchId_ = 1U;
    std::string lastError_ {};
};
} // namespace mfd
