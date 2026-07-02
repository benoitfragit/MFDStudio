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
#include <utility>
#include <vector>

#include "mfd/ipc/ExchangeChannel.h"
#include "mfd/ipc/UdpLimits.h"
#include "mfd/model/PageName.h"
#include "mfd/control/internal/CommandIdentifierHelpers.h"
#include "mfd/control/internal/CommandWireSizeHelpers.h"

namespace mfd
{
namespace
{
constexpr RuntimeDynamicId kGeneratedDynamicRuntimeIdBit = RuntimeDynamicId {1} << 63U;
constexpr std::size_t kMaxDynamicReticlesPerSplit = 4096U;

std::uint64_t AppendFnv1aHash(std::uint64_t hash, const std::string_view value) noexcept
{
    constexpr std::uint64_t kFnvPrime = 1099511628211ull;
    for (const unsigned char ch : value)
    {
        hash ^= static_cast<std::uint64_t>(ch);
        hash *= kFnvPrime;
    }

    return hash;
}

std::uint64_t HashNormalizedIdentifier(std::string_view lhs, std::string_view rhs) noexcept
{
    constexpr std::uint64_t kFnvOffset = 1469598103934665603ull;
    std::uint64_t hash = AppendFnv1aHash(kFnvOffset, lhs);
    hash = AppendFnv1aHash(hash, std::string_view {"\x1F", 1});
    hash = AppendFnv1aHash(hash, rhs);
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

const TransportMapStrobeEntry* FindStrobe(const GeneratedTransportMap& map,
                                          const TransportId pageId,
                                          const std::string_view strobeName) noexcept
{
    const std::string normalizedStrobeName = NormalizePageName(strobeName);
    const auto iterator = std::find_if(
        map.strobes.begin(),
        map.strobes.end(),
        [pageId, &normalizedStrobeName](const TransportMapStrobeEntry& entry)
        {
            return entry.pageId == pageId && entry.normalizedStrobeName == normalizedStrobeName;
        });
    return iterator == map.strobes.end() ? nullptr : &(*iterator);
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

bool FitsAsSingleCommandPayload(const UserCommand& command,
                                const std::uint32_t sequence,
                                const std::string_view mappingHash,
                                const std::size_t maxPayloadBytes,
                                std::string& error)
{
    const auto payload = TrySerializeBatch(
        MakeBatch(sequence, std::string(mappingHash), std::vector<UserCommand> {command}),
        error);
    return payload.has_value() && payload->size() <= maxPayloadBytes;
}

const GeneratedTransportMap* RequireTransportMap(const std::optional<GeneratedTransportMap>& transportMap,
                                                 std::string& lastError,
                                                 const std::string_view context)
{
    if (!transportMap.has_value())
    {
        lastError = "Command transport normalization requires a generated transport map: " + std::string(context);
        return nullptr;
    }

    return &(*transportMap);
}

bool ResolveGeneratedPage(const std::optional<GeneratedTransportMap>& transportMap,
                          std::string& lastError,
                          std::string& page,
                          TransportId& pageId,
                          const std::string_view context)
{
    if (pageId != 0)
    {
        if (transportMap.has_value())
        {
            const TransportMapPageEntry* resolved = FindPageById(*transportMap, pageId);
            if (resolved == nullptr)
            {
                lastError =
                    "Unknown generated page id " + std::to_string(pageId) + " while normalizing " + std::string(context);
                return false;
            }

            if (!page.empty() && NormalizePageName(page) != resolved->normalizedName)
            {
                lastError = "Generated page id " + std::to_string(pageId) + " does not match page '" + page + "'";
                return false;
            }

            page = resolved->name;
        }

        return true;
    }

    if (page.empty())
    {
        lastError = std::string(context) + " requires a target page";
        return false;
    }

    const GeneratedTransportMap* map = RequireTransportMap(transportMap, lastError, context);
    if (map == nullptr)
    {
        return false;
    }

    const TransportMapPageEntry* resolved = FindPageByName(*map, page);
    if (resolved == nullptr)
    {
        lastError = "Unknown page '" + page + "' while normalizing " + std::string(context);
        return false;
    }

    pageId = resolved->id;
    page = resolved->name;
    return true;
}

bool ResolveGeneratedStaticReticle(const std::optional<GeneratedTransportMap>& transportMap,
                                   std::string& lastError,
                                   StaticReticleHandle& target,
                                   const std::string_view context)
{
    if (target.pageId == 0)
    {
        lastError = std::string(context) + " requires a generated pageId";
        return false;
    }

    if (target.reticleId != 0)
    {
        if (transportMap.has_value())
        {
            const auto iterator = std::find_if(
                transportMap->reticles.begin(),
                transportMap->reticles.end(),
                [&target](const TransportMapReticleEntry& entry)
                {
                    return entry.id == target.reticleId;
                });
            if (iterator == transportMap->reticles.end())
            {
                lastError = "Unknown generated static reticle id " + std::to_string(target.reticleId);
                return false;
            }

            if (iterator->pageId != target.pageId)
            {
                lastError = "Generated static reticle id " + std::to_string(target.reticleId) +
                            " does not belong to page id " + std::to_string(target.pageId);
                return false;
            }

            if (!target.reticle.empty() && NormalizePageName(target.reticle) != iterator->normalizedReticleId)
            {
                lastError = "Generated static reticle id " + std::to_string(target.reticleId) +
                            " does not match reticle '" + target.reticle + "'";
                return false;
            }

            target.reticle = iterator->reticleId;
        }

        return true;
    }

    if (target.reticle.empty())
    {
        lastError = std::string(context) + " requires a target reticle";
        return false;
    }

    const GeneratedTransportMap* map = RequireTransportMap(transportMap, lastError, context);
    if (map == nullptr)
    {
        return false;
    }

    const TransportMapReticleEntry* resolved = FindStaticReticle(*map, target.pageId, target.reticle);
    if (resolved == nullptr)
    {
        lastError = "Unknown static reticle '" + target.reticle + "' on page '" + target.page + "'";
        return false;
    }

    target.reticleId = resolved->id;
    target.reticle = resolved->reticleId;
    return true;
}

bool ResolveGeneratedTemplate(const std::optional<GeneratedTransportMap>& transportMap,
                              std::string& lastError,
                              std::string& templateId,
                              TransportId& templateTransportId,
                              const std::string_view context)
{
    if (templateTransportId != 0)
    {
        if (transportMap.has_value())
        {
            const TransportMapTemplateEntry* resolved = FindTemplateById(*transportMap, templateTransportId);
            if (resolved == nullptr)
            {
                lastError = "Unknown generated template id " + std::to_string(templateTransportId);
                return false;
            }

            if (!templateId.empty() && NormalizePageName(templateId) != resolved->normalizedTemplateId)
            {
                lastError = "Generated template id " + std::to_string(templateTransportId) +
                            " does not match template '" + templateId + "'";
                return false;
            }

            templateId = resolved->templateId;
        }

        return true;
    }

    if (templateId.empty())
    {
        lastError = std::string(context) + " requires a dynamic template";
        return false;
    }

    const GeneratedTransportMap* map = RequireTransportMap(transportMap, lastError, context);
    if (map == nullptr)
    {
        return false;
    }

    const TransportMapTemplateEntry* resolved = FindTemplateByName(*map, templateId);
    if (resolved == nullptr)
    {
        lastError = "Unknown dynamic template '" + templateId + "'";
        return false;
    }

    templateTransportId = resolved->id;
    templateId = resolved->templateId;
    return true;
}

bool ResolveGeneratedStrobe(const std::optional<GeneratedTransportMap>& transportMap,
                            std::string& lastError,
                            const TransportId pageId,
                            std::string& strobeName,
                            TransportId& strobeId,
                            const std::string_view context)
{
    if (strobeId != 0)
    {
        if (transportMap.has_value())
        {
            const auto iterator = std::find_if(
                transportMap->strobes.begin(),
                transportMap->strobes.end(),
                [pageId, &strobeId](const TransportMapStrobeEntry& entry)
                {
                    return entry.id == strobeId && entry.pageId == pageId;
                });
            if (iterator == transportMap->strobes.end())
            {
                lastError = "Unknown generated strobe id " + std::to_string(strobeId);
                return false;
            }

            if (!strobeName.empty() && NormalizePageName(strobeName) != iterator->normalizedStrobeName)
            {
                lastError = "Generated strobe id " + std::to_string(strobeId) +
                            " does not match strobe '" + strobeName + "'";
                return false;
            }

            strobeName = iterator->strobeName;
        }

        return true;
    }

    if (strobeName.empty())
    {
        return true;
    }

    const GeneratedTransportMap* map = RequireTransportMap(transportMap, lastError, context);
    if (map == nullptr)
    {
        return false;
    }

    const TransportMapStrobeEntry* resolved = FindStrobe(*map, pageId, strobeName);
    if (resolved == nullptr)
    {
        lastError = "Unknown strobe '" + strobeName + "' on page id " + std::to_string(pageId);
        return false;
    }

    strobeId = resolved->id;
    strobeName = resolved->strobeName;
    return true;
}

bool ResolveDynamicRuntimeId(std::string& lastError,
                             const std::string_view page,
                             DynamicReticleHandle& target,
                             const std::string_view context)
{
    if (target.runtimeReticleId != 0)
    {
        return true;
    }

    if (target.reticleId.empty())
    {
        lastError = std::string(context) + " requires a dynamic reticle runtime id or alias";
        return false;
    }

    target.runtimeReticleId = MakeStableNamedRuntimeDynamicId(page, target.reticleId);
    return true;
}

bool ResolveDynamicStateIds(std::string& lastError,
                            const std::string_view page,
                            std::vector<DynamicReticleState>& states,
                            const std::string_view context)
{
    for (DynamicReticleState& state : states)
    {
        if (state.runtimeReticleId != 0)
        {
            continue;
        }

        if (state.reticleId.empty())
        {
            lastError = std::string(context) + " requires a dynamic reticle runtime id or alias";
            return false;
        }

        state.runtimeReticleId = MakeStableNamedRuntimeDynamicId(page, state.reticleId);
    }

    return true;
}

template <typename TValue>
bool MoveNamedPrimitiveFields(const GeneratedTransportMap& map,
                              std::unordered_map<std::string, TValue>& named,
                              std::unordered_map<TransportId, TValue>& byId,
                              const TransportPrimitiveOwnerKind ownerKind,
                              const TransportId ownerId,
                              std::string& lastError,
                              const std::string_view context)
{
    for (const auto& [primitiveId, value] : named)
    {
        const TransportMapPrimitiveEntry* resolved = FindPrimitiveByOwner(map, ownerKind, ownerId, primitiveId);
        if (resolved == nullptr)
        {
            lastError = "Unknown primitive '" + primitiveId + "' while normalizing " + std::string(context);
            return false;
        }

        byId.insert_or_assign(resolved->id, value);
    }

    named.clear();
    return true;
}

bool NormalizePatchForTransport(const std::optional<GeneratedTransportMap>& transportMap,
                                std::string& lastError,
                                ReticlePatch& patch,
                                const TransportId pageId,
                                const TransportPrimitiveOwnerKind ownerKind,
                                const TransportId ownerId,
                                const std::string_view context)
{
    if (patch.blinkType.has_value() && !patch.blinkTypeId.has_value())
    {
        if (patch.blinkType->empty())
        {
            patch.blinkTypeId = TransportId {0};
        }
        else
        {
            const GeneratedTransportMap* map = RequireTransportMap(transportMap, lastError, context);
            if (map == nullptr)
            {
                return false;
            }

            if (pageId == 0)
            {
                lastError = std::string(context) + " requires a generated pageId to resolve blinkType";
                return false;
            }

            const TransportMapBlinkTypeEntry* resolved = FindBlinkType(*map, pageId, *patch.blinkType);
            if (resolved == nullptr)
            {
                lastError = "Unknown blink type '" + *patch.blinkType + "'";
                return false;
            }

            patch.blinkTypeId = resolved->id;
        }
    }

    if (patch.texts.empty() && patch.letterSpacings.empty() && patch.primitivePatches.empty())
    {
        return true;
    }

    if (ownerId == 0)
    {
        lastError = std::string(context) + " requires generated owner ids to resolve named primitive fields";
        return false;
    }

    const GeneratedTransportMap* map = RequireTransportMap(transportMap, lastError, context);
    if (map == nullptr)
    {
        return false;
    }

    return MoveNamedPrimitiveFields(*map, patch.texts, patch.textsById, ownerKind, ownerId, lastError, context) &&
           MoveNamedPrimitiveFields(
               *map, patch.letterSpacings, patch.letterSpacingsById, ownerKind, ownerId, lastError, context) &&
           MoveNamedPrimitiveFields(
               *map, patch.primitivePatches, patch.primitivePatchesById, ownerKind, ownerId, lastError, context);
}

// Upper bound of the wire overhead wrapped around one serialized dynamic reticle state
// inside the enclosing command message: the repeated-field tag (at most 2 bytes for
// field numbers up to 2047) plus the length-prefix varint of the entry payload. Using an
// upper bound keeps the greedy chunk estimate safe; the one serialization per final
// chunk below still validates the real size defensively.
constexpr std::size_t kRepeatedFieldTagUpperBoundBytes = 2U;

std::size_t VarintWireSize(std::size_t value) noexcept
{
    std::size_t bytes = 1U;
    while (value >= 0x80U)
    {
        value >>= 7U;
        ++bytes;
    }

    return bytes;
}

std::size_t EstimatedDynamicReticleStateChunkBytes(const DynamicReticleState& state)
{
    const std::size_t payloadBytes = detail::DynamicReticleStateWireSize(state);
    return kRepeatedFieldTagUpperBoundBytes + VarintWireSize(payloadBytes) + payloadBytes;
}

// Splits one oversized command into transport-sized chunks with one explicit overload per
// command type. Only bulk dynamic reticle updates can shrink; every other command type
// states explicitly that it cannot. This visitor intentionally has no generic fallback:
// adding a new UserCommand alternative must fail to compile here until its splitting rule
// is written.
struct OversizedCommandSplitter
{
    std::uint32_t sequence = 0;
    std::string_view mappingHash;
    std::size_t maxPayloadBytes = 0;
    std::string& error;

    std::optional<std::vector<UserCommand>> RejectUnsplittableCommand() const
    {
        error = "A single command exceeds the configured UDP payload limit of " +
                std::to_string(maxPayloadBytes) + " bytes";
        return std::nullopt;
    }

    std::optional<std::vector<UserCommand>> operator()(const ActivatePageCommand&) const
    {
        return RejectUnsplittableCommand();
    }

    std::optional<std::vector<UserCommand>> operator()(const SetPageViewCommand&) const
    {
        return RejectUnsplittableCommand();
    }

    std::optional<std::vector<UserCommand>> operator()(const UpdateWindowDisplayCommand&) const
    {
        return RejectUnsplittableCommand();
    }

    std::optional<std::vector<UserCommand>> operator()(const UpdateReticleCommand&) const
    {
        return RejectUnsplittableCommand();
    }

    std::optional<std::vector<UserCommand>> operator()(const UpdateStrobeCommand&) const
    {
        return RejectUnsplittableCommand();
    }

    std::optional<std::vector<UserCommand>> operator()(const UpsertDynamicReticleCommand&) const
    {
        return RejectUnsplittableCommand();
    }

    std::optional<std::vector<UserCommand>> operator()(const UpsertDynamicReticlesCommand& command) const
    {
        if (command.reticles.empty())
        {
            return RejectUnsplittableCommand();
        }

        if (command.reticles.size() > kMaxDynamicReticlesPerSplit)
        {
            error = "Dynamic reticle bulk update exceeds runtime safety limits";
            return std::nullopt;
        }

        // The envelope cost (batch framing plus the command without its entries) is
        // measured with one serialization; every chunk then adds its entries' estimated
        // wire contributions on top instead of serializing a full candidate batch per
        // probe as the previous binary search did.
        UpsertDynamicReticlesCommand chunkPrototype;
        chunkPrototype.page = command.page;
        chunkPrototype.pageId = command.pageId;
        chunkPrototype.templateId = command.templateId;
        chunkPrototype.templateTransportId = command.templateTransportId;

        const auto emptyChunkPayload = TrySerializeBatch(
            MakeBatch(sequence, std::string(mappingHash), std::vector<UserCommand> {UserCommand {chunkPrototype}}),
            error);
        if (!emptyChunkPayload.has_value())
        {
            return std::nullopt;
        }

        if (emptyChunkPayload->size() >= maxPayloadBytes)
        {
            return RejectUnsplittableCommand();
        }

        const std::size_t chunkEntryBudget = maxPayloadBytes - emptyChunkPayload->size();

        std::vector<std::size_t> entryBytes;
        entryBytes.reserve(command.reticles.size());
        std::size_t totalEntryBytes = 0U;
        for (const DynamicReticleState& state : command.reticles)
        {
            entryBytes.push_back(EstimatedDynamicReticleStateChunkBytes(state));
            totalEntryBytes += entryBytes.back();
        }

        std::vector<UserCommand> splitCommands;
        splitCommands.reserve(totalEntryBytes / chunkEntryBudget + 1U);

        std::size_t firstIndex = 0U;
        while (firstIndex < command.reticles.size())
        {
            std::size_t entryCount = 0U;
            std::size_t usedBudget = 0U;
            while (firstIndex + entryCount < command.reticles.size() &&
                   usedBudget + entryBytes[firstIndex + entryCount] <= chunkEntryBudget)
            {
                usedBudget += entryBytes[firstIndex + entryCount];
                ++entryCount;
            }

            if (entryCount == 0U)
            {
                error = "One dynamic reticle update exceeds the configured UDP payload limit of " +
                        std::to_string(maxPayloadBytes) + " bytes";
                return std::nullopt;
            }

            UpsertDynamicReticlesCommand chunk = chunkPrototype;
            chunk.reticles.assign(command.reticles.begin() + static_cast<std::ptrdiff_t>(firstIndex),
                                  command.reticles.begin() + static_cast<std::ptrdiff_t>(firstIndex + entryCount));

            // Defensive validation: the estimate uses upper bounds and is expected to fit
            // on the first try; shrinking one entry at a time keeps the guarantee exact
            // even if the wire format ever grows past the estimated overhead.
            while (true)
            {
                const auto chunkPayload = TrySerializeBatch(
                    MakeBatch(sequence, std::string(mappingHash), std::vector<UserCommand> {UserCommand {chunk}}),
                    error);
                if (!chunkPayload.has_value())
                {
                    return std::nullopt;
                }

                if (chunkPayload->size() <= maxPayloadBytes)
                {
                    break;
                }

                if (chunk.reticles.size() == 1U)
                {
                    error = "One dynamic reticle update exceeds the configured UDP payload limit of " +
                            std::to_string(maxPayloadBytes) + " bytes";
                    return std::nullopt;
                }

                chunk.reticles.pop_back();
                --entryCount;
            }

            splitCommands.emplace_back(std::move(chunk));
            firstIndex += entryCount;
        }

        return splitCommands;
    }

    std::optional<std::vector<UserCommand>> operator()(const SetDynamicReticleSetVisibilityCommand&) const
    {
        return RejectUnsplittableCommand();
    }

    std::optional<std::vector<UserCommand>> operator()(const SetDynamicReticleSetStrobeMagnetEnabledCommand&) const
    {
        return RejectUnsplittableCommand();
    }

    std::optional<std::vector<UserCommand>> operator()(const RemoveDynamicReticleCommand&) const
    {
        return RejectUnsplittableCommand();
    }

    std::optional<std::vector<UserCommand>> operator()(const ResetWindowCommand&) const
    {
        return RejectUnsplittableCommand();
    }
};

std::optional<std::vector<UserCommand>> SplitOversizedCommand(const UserCommand& command,
                                                              const std::uint32_t sequence,
                                                              const std::string_view mappingHash,
                                                              const std::size_t maxPayloadBytes,
                                                              std::string& error)
{
    if (FitsAsSingleCommandPayload(command, sequence, mappingHash, maxPayloadBytes, error))
    {
        return std::vector<UserCommand> {command};
    }

    return std::visit(OversizedCommandSplitter {sequence, mappingHash, maxPayloadBytes, error}, command);
}

// Rewrites one typed command into its transport form (generated ids resolved, named fields
// moved to id-keyed fields) with one explicit overload per command type. This visitor
// intentionally has no generic fallback: adding a new UserCommand alternative must fail to
// compile here until its transport normalization rule is written.
struct CommandTransportNormalizer
{
    const std::optional<GeneratedTransportMap>& transportMap;
    std::string& lastError;

    bool operator()(ActivatePageCommand& command) const
    {
        return ResolveGeneratedPage(transportMap, lastError, command.page, command.pageId, "ActivatePageCommand");
    }

    bool operator()(SetPageViewCommand& command) const
    {
        return ResolveGeneratedPage(transportMap, lastError, command.page, command.pageId, "SetPageViewCommand");
    }

    bool operator()(UpdateWindowDisplayCommand&) const noexcept
    {
        return true;
    }

    bool operator()(UpdateReticleCommand& command) const
    {
        return ResolveGeneratedPage(
                   transportMap, lastError, command.target.page, command.target.pageId, "UpdateReticleCommand") &&
               ResolveGeneratedStaticReticle(transportMap, lastError, command.target, "UpdateReticleCommand") &&
               NormalizePatchForTransport(
                   transportMap,
                   lastError,
                   command.patch,
                   command.target.pageId,
                   TransportPrimitiveOwnerKind::Reticle,
                   command.target.reticleId,
                   "UpdateReticleCommand");
    }

    bool operator()(UpdateStrobeCommand& command) const
    {
        return ResolveGeneratedPage(transportMap, lastError, command.page, command.pageId, "UpdateStrobeCommand") &&
               ResolveGeneratedStrobe(
                   transportMap,
                   lastError,
                   command.pageId,
                   command.strobe,
                   command.strobeId,
                   "UpdateStrobeCommand");
    }

    bool operator()(UpsertDynamicReticleCommand& command) const
    {
        return ResolveGeneratedPage(
                   transportMap,
                   lastError,
                   command.target.page,
                   command.target.pageId,
                   "UpsertDynamicReticleCommand") &&
               ResolveDynamicRuntimeId(
                   lastError, command.target.page, command.target, "UpsertDynamicReticleCommand") &&
               ResolveGeneratedTemplate(
                   transportMap,
                   lastError,
                   command.templateId,
                   command.templateTransportId,
                   "UpsertDynamicReticleCommand") &&
               NormalizePatchForTransport(
                   transportMap,
                   lastError,
                   command.patch,
                   command.target.pageId,
                   TransportPrimitiveOwnerKind::Template,
                   command.templateTransportId,
                   "UpsertDynamicReticleCommand");
    }

    bool operator()(UpsertDynamicReticlesCommand& command) const
    {
        if (!ResolveGeneratedPage(
                transportMap,
                lastError,
                command.page,
                command.pageId,
                "UpsertDynamicReticlesCommand") ||
            !ResolveGeneratedTemplate(
                transportMap,
                lastError,
                command.templateId,
                command.templateTransportId,
                "UpsertDynamicReticlesCommand") ||
            !ResolveDynamicStateIds(
                lastError, command.page, command.reticles, "UpsertDynamicReticlesCommand"))
        {
            return false;
        }

        for (DynamicReticleState& state : command.reticles)
        {
            if (!NormalizePatchForTransport(
                    transportMap,
                    lastError,
                    state.patch,
                    command.pageId,
                    TransportPrimitiveOwnerKind::Template,
                    command.templateTransportId,
                    "UpsertDynamicReticlesCommand"))
            {
                return false;
            }
        }

        return true;
    }

    bool operator()(SetDynamicReticleSetVisibilityCommand& command) const
    {
        return ResolveGeneratedPage(
                   transportMap,
                   lastError,
                   command.page,
                   command.pageId,
                   "SetDynamicReticleSetVisibilityCommand") &&
               ResolveGeneratedTemplate(
                   transportMap,
                   lastError,
                   command.templateId,
                   command.templateTransportId,
                   "SetDynamicReticleSetVisibilityCommand");
    }

    bool operator()(SetDynamicReticleSetStrobeMagnetEnabledCommand& command) const
    {
        return ResolveGeneratedPage(
                   transportMap,
                   lastError,
                   command.page,
                   command.pageId,
                   "SetDynamicReticleSetStrobeMagnetEnabledCommand") &&
               ResolveGeneratedTemplate(
                   transportMap,
                   lastError,
                   command.templateId,
                   command.templateTransportId,
                   "SetDynamicReticleSetStrobeMagnetEnabledCommand");
    }

    bool operator()(RemoveDynamicReticleCommand& command) const
    {
        return ResolveGeneratedPage(
                   transportMap,
                   lastError,
                   command.target.page,
                   command.target.pageId,
                   "RemoveDynamicReticleCommand") &&
               ResolveDynamicRuntimeId(
                   lastError, command.target.page, command.target, "RemoveDynamicReticleCommand");
    }

    bool operator()(ResetWindowCommand&) const noexcept
    {
        return true;
    }
};
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
                                      kUdpMinPayloadBytes,
                                      kUdpMaxPayloadBytes);
    }
}

CommandClient::CommandClient(const WindowUdpCommandTransport& config, std::optional<GeneratedTransportMap> transportMap)
    : CommandClient(CreateCommandClientChannel(config), std::move(transportMap))
{
    maxPayloadBytes_ = std::clamp(config.maxPacketSize,
                                  kUdpMinPayloadBytes,
                                  kUdpMaxPayloadBytes);
}

bool CommandClient::NormalizeBatchForTransport(const CommandBatch& sourceBatch, CommandBatch& normalizedBatch)
{
    normalizedBatch = sourceBatch;

    for (UserCommand& command : normalizedBatch.commands)
    {
        if (!std::visit(CommandTransportNormalizer {transportMap_, lastError_}, command))
        {
            return false;
        }
    }

    const bool usesGeneratedIdentifiers = std::any_of(
        normalizedBatch.commands.begin(),
        normalizedBatch.commands.end(),
        [](const UserCommand& command)
        {
            return detail::CommandUsesGeneratedIdentifiers(command);
        });

    if (usesGeneratedIdentifiers)
    {
        if (normalizedBatch.mappingHash.empty())
        {
            const GeneratedTransportMap* map = RequireTransportMap(transportMap_, lastError_, "mapping hash");
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
    return maxPayloadBytes_ == 0 ? kUdpDefaultPayloadBytes : maxPayloadBytes_;
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

bool CommandClient::SelectStrobe(const std::string_view page, const std::string_view strobeName)
{
    UpdateStrobeCommand command;
    command.page = std::string(page);
    command.strobe = std::string(strobeName);
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

bool CommandClient::SetDynamicReticleSetStrobeMagnetEnabled(const std::string_view page,
                                                            const std::string_view templateId,
                                                            const bool enabled)
{
    SetDynamicReticleSetStrobeMagnetEnabledCommand command;
    command.page = std::string(page);
    command.templateId = std::string(templateId);
    command.enabled = enabled;
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

bool CommandClient::FlushPayloadChunk(CommandBatch& currentChunk, std::string& error)
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

        if (!FlushPayloadChunk(currentChunk, error))
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

    return FlushPayloadChunk(currentChunk, error);
}
} // namespace mfd
