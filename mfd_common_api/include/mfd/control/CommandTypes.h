/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Serializable user commands exchanged through UDP using Protocol Buffers.
 */

#include <optional>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#include "mfd/MfdExport.h"
#include "mfd/model/Types.h"
#include "mfd/runtime/GeneratedTransportMap.h"

namespace mfd
{
/**
 * @brief Public identifier of one authored static reticle.
 */
struct StaticReticleHandle
{
    /**
     * @brief Target page name.
     *
     * @note This convenience field is resolved to `pageId` before
     * serialization. Protocol Buffers transport carries only `pageId`.
     */
    std::string page;
    /**
     * @brief Target authored reticle id unique within the page.
     *
     * @note This convenience field is resolved to `reticleId` before
     * serialization. Protocol Buffers transport carries only `reticleId`.
     */
    std::string reticle;
    /** @brief Optional generated transport id of the target page. */
    TransportId pageId = 0;
    /** @brief Optional generated transport id of the target static reticle. */
    TransportId reticleId = 0;
};

/**
 * @brief Public identifier of one runtime dynamic reticle instance.
 */
struct DynamicReticleHandle
{
    /**
     * @brief Target page name.
     *
     * @note This convenience field is resolved to `pageId` before
     * serialization. Protocol Buffers transport carries only `pageId`.
     */
    std::string page;
    /**
     * @brief Optional human-readable alias of the dynamic reticle.
     *
     * @note This field is kept only for local client-side convenience. The
     * command transport serializes `runtimeReticleId` instead.
     */
    std::string reticleId;
    /** @brief Optional generated transport id of the target page. */
    TransportId pageId = 0;
    /** @brief Runtime-scoped identifier of the target dynamic reticle. */
    RuntimeDynamicId runtimeReticleId = 0;
};

/**
 * @brief Partial primitive update payload nested inside a reticle patch.
 *
 * @note Fields left empty are ignored by the runtime.
 */
struct PrimitivePatch
{
    /** @brief Optional primitive visibility override. */
    std::optional<bool> visible;
    /** @brief Optional primitive position override in local reticle space. */
    std::optional<Vec2> position;
    /** @brief Optional primitive rotation override in degrees. */
    std::optional<float> rotationDegrees;
    /** @brief Optional primitive non-uniform scale override. */
    std::optional<Vec2> scale;
    /** @brief Optional primitive stroke color override. */
    std::optional<ColorRgba> color;
    /** @brief Optional primitive fill color override. */
    std::optional<ColorRgba> fillColor;
    /** @brief Optional filled-state override. */
    std::optional<bool> filled;
    /** @brief Optional primitive line thickness override. */
    std::optional<float> thickness;
    /** @brief Optional primitive stroke pattern override. */
    std::optional<LineStyle> lineStyle;
    /** @brief Optional text payload for text-like primitives. */
    std::optional<std::string> text;
    /** @brief Optional letter spacing override for text-like primitives. */
    std::optional<float> letterSpacing;
    /** @brief Optional line start override for line primitives. */
    std::optional<Vec2> lineStart;
    /** @brief Optional line end override for line primitives. */
    std::optional<Vec2> lineEnd;
    /** @brief Optional radius override for circle-like primitives. */
    std::optional<float> radius;
    /** @brief Optional inner radius override for ring primitives. */
    std::optional<float> innerRadius;
    /** @brief Optional outer radius override for ring primitives. */
    std::optional<float> outerRadius;
    /** @brief Optional width override for rectangular primitives. */
    std::optional<float> width;
    /** @brief Optional height override for rectangular primitives. */
    std::optional<float> height;
    /** @brief Optional combined size override for two-axis primitives. */
    std::optional<Vec2> size;
    /** @brief Optional point list override for triangle, polyline, and bezier primitives. */
    std::optional<std::vector<Vec2>> points;
    /** @brief Optional loop-closing override for polyline primitives. */
    std::optional<bool> closed;
    /** @brief Optional segment-count override for sampled primitives. */
    std::optional<int> segments;
    /** @brief Optional start-angle override for arc primitives. */
    std::optional<float> startAngleDegrees;
    /** @brief Optional end-angle override for arc primitives. */
    std::optional<float> endAngleDegrees;
};

/**
 * @brief Partial reticle update payload used by user commands.
 *
 * @note Fields left empty are ignored by the runtime.
 */
struct ReticlePatch
{
    /** @brief Optional visibility override. */
    std::optional<bool> visible;
    /** @brief Optional blink enable flag. */
    std::optional<bool> blinkEnabled;
    /**
     * @brief Optional blink type override resolved on the target page.
     *
     * @note An empty string explicitly clears the reticle-specific blink type
     * and falls back to the page default when blinking stays enabled.
     *
     * @note This convenience field is resolved to `blinkTypeId` before
     * serialization. Protocol Buffers transport carries only `blinkTypeId`.
     */
    std::optional<std::string> blinkType;
    /**
     * @brief Optional generated transport id of the target blink type.
     *
     * @note A value of `0` explicitly clears the reticle-specific blink type
     * when the field is present.
     */
    std::optional<TransportId> blinkTypeId;
    /** @brief Optional reticle position override in logical coordinates. */
    std::optional<Vec2> position;
    /** @brief Optional reticle rotation override in degrees. */
    std::optional<float> rotationDegrees;
    /** @brief Optional reticle non-uniform scale override. */
    std::optional<Vec2> scale;
    /** @brief Optional stroke color override. */
    std::optional<ColorRgba> color;
    /** @brief Optional line thickness override. */
    std::optional<float> thickness;
    /** @brief Optional text value applied to the first text primitive. */
    std::optional<std::string> text;
    /**
     * @brief Per-primitive text overrides indexed by primitive id.
     *
     * @note This convenience field is resolved to `textsById` before
     * serialization. Protocol Buffers transport carries only `textsById`.
     */
    std::unordered_map<std::string, std::string> texts;
    /** @brief Per-primitive text overrides indexed by generated primitive transport id. */
    std::unordered_map<TransportId, std::string> textsById;
    /** @brief Optional character spacing applied to the first text primitive. */
    std::optional<float> letterSpacing;
    /**
     * @brief Per-primitive character spacing overrides indexed by primitive id.
     *
     * @note This convenience field is resolved to `letterSpacingsById` before
     * serialization. Protocol Buffers transport carries only
     * `letterSpacingsById`.
     */
    std::unordered_map<std::string, float> letterSpacings;
    /** @brief Per-primitive character spacing overrides indexed by generated primitive transport id. */
    std::unordered_map<TransportId, float> letterSpacingsById;
    /**
     * @brief Rich primitive overrides indexed by primitive id.
     *
     * @note This convenience field is resolved to `primitivePatchesById`
     * before serialization. Protocol Buffers transport carries only
     * `primitivePatchesById`.
     */
    std::unordered_map<std::string, PrimitivePatch> primitivePatches;
    /** @brief Rich primitive overrides indexed by generated primitive transport id. */
    std::unordered_map<TransportId, PrimitivePatch> primitivePatchesById;
};

/**
 * @brief Partial window-level display update payload used by user commands.
 *
 * @note `brightness` is a normalized factor in the `[0, 1]` range where
 * `1.0` keeps the original luminance and `0.0` blacks the rendered window out.
 *
 * @note `disabled` hides the whole rendered page output behind a black screen
 * without mutating the underlying scene state.
 */
struct WindowDisplayPatch
{
    /** @brief Optional whole-window color inversion flag. */
    std::optional<bool> invertColors;
    /** @brief Optional whole-window brightness factor clamped to `[0, 1]`. */
    std::optional<float> brightness;
    /** @brief Optional whole-window blackout flag. */
    std::optional<bool> disabled;
};

/**
 * @brief Command activating a page.
 */
struct ActivatePageCommand
{
    /**
     * @brief Page name to activate.
     *
     * @note This convenience field is resolved to `pageId` before
     * serialization. Protocol Buffers transport carries only `pageId`.
     */
    std::string page;
    /** @brief Optional generated transport id of the page to activate. */
    TransportId pageId = 0;
};

/**
 * @brief Command updating the view center and zoom of a page.
 */
struct SetPageViewCommand
{
    /**
     * @brief Page name to update.
     *
     * @note This convenience field is resolved to `pageId` before
     * serialization. Protocol Buffers transport carries only `pageId`.
     */
    std::string page;
    /** @brief New center and zoom applied to the page. */
    PageViewState view {};
    /** @brief Optional generated transport id of the page to update. */
    TransportId pageId = 0;
};

/**
 * @brief Command updating whole-window display properties.
 */
struct UpdateWindowDisplayCommand
{
    /** @brief Window-level display patch applied to the renderer output. */
    WindowDisplayPatch patch;
};

/**
 * @brief Command updating a static or dynamic reticle on a page.
 */
struct UpdateReticleCommand
{
    /** @brief Target reticle identified by page name and reticle id. */
    StaticReticleHandle target;
    /** @brief Properties to update on the target reticle. */
    ReticlePatch patch;
};

/**
 * @brief Command updating the optional strobe attached to a page.
 */
struct UpdateStrobeCommand
{
    /**
     * @brief Page owning the strobe to update.
     *
     * @note This convenience field is resolved to `pageId` before
     * serialization. Protocol Buffers transport carries only `pageId`.
     */
    std::string page;
    /** @brief Optional generated transport id of the page owning the strobe. */
    TransportId pageId = 0;
    /**
     * @brief Optional strobe catalog name selected on the page.
     *
     * @note This convenience field is resolved to `strobeId` before
     * serialization. Protocol Buffers transport carries only `strobeId`.
     */
    std::string strobe;
    /** @brief Optional generated transport id of the selected strobe entry. */
    TransportId strobeId = 0;
    /** @brief Optional activation flag for the strobe. */
    std::optional<bool> active;
    /** @brief Optional logical position override for the strobe. */
    std::optional<Vec2> position;
};

/**
 * @brief Command creating or updating a dynamic reticle from a template.
 */
struct UpsertDynamicReticleCommand
{
    /** @brief Target dynamic reticle identified by page and runtime instance id. */
    DynamicReticleHandle target;
    /**
     * @brief Template id used when the dynamic reticle must be created.
     *
     * @note This convenience field is resolved to `templateTransportId`
     * before serialization. Protocol Buffers transport carries only
     * `templateTransportId`.
     */
    std::string templateId;
    /** @brief Optional generated transport id of the authored template used for creation. */
    TransportId templateTransportId = 0;
    /** @brief Properties applied after creation or update. */
    ReticlePatch patch;
};

/**
 * @brief One dynamic reticle update entry used by bulk dynamic commands.
 */
struct DynamicReticleState
{
    /**
     * @brief Optional local alias of the dynamic reticle.
     *
     * @note This field is kept only for local client-side convenience. The
     * transport serializes `runtimeReticleId` instead.
     */
    std::string reticleId;
    /** @brief Runtime-scoped identifier of the dynamic reticle instance. */
    RuntimeDynamicId runtimeReticleId = 0;
    /** @brief Properties applied to the target dynamic reticle. */
    ReticlePatch patch;
};

/**
 * @brief Command creating or updating many dynamic reticles from one template.
 */
struct UpsertDynamicReticlesCommand
{
    /**
     * @brief Target page receiving the dynamic reticles.
     *
     * @note This convenience field is resolved to `pageId` before
     * serialization. Protocol Buffers transport carries only `pageId`.
     */
    std::string page;
    /** @brief Optional generated transport id of the target page. */
    TransportId pageId = 0;
    /**
     * @brief Template id used when missing reticles must be created.
     *
     * @note This convenience field is resolved to `templateTransportId`
     * before serialization. Protocol Buffers transport carries only
     * `templateTransportId`.
     */
    std::string templateId;
    /** @brief Optional generated transport id of the authored template used for creation. */
    TransportId templateTransportId = 0;
    /** @brief Dynamic reticle updates applied during the same cycle. */
    std::vector<DynamicReticleState> reticles;
};

/**
 * @brief Command toggling visibility for all dynamic reticles of one page/template set.
 */
struct SetDynamicReticleSetVisibilityCommand
{
    /**
     * @brief Target page receiving the dynamic reticles.
     *
     * @note This convenience field is resolved to `pageId` before
     * serialization. Protocol Buffers transport carries only `pageId`.
     */
    std::string page;
    /** @brief Optional generated transport id of the target page. */
    TransportId pageId = 0;
    /**
     * @brief Dynamic reticle template id defining the set.
     *
     * @note This convenience field is resolved to `templateTransportId`
     * before serialization. Protocol Buffers transport carries only
     * `templateTransportId`.
     */
    std::string templateId;
    /** @brief Optional generated transport id of the authored template defining the set. */
    TransportId templateTransportId = 0;
    /** @brief Visibility applied to every dynamic reticle of the set. */
    bool visible = true;
};

/**
 * @brief Command removing a dynamic reticle from a page.
 */
struct RemoveDynamicReticleCommand
{
    /** @brief Target dynamic reticle identified by page and runtime instance id. */
    DynamicReticleHandle target;
};

/**
 * @brief Command restoring one window runtime to its document-defined initial state.
 *
 * @note This command resets active page selection, page views, whole-window
 * display parameters, all dynamic reticles and strobe runtime state.
 */
struct ResetWindowCommand
{
};

/**
 * @brief Variant containing every command accepted by the user command API.
 */
using UserCommand = std::variant<ActivatePageCommand,
                                 SetPageViewCommand,
                                 UpdateWindowDisplayCommand,
                                 UpdateReticleCommand,
                                 UpdateStrobeCommand,
                                 UpsertDynamicReticleCommand,
                                 UpsertDynamicReticlesCommand,
                                 SetDynamicReticleSetVisibilityCommand,
                                 RemoveDynamicReticleCommand,
                                 ResetWindowCommand>;

/**
 * @brief Batch of user commands sent during the same external update cycle.
 *
 * @note `sequence` is user-defined and can be used to tag one simulation frame
 * or one 20 ms update cycle.
 */
struct CommandBatch
{
    /** @brief Optional external cycle identifier. */
    std::uint32_t sequence = 0;
    /**
     * @brief Optional generation hash validating the authored-name to transport-id mapping.
     *
     * @note This field becomes required as soon as one command in the batch uses
     * generated transport ids instead of authored names.
     */
    std::string mappingHash;
    /** @brief Commands carried by the batch. */
    std::vector<UserCommand> commands;
};

/**
 * @brief Serializes a command to a Protocol Buffers envelope.
 * @param command Command to serialize.
 * @return Binary payload ready to be sent through UDP.
 */
MFD_API std::string SerializeUserCommand(const UserCommand& command);

/**
 * @brief Serializes a batch of commands to one Protocol Buffers envelope.
 * @param batch Batch to serialize.
 * @return Binary payload ready to be sent through UDP.
 */
MFD_API std::string SerializeCommandBatch(const CommandBatch& batch);

/**
 * @brief Deserializes a Protocol Buffers envelope into one typed command batch.
 * @param payload Raw binary Protocol Buffers payload containing a command batch.
 * @param error Optional output string receiving the parsing error.
 * @return Parsed batch, or `std::nullopt` if the payload is invalid.
 */
MFD_API std::optional<CommandBatch> DeserializeCommandBatch(std::string_view payload, std::string* error = nullptr);

/**
 * @brief Deserializes a Protocol Buffers envelope into a typed command.
 * @param payload Raw binary Protocol Buffers payload.
 * @param error Optional output string receiving the parsing error.
 * @return Parsed command, or `std::nullopt` if the payload is invalid.
 */
MFD_API std::optional<UserCommand> DeserializeUserCommand(std::string_view payload, std::string* error = nullptr);

/**
 * @brief Deserializes a Protocol Buffers envelope into one or more typed commands.
 * @param payload Raw binary Protocol Buffers payload containing one or more commands.
 * @param error Optional output string receiving the parsing error.
 * @return Parsed commands, or `std::nullopt` if the payload is invalid.
 */
MFD_API std::optional<std::vector<UserCommand>> DeserializeUserCommands(std::string_view payload,
                                                                        std::string* error = nullptr);
} // namespace mfd
