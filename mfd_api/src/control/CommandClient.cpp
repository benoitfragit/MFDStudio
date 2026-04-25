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
#include <cstdint>
#include <cstddef>
#include <exception>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "mfd/ipc/ExchangeChannel.h"
#include "mfd/model/PageName.h"

namespace mfd
{
namespace
{
constexpr std::size_t kUdpHardPayloadLimit = 65507;
constexpr std::size_t kDefaultCommandPayloadLimit = 4096;
constexpr RuntimeDynamicId kGeneratedDynamicRuntimeIdBit = RuntimeDynamicId {1} << 63U;

bool PatchUsesGeneratedIdentifiers(const ReticlePatch& patch) noexcept
{
    return patch.blinkTypeId.has_value() ||
           !patch.textsById.empty() ||
           !patch.letterSpacingsById.empty() ||
           !patch.primitivePatchesById.empty();
}

bool CommandUsesGeneratedIdentifiers(const UserCommand& command) noexcept
{
    return std::visit(
        [](const auto& value) noexcept -> bool
        {
            using Command = std::decay_t<decltype(value)>;

            if constexpr (std::is_same_v<Command, ActivatePageCommand> ||
                          std::is_same_v<Command, SetPageViewCommand> ||
                          std::is_same_v<Command, UpdateStrobeCommand>)
            {
                return value.pageId != 0;
            }
            else if constexpr (std::is_same_v<Command, UpdateReticleCommand>)
            {
                return value.target.pageId != 0 || value.target.reticleId != 0 ||
                       PatchUsesGeneratedIdentifiers(value.patch);
            }
            else if constexpr (std::is_same_v<Command, UpsertDynamicReticleCommand>)
            {
                return value.target.pageId != 0 || value.templateTransportId != 0 ||
                       PatchUsesGeneratedIdentifiers(value.patch);
            }
            else if constexpr (std::is_same_v<Command, UpsertDynamicReticlesCommand>)
            {
                if (value.pageId != 0 || value.templateTransportId != 0)
                {
                    return true;
                }

                for (const DynamicReticleState& state : value.reticles)
                {
                    if (PatchUsesGeneratedIdentifiers(state.patch))
                    {
                        return true;
                    }
                }

                return false;
            }
            else if constexpr (std::is_same_v<Command, SetDynamicReticleSetVisibilityCommand>)
            {
                return value.pageId != 0 || value.templateTransportId != 0;
            }
            else if constexpr (std::is_same_v<Command, RemoveDynamicReticleCommand>)
            {
                return value.target.pageId != 0;
            }
            else
            {
                return false;
            }
        },
        command);
}

std::uint64_t HashNormalizedIdentifier(std::string_view lhs, std::string_view rhs) noexcept
{
    constexpr std::uint64_t kFnvOffset = 1469598103934665603ull;
    constexpr std::uint64_t kFnvPrime = 1099511628211ull;

    auto append = [](std::uint64_t hash, const std::string_view value) noexcept
    {
        for (const unsigned char ch : value)
        {
            hash ^= static_cast<std::uint64_t>(ch);
            hash *= kFnvPrime;
        }

        return hash;
    };

    std::uint64_t hash = append(kFnvOffset, lhs);
    hash = append(hash, std::string_view {"\x1F", 1});
    hash = append(hash, rhs);
    return hash;
}

RuntimeDynamicId MakeStableNamedRuntimeDynamicId(const std::string_view pageName,
                                                 const std::string_view reticleId) noexcept
{
    RuntimeDynamicId runtimeId =
        HashNormalizedIdentifier(NormalizePageName(pageName), NormalizePageName(reticleId)) &
        ~kGeneratedDynamicRuntimeIdBit;
    if (runtimeId == 0)
    {
        runtimeId = 1;
    }

    return runtimeId;
}

const TransportMapPageEntry* FindPageById(const GeneratedTransportMap& map, const TransportId pageId) noexcept
{
    const auto iterator = std::find_if(
        map.pages.begin(),
        map.pages.end(),
        [pageId](const TransportMapPageEntry& page)
        {
            return page.id == pageId;
        });
    return iterator == map.pages.end() ? nullptr : &(*iterator);
}

const TransportMapPageEntry* FindPageByName(const GeneratedTransportMap& map, const std::string_view pageName) noexcept
{
    const std::string normalizedPage = NormalizePageName(pageName);
    const auto iterator = std::find_if(
        map.pages.begin(),
        map.pages.end(),
        [&normalizedPage](const TransportMapPageEntry& page)
        {
            return page.normalizedName == normalizedPage;
        });
    return iterator == map.pages.end() ? nullptr : &(*iterator);
}

const TransportMapReticleEntry* FindStaticReticle(const GeneratedTransportMap& map,
                                                  const TransportId pageId,
                                                  const std::string_view reticleId) noexcept
{
    const std::string normalizedReticle = NormalizePageName(reticleId);
    const auto iterator = std::find_if(
        map.reticles.begin(),
        map.reticles.end(),
        [pageId, &normalizedReticle](const TransportMapReticleEntry& reticle)
        {
            return reticle.pageId == pageId && reticle.normalizedReticleId == normalizedReticle;
        });
    return iterator == map.reticles.end() ? nullptr : &(*iterator);
}

const TransportMapTemplateEntry* FindTemplateById(const GeneratedTransportMap& map, const TransportId templateId) noexcept
{
    const auto iterator = std::find_if(
        map.templates.begin(),
        map.templates.end(),
        [templateId](const TransportMapTemplateEntry& templ)
        {
            return templ.id == templateId;
        });
    return iterator == map.templates.end() ? nullptr : &(*iterator);
}

const TransportMapTemplateEntry* FindTemplateByName(const GeneratedTransportMap& map,
                                                    const std::string_view templateId) noexcept
{
    const std::string normalizedTemplate = NormalizePageName(templateId);
    const auto iterator = std::find_if(
        map.templates.begin(),
        map.templates.end(),
        [&normalizedTemplate](const TransportMapTemplateEntry& templ)
        {
            return templ.normalizedTemplateId == normalizedTemplate;
        });
    return iterator == map.templates.end() ? nullptr : &(*iterator);
}

const TransportMapBlinkTypeEntry* FindBlinkType(const GeneratedTransportMap& map,
                                                const TransportId pageId,
                                                const std::string_view blinkType) noexcept
{
    const std::string normalizedBlinkType = NormalizePageName(blinkType);
    const auto iterator = std::find_if(
        map.blinkTypes.begin(),
        map.blinkTypes.end(),
        [pageId, &normalizedBlinkType](const TransportMapBlinkTypeEntry& entry)
        {
            return entry.pageId == pageId && entry.normalizedBlinkType == normalizedBlinkType;
        });
    return iterator == map.blinkTypes.end() ? nullptr : &(*iterator);
}

const TransportMapPrimitiveEntry* FindPrimitiveByOwner(const GeneratedTransportMap& map,
                                                       const TransportPrimitiveOwnerKind ownerKind,
                                                       const TransportId ownerId,
                                                       const std::string_view primitiveId) noexcept
{
    const std::string normalizedPrimitive = NormalizePageName(primitiveId);
    const auto iterator = std::find_if(
        map.primitives.begin(),
        map.primitives.end(),
        [ownerKind, ownerId, &normalizedPrimitive](const TransportMapPrimitiveEntry& entry)
        {
            return entry.ownerKind == ownerKind &&
                   entry.ownerId == ownerId &&
                   entry.normalizedPrimitiveId == normalizedPrimitive;
        });
    return iterator == map.primitives.end() ? nullptr : &(*iterator);
}

CommandBatch MakeBatch(const std::uint32_t sequence,
                       std::string mappingHash,
                       std::vector<UserCommand> commands)
{
    CommandBatch batch;
    batch.sequence = sequence;
    batch.mappingHash = std::move(mappingHash);
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
                                                              const std::string_view mappingHash,
                                                              const std::size_t maxPayloadBytes,
                                                              std::string& error)
{
    const auto fitsAsSingleCommand =
        [&command, sequence, mappingHash, maxPayloadBytes, &error]() -> bool
    {
        auto payload = TrySerializeBatch(MakeBatch(sequence, std::string(mappingHash), std::vector<UserCommand> {command}), error);
        return payload.has_value() && payload->size() <= maxPayloadBytes;
    };

    if (fitsAsSingleCommand())
    {
        return std::vector<UserCommand> {command};
    }

    return std::visit(
        [sequence, mappingHash, maxPayloadBytes, &error](const auto& value) -> std::optional<std::vector<UserCommand>>
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
                        candidate.pageId = value.pageId;
                        candidate.templateId = value.templateId;
                        candidate.templateTransportId = value.templateTransportId;
                        candidate.reticles.assign(value.reticles.begin() + static_cast<std::ptrdiff_t>(firstIndex),
                                                  value.reticles.begin() + static_cast<std::ptrdiff_t>(firstIndex + middle));

                        auto payload = TrySerializeBatch(
                            MakeBatch(sequence,
                                      std::string(mappingHash),
                                      std::vector<UserCommand> {UserCommand {std::move(candidate)}}),
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
                    chunk.pageId = value.pageId;
                    chunk.templateId = value.templateId;
                    chunk.templateTransportId = value.templateTransportId;
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

CommandClient::CommandClient(std::unique_ptr<IExchangeChannel> channel, std::optional<GeneratedTransportMap> transportMap)
    : channel_(std::move(channel)),
      transportMap_(std::move(transportMap))
{
}

CommandClient::CommandClient(const WindowCommandTransportConfig& config, std::optional<GeneratedTransportMap> transportMap)
    : CommandClient(CreateCommandClientChannel(config), std::move(transportMap))
{
    if (config.udp.has_value())
    {
        maxPayloadBytes_ = std::clamp(config.udp->maxPacketSize,
                                      std::size_t {1},
                                      kUdpHardPayloadLimit);
    }
}

CommandClient::CommandClient(const WindowUdpCommandTransport& config, std::optional<GeneratedTransportMap> transportMap)
    : CommandClient(CreateCommandClientChannel(config), std::move(transportMap))
{
    maxPayloadBytes_ = std::clamp(config.maxPacketSize,
                                  std::size_t {1},
                                  kUdpHardPayloadLimit);
}

bool CommandClient::NormalizeBatchForTransport(const CommandBatch& sourceBatch, CommandBatch& normalizedBatch)
{
    normalizedBatch = sourceBatch;

    auto requireTransportMap = [this](const std::string_view context) -> const GeneratedTransportMap*
    {
        if (!transportMap_.has_value())
        {
            lastError_ = "Command transport normalization requires a generated transport map: " + std::string(context);
            return nullptr;
        }

        return &(*transportMap_);
    };

    auto resolvePage = [this, &requireTransportMap](std::string& page,
                                                    TransportId& pageId,
                                                    const std::string_view context) -> bool
    {
        if (pageId != 0)
        {
            if (transportMap_.has_value())
            {
                const TransportMapPageEntry* resolved = FindPageById(*transportMap_, pageId);
                if (resolved == nullptr)
                {
                    lastError_ = "Unknown generated page id " + std::to_string(pageId) + " while normalizing " +
                                 std::string(context);
                    return false;
                }

                if (!page.empty() && NormalizePageName(page) != resolved->normalizedName)
                {
                    lastError_ = "Generated page id " + std::to_string(pageId) + " does not match page '" + page + "'";
                    return false;
                }

                page = resolved->name;
            }

            return true;
        }

        if (page.empty())
        {
            lastError_ = std::string(context) + " requires a target page";
            return false;
        }

        const GeneratedTransportMap* map = requireTransportMap(context);
        if (map == nullptr)
        {
            return false;
        }

        const TransportMapPageEntry* resolved = FindPageByName(*map, page);
        if (resolved == nullptr)
        {
            lastError_ = "Unknown page '" + page + "' while normalizing " + std::string(context);
            return false;
        }

        pageId = resolved->id;
        page = resolved->name;
        return true;
    };

    auto resolveStaticReticle = [this, &requireTransportMap](StaticReticleHandle& target,
                                                             const std::string_view context) -> bool
    {
        if (target.pageId == 0)
        {
            lastError_ = std::string(context) + " requires a generated pageId";
            return false;
        }

        if (target.reticleId != 0)
        {
            if (transportMap_.has_value())
            {
                const auto iterator = std::find_if(
                    transportMap_->reticles.begin(),
                    transportMap_->reticles.end(),
                    [&target](const TransportMapReticleEntry& entry)
                    {
                        return entry.id == target.reticleId;
                    });
                if (iterator == transportMap_->reticles.end())
                {
                    lastError_ = "Unknown generated static reticle id " + std::to_string(target.reticleId);
                    return false;
                }

                if (iterator->pageId != target.pageId)
                {
                    lastError_ = "Generated static reticle id " + std::to_string(target.reticleId) +
                                 " does not belong to page id " + std::to_string(target.pageId);
                    return false;
                }

                if (!target.reticle.empty() && NormalizePageName(target.reticle) != iterator->normalizedReticleId)
                {
                    lastError_ = "Generated static reticle id " + std::to_string(target.reticleId) +
                                 " does not match reticle '" + target.reticle + "'";
                    return false;
                }

                target.reticle = iterator->reticleId;
            }

            return true;
        }

        if (target.reticle.empty())
        {
            lastError_ = std::string(context) + " requires a target reticle";
            return false;
        }

        const GeneratedTransportMap* map = requireTransportMap(context);
        if (map == nullptr)
        {
            return false;
        }

        const TransportMapReticleEntry* resolved = FindStaticReticle(*map, target.pageId, target.reticle);
        if (resolved == nullptr)
        {
            lastError_ = "Unknown static reticle '" + target.reticle + "' on page '" + target.page + "'";
            return false;
        }

        target.reticleId = resolved->id;
        target.reticle = resolved->reticleId;
        return true;
    };

    auto resolveTemplate = [this, &requireTransportMap](std::string& templateId,
                                                        TransportId& templateTransportId,
                                                        const std::string_view context) -> bool
    {
        if (templateTransportId != 0)
        {
            if (transportMap_.has_value())
            {
                const TransportMapTemplateEntry* resolved = FindTemplateById(*transportMap_, templateTransportId);
                if (resolved == nullptr)
                {
                    lastError_ = "Unknown generated template id " + std::to_string(templateTransportId);
                    return false;
                }

                if (!templateId.empty() && NormalizePageName(templateId) != resolved->normalizedTemplateId)
                {
                    lastError_ = "Generated template id " + std::to_string(templateTransportId) +
                                 " does not match template '" + templateId + "'";
                    return false;
                }

                templateId = resolved->templateId;
            }

            return true;
        }

        if (templateId.empty())
        {
            lastError_ = std::string(context) + " requires a dynamic template";
            return false;
        }

        const GeneratedTransportMap* map = requireTransportMap(context);
        if (map == nullptr)
        {
            return false;
        }

        const TransportMapTemplateEntry* resolved = FindTemplateByName(*map, templateId);
        if (resolved == nullptr)
        {
            lastError_ = "Unknown dynamic template '" + templateId + "'";
            return false;
        }

        templateTransportId = resolved->id;
        templateId = resolved->templateId;
        return true;
    };

    auto resolveDynamicRuntimeId = [this](const std::string_view page,
                                          DynamicReticleHandle& target,
                                          const std::string_view context) -> bool
    {
        if (target.runtimeReticleId != 0)
        {
            return true;
        }

        if (target.reticleId.empty())
        {
            lastError_ = std::string(context) + " requires a dynamic reticle runtime id or alias";
            return false;
        }

        target.runtimeReticleId = MakeStableNamedRuntimeDynamicId(page, target.reticleId);
        return true;
    };

    auto resolveDynamicStateIds = [this](const std::string_view page,
                                         std::vector<DynamicReticleState>& states,
                                         const std::string_view context) -> bool
    {
        for (DynamicReticleState& state : states)
        {
            if (state.runtimeReticleId != 0)
            {
                continue;
            }

            if (state.reticleId.empty())
            {
                lastError_ = std::string(context) + " requires a dynamic reticle runtime id or alias";
                return false;
            }

            state.runtimeReticleId = MakeStableNamedRuntimeDynamicId(page, state.reticleId);
        }

        return true;
    };

    auto normalizePatch = [this, &requireTransportMap](ReticlePatch& patch,
                                                       const TransportId pageId,
                                                       const TransportPrimitiveOwnerKind ownerKind,
                                                       const TransportId ownerId,
                                                       const std::string_view context) -> bool
    {
        if (patch.blinkType.has_value() && !patch.blinkTypeId.has_value())
        {
            if (patch.blinkType->empty())
            {
                patch.blinkTypeId = TransportId {0};
            }
            else
            {
                const GeneratedTransportMap* map = requireTransportMap(context);
                if (map == nullptr)
                {
                    return false;
                }

                if (pageId == 0)
                {
                    lastError_ = std::string(context) + " requires a generated pageId to resolve blinkType";
                    return false;
                }

                const TransportMapBlinkTypeEntry* resolved = FindBlinkType(*map, pageId, *patch.blinkType);
                if (resolved == nullptr)
                {
                    lastError_ = "Unknown blink type '" + *patch.blinkType + "'";
                    return false;
                }

                patch.blinkTypeId = resolved->id;
            }
        }

        if (!patch.texts.empty() || !patch.letterSpacings.empty() || !patch.primitivePatches.empty())
        {
            if (ownerId == 0)
            {
                lastError_ = std::string(context) + " requires generated owner ids to resolve named primitive fields";
                return false;
            }

            const GeneratedTransportMap* map = requireTransportMap(context);
            if (map == nullptr)
            {
                return false;
            }

            auto moveNamedText = [&map, ownerKind, ownerId, this, &context](std::unordered_map<std::string, std::string>& named,
                                                                            std::unordered_map<TransportId, std::string>& byId) -> bool
            {
                for (const auto& [primitiveId, value] : named)
                {
                    const TransportMapPrimitiveEntry* resolved = FindPrimitiveByOwner(*map, ownerKind, ownerId, primitiveId);
                    if (resolved == nullptr)
                    {
                        lastError_ = "Unknown primitive '" + primitiveId + "' while normalizing " + std::string(context);
                        return false;
                    }

                    byId.insert_or_assign(resolved->id, value);
                }

                named.clear();
                return true;
            };

            auto moveNamedSpacing = [&map, ownerKind, ownerId, this, &context](std::unordered_map<std::string, float>& named,
                                                                               std::unordered_map<TransportId, float>& byId) -> bool
            {
                for (const auto& [primitiveId, value] : named)
                {
                    const TransportMapPrimitiveEntry* resolved = FindPrimitiveByOwner(*map, ownerKind, ownerId, primitiveId);
                    if (resolved == nullptr)
                    {
                        lastError_ = "Unknown primitive '" + primitiveId + "' while normalizing " + std::string(context);
                        return false;
                    }

                    byId.insert_or_assign(resolved->id, value);
                }

                named.clear();
                return true;
            };

            auto moveNamedPatch = [&map, ownerKind, ownerId, this, &context](
                                      std::unordered_map<std::string, PrimitivePatch>& named,
                                      std::unordered_map<TransportId, PrimitivePatch>& byId) -> bool
            {
                for (const auto& [primitiveId, value] : named)
                {
                    const TransportMapPrimitiveEntry* resolved = FindPrimitiveByOwner(*map, ownerKind, ownerId, primitiveId);
                    if (resolved == nullptr)
                    {
                        lastError_ = "Unknown primitive '" + primitiveId + "' while normalizing " + std::string(context);
                        return false;
                    }

                    byId.insert_or_assign(resolved->id, value);
                }

                named.clear();
                return true;
            };

            if (!moveNamedText(patch.texts, patch.textsById) ||
                !moveNamedSpacing(patch.letterSpacings, patch.letterSpacingsById) ||
                !moveNamedPatch(patch.primitivePatches, patch.primitivePatchesById))
            {
                return false;
            }
        }

        return true;
    };

    for (UserCommand& command : normalizedBatch.commands)
    {
        const bool ok = std::visit(
            [this,
             &resolvePage,
             &resolveStaticReticle,
             &resolveTemplate,
             &resolveDynamicRuntimeId,
             &resolveDynamicStateIds,
             &normalizePatch](auto& value) -> bool
            {
                using Command = std::decay_t<decltype(value)>;

                if constexpr (std::is_same_v<Command, ActivatePageCommand>)
                {
                    return resolvePage(value.page, value.pageId, "ActivatePageCommand");
                }
                else if constexpr (std::is_same_v<Command, SetPageViewCommand>)
                {
                    return resolvePage(value.page, value.pageId, "SetPageViewCommand");
                }
                else if constexpr (std::is_same_v<Command, UpdateWindowDisplayCommand> ||
                                   std::is_same_v<Command, ResetWindowCommand>)
                {
                    return true;
                }
                else if constexpr (std::is_same_v<Command, UpdateReticleCommand>)
                {
                    return resolvePage(value.target.page, value.target.pageId, "UpdateReticleCommand") &&
                           resolveStaticReticle(value.target, "UpdateReticleCommand") &&
                           normalizePatch(
                               value.patch,
                               value.target.pageId,
                               TransportPrimitiveOwnerKind::Reticle,
                               value.target.reticleId,
                               "UpdateReticleCommand");
                }
                else if constexpr (std::is_same_v<Command, UpdateStrobeCommand>)
                {
                    return resolvePage(value.page, value.pageId, "UpdateStrobeCommand");
                }
                else if constexpr (std::is_same_v<Command, UpsertDynamicReticleCommand>)
                {
                    return resolvePage(value.target.page, value.target.pageId, "UpsertDynamicReticleCommand") &&
                           resolveDynamicRuntimeId(value.target.page, value.target, "UpsertDynamicReticleCommand") &&
                           resolveTemplate(
                               value.templateId,
                               value.templateTransportId,
                               "UpsertDynamicReticleCommand") &&
                           normalizePatch(
                               value.patch,
                               value.target.pageId,
                               TransportPrimitiveOwnerKind::Template,
                               value.templateTransportId,
                               "UpsertDynamicReticleCommand");
                }
                else if constexpr (std::is_same_v<Command, UpsertDynamicReticlesCommand>)
                {
                    if (!resolvePage(value.page, value.pageId, "UpsertDynamicReticlesCommand") ||
                        !resolveTemplate(
                            value.templateId,
                            value.templateTransportId,
                            "UpsertDynamicReticlesCommand") ||
                        !resolveDynamicStateIds(value.page, value.reticles, "UpsertDynamicReticlesCommand"))
                    {
                        return false;
                    }

                    for (DynamicReticleState& state : value.reticles)
                    {
                        if (!normalizePatch(
                                state.patch,
                                value.pageId,
                                TransportPrimitiveOwnerKind::Template,
                                value.templateTransportId,
                                "UpsertDynamicReticlesCommand"))
                        {
                            return false;
                        }
                    }

                    return true;
                }
                else if constexpr (std::is_same_v<Command, SetDynamicReticleSetVisibilityCommand>)
                {
                    return resolvePage(value.page, value.pageId, "SetDynamicReticleSetVisibilityCommand") &&
                           resolveTemplate(
                               value.templateId,
                               value.templateTransportId,
                               "SetDynamicReticleSetVisibilityCommand");
                }
                else if constexpr (std::is_same_v<Command, RemoveDynamicReticleCommand>)
                {
                    return resolvePage(value.target.page, value.target.pageId, "RemoveDynamicReticleCommand") &&
                           resolveDynamicRuntimeId(value.target.page, value.target, "RemoveDynamicReticleCommand");
                }
                else
                {
                    return true;
                }
            },
            command);

        if (!ok)
        {
            return false;
        }
    }

    const bool usesGeneratedIdentifiers = std::any_of(
        normalizedBatch.commands.begin(),
        normalizedBatch.commands.end(),
        [](const UserCommand& command)
        {
            return CommandUsesGeneratedIdentifiers(command);
        });

    if (usesGeneratedIdentifiers)
    {
        if (normalizedBatch.mappingHash.empty())
        {
            const GeneratedTransportMap* map = requireTransportMap("mapping hash");
            if (map == nullptr)
            {
                return false;
            }

            normalizedBatch.mappingHash = map->mappingHash;
        }
        else if (transportMap_.has_value() && normalizedBatch.mappingHash != transportMap_->mappingHash)
        {
            lastError_ = "Command batch mappingHash does not match the generated transport map configured on the client";
            return false;
        }
    }

    return true;
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

bool CommandClient::SendBatch(const ArrayView<const UserCommand> commands, const std::uint32_t sequence)
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

bool CommandClient::ActivateGeneratedPage(const TransportId pageId, const std::string_view mappingHash)
{
    CommandBatch batch;
    batch.mappingHash = std::string(mappingHash);

    ActivatePageCommand command;
    command.pageId = pageId;
    batch.commands.emplace_back(command);
    return SendBatch(batch);
}

bool CommandClient::SetPageView(const std::string_view page, const Vec2 center, const float zoom)
{
    return Send(SetPageViewCommand {std::string(page), PageViewState {center, zoom}});
}

bool CommandClient::SetGeneratedPageView(const TransportId pageId,
                                         const std::string_view mappingHash,
                                         const Vec2 center,
                                         const float zoom)
{
    CommandBatch batch;
    batch.mappingHash = std::string(mappingHash);

    SetPageViewCommand command;
    command.pageId = pageId;
    command.view = PageViewState {center, zoom};
    batch.commands.emplace_back(command);
    return SendBatch(batch);
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
    return Send(UpdateReticleCommand {StaticReticleHandle {std::string(page), std::string(reticle)}, patch});
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
    UpdateStrobeCommand command;
    command.page = std::string(page);
    command.active = active;
    return Send(command);
}

bool CommandClient::SetStrobePosition(const std::string_view page, const Vec2 position)
{
    UpdateStrobeCommand command;
    command.page = std::string(page);
    command.position = position;
    return Send(command);
}

bool CommandClient::UpsertDynamicReticle(const std::string_view page,
                                         const std::string_view reticle,
                                         const std::string_view templateId,
                                         const ReticlePatch& patch)
{
    UpsertDynamicReticleCommand command;
    command.target = DynamicReticleHandle {std::string(page), std::string(reticle)};
    command.templateId = std::string(templateId);
    command.patch = patch;
    return Send(command);
}

bool CommandClient::UpsertDynamicReticles(const std::string_view page,
                                          const std::string_view templateId,
                                          const ArrayView<const DynamicReticleState> reticles)
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
    SetDynamicReticleSetVisibilityCommand command;
    command.page = std::string(page);
    command.templateId = std::string(templateId);
    command.visible = visible;
    return Send(command);
}

bool CommandClient::RemoveDynamicReticle(const std::string_view page, const std::string_view reticle)
{
    return Send(RemoveDynamicReticleCommand {DynamicReticleHandle {std::string(page), std::string(reticle)}});
}

bool CommandClient::ResetWindow()
{
    return Send(ResetWindowCommand {});
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
        const ByteView payloadView(payloadBytes, payload.size());

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

    CommandBatch normalizedBatch;
    if (!NormalizeBatchForTransport(batch, normalizedBatch))
    {
        return false;
    }

    const std::size_t maxPayloadBytes = MaxPayloadBytes();
    std::string error;
    std::vector<UserCommand> expandedCommands;
    expandedCommands.reserve(normalizedBatch.commands.size());

    for (const UserCommand& command : normalizedBatch.commands)
    {
        const auto splitCommands = SplitOversizedCommand(
            command,
            normalizedBatch.sequence,
            normalizedBatch.mappingHash,
            maxPayloadBytes,
            error);
        if (!splitCommands.has_value())
        {
            lastError_ = std::move(error);
            return false;
        }

        expandedCommands.insert(expandedCommands.end(), splitCommands->begin(), splitCommands->end());
    }

    CommandBatch currentChunk;
    currentChunk.sequence = normalizedBatch.sequence;
    currentChunk.mappingHash = normalizedBatch.mappingHash;

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
        singleCommandChunk.sequence = normalizedBatch.sequence;
        singleCommandChunk.mappingHash = normalizedBatch.mappingHash;
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
