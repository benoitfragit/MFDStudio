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

namespace mfd
{
/**
 * @brief Public reticle identifier used by command transports.
 */
struct ReticleHandle
{
    /** @brief Target page name. */
    std::string page;
    /** @brief Target reticle id unique within the page. */
    std::string reticle;
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
     */
    std::optional<std::string> blinkType;
    /** @brief Optional reticle position override in logical coordinates. */
    std::optional<Vec2> position;
    /** @brief Optional reticle rotation override in degrees. */
    std::optional<float> rotationDegrees;
    /** @brief Optional stroke color override. */
    std::optional<ColorRgba> color;
    /** @brief Optional line thickness override. */
    std::optional<float> thickness;
    /** @brief Optional text value applied to the first text primitive. */
    std::optional<std::string> text;
    /** @brief Per-primitive text overrides indexed by primitive id. */
    std::unordered_map<std::string, std::string> texts;
    /** @brief Optional character spacing applied to the first text primitive. */
    std::optional<float> letterSpacing;
    /** @brief Per-primitive character spacing overrides indexed by primitive id. */
    std::unordered_map<std::string, float> letterSpacings;
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
 * @brief Command activating a page by name.
 */
struct ActivatePageCommand
{
    /** @brief Page name to activate. */
    std::string page;
};

/**
 * @brief Command updating the view center and zoom of a page.
 */
struct SetPageViewCommand
{
    /** @brief Page name to update. */
    std::string page;
    /** @brief New center and zoom applied to the page. */
    PageViewState view {};
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
    ReticleHandle target;
    /** @brief Properties to update on the target reticle. */
    ReticlePatch patch;
};

/**
 * @brief Command updating the optional strobe attached to a page.
 */
struct UpdateStrobeCommand
{
    /** @brief Page owning the strobe to update. */
    std::string page;
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
    /** @brief Target dynamic reticle identified by page name and reticle id. */
    ReticleHandle target;
    /** @brief Template id used when the dynamic reticle must be created. */
    std::string templateId;
    /** @brief Properties applied after creation or update. */
    ReticlePatch patch;
};

/**
 * @brief One dynamic reticle update entry used by bulk dynamic commands.
 */
struct DynamicReticleState
{
    /** @brief Public id of the dynamic reticle. */
    std::string reticleId;
    /** @brief Properties applied to the target dynamic reticle. */
    ReticlePatch patch;
};

/**
 * @brief Command creating or updating many dynamic reticles from one template.
 */
struct UpsertDynamicReticlesCommand
{
    /** @brief Target page receiving the dynamic reticles. */
    std::string page;
    /** @brief Template id used when missing reticles must be created. */
    std::string templateId;
    /** @brief Dynamic reticle updates applied during the same cycle. */
    std::vector<DynamicReticleState> reticles;
};

/**
 * @brief Command toggling visibility for all dynamic reticles of one page/template set.
 */
struct SetDynamicReticleSetVisibilityCommand
{
    /** @brief Target page receiving the dynamic reticles. */
    std::string page;
    /** @brief Dynamic reticle template id defining the set. */
    std::string templateId;
    /** @brief Visibility applied to every dynamic reticle of the set. */
    bool visible = true;
};

/**
 * @brief Command removing a dynamic reticle from a page.
 */
struct RemoveDynamicReticleCommand
{
    /** @brief Target dynamic reticle identified by page name and reticle id. */
    ReticleHandle target;
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
                                 RemoveDynamicReticleCommand>;

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
