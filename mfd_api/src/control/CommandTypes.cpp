/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation for CommandTypes.
 */

#include "mfd/control/CommandTypes.h"

#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <type_traits>

#include "mfd_commands.pb.h"

namespace mfd
{
namespace
{
namespace pb = ::mfd::transport;
constexpr std::size_t kMaxCommandPayloadBytes = 1024U * 1024U;

template <typename>
inline constexpr bool kUnsupportedCommand = false;

std::uint32_t PackColor(const ColorRgba& color) noexcept
{
    return static_cast<std::uint32_t>(color.r) |
           (static_cast<std::uint32_t>(color.g) << 8U) |
           (static_cast<std::uint32_t>(color.b) << 16U) |
           (static_cast<std::uint32_t>(color.a) << 24U);
}

ColorRgba UnpackColor(const std::uint32_t packed) noexcept
{
    return ColorRgba {
        static_cast<std::uint8_t>(packed & 0xFFU),
        static_cast<std::uint8_t>((packed >> 8U) & 0xFFU),
        static_cast<std::uint8_t>((packed >> 16U) & 0xFFU),
        static_cast<std::uint8_t>((packed >> 24U) & 0xFFU)};
}

void FillProtoVec2(const Vec2& value, pb::Vec2* target)
{
    target->set_x(value.x);
    target->set_y(value.y);
}

Vec2 FromProtoVec2(const pb::Vec2& value) noexcept
{
    return Vec2 {value.x(), value.y()};
}

void FillProtoWindowDisplayPatch(const WindowDisplayPatch& patch, pb::WindowDisplayPatch* target)
{
    if (patch.invertColors.has_value())
    {
        target->set_invert_colors(*patch.invertColors);
    }

    if (patch.brightness.has_value())
    {
        target->set_brightness(*patch.brightness);
    }

    if (patch.disabled.has_value())
    {
        target->set_disabled(*patch.disabled);
    }
}

WindowDisplayPatch FromProtoWindowDisplayPatch(const pb::WindowDisplayPatch& value)
{
    WindowDisplayPatch patch;

    if (value.has_invert_colors())
    {
        patch.invertColors = value.invert_colors();
    }

    if (value.has_brightness())
    {
        patch.brightness = value.brightness();
    }

    if (value.has_disabled())
    {
        patch.disabled = value.disabled();
    }

    return patch;
}

void FillProtoReticlePatch(const ReticlePatch& patch, pb::ReticlePatch* target)
{
    if (patch.visible.has_value())
    {
        target->set_visible(*patch.visible);
    }

    if (patch.blinkEnabled.has_value())
    {
        target->set_blink_enabled(*patch.blinkEnabled);
    }

    if (patch.blinkType.has_value())
    {
        target->set_blink_type(*patch.blinkType);
    }

    if (patch.position.has_value())
    {
        FillProtoVec2(*patch.position, target->mutable_position());
    }

    if (patch.rotationDegrees.has_value())
    {
        target->set_rotation_degrees(*patch.rotationDegrees);
    }

    if (patch.color.has_value())
    {
        target->set_packed_rgba(PackColor(*patch.color));
    }

    if (patch.thickness.has_value())
    {
        target->set_thickness(*patch.thickness);
    }

    if (patch.text.has_value())
    {
        target->set_text(*patch.text);
    }

    for (const auto& [primitiveId, text] : patch.texts)
    {
        (*target->mutable_texts())[primitiveId] = text;
    }

    if (patch.letterSpacing.has_value())
    {
        target->set_letter_spacing(*patch.letterSpacing);
    }

    for (const auto& [primitiveId, spacing] : patch.letterSpacings)
    {
        (*target->mutable_letter_spacings())[primitiveId] = spacing;
    }
}

ReticlePatch FromProtoReticlePatch(const pb::ReticlePatch& value)
{
    ReticlePatch patch;

    if (value.has_visible())
    {
        patch.visible = value.visible();
    }

    if (value.has_blink_enabled())
    {
        patch.blinkEnabled = value.blink_enabled();
    }

    if (value.has_blink_type())
    {
        patch.blinkType = value.blink_type();
    }

    if (value.has_position())
    {
        patch.position = FromProtoVec2(value.position());
    }

    if (value.has_rotation_degrees())
    {
        patch.rotationDegrees = value.rotation_degrees();
    }

    if (value.has_packed_rgba())
    {
        patch.color = UnpackColor(value.packed_rgba());
    }

    if (value.has_thickness())
    {
        patch.thickness = value.thickness();
    }

    if (value.has_text())
    {
        patch.text = value.text();
    }

    for (const auto& [primitiveId, text] : value.texts())
    {
        patch.texts.emplace(primitiveId, text);
    }

    if (value.has_letter_spacing())
    {
        patch.letterSpacing = value.letter_spacing();
    }

    for (const auto& [primitiveId, spacing] : value.letter_spacings())
    {
        patch.letterSpacings.emplace(primitiveId, spacing);
    }

    return patch;
}

void FillProtoHandle(const ReticleHandle& handle, pb::ReticleHandle* target)
{
    target->set_page(handle.page);
    target->set_reticle(handle.reticle);
}

ReticleHandle FromProtoHandle(const pb::ReticleHandle& value)
{
    return ReticleHandle {value.page(), value.reticle()};
}

void FillProtoDynamicReticleState(const DynamicReticleState& state, pb::DynamicReticleState* target)
{
    target->set_reticle(state.reticleId);
    FillProtoReticlePatch(state.patch, target->mutable_patch());
}

DynamicReticleState FromProtoDynamicReticleState(const pb::DynamicReticleState& value)
{
    DynamicReticleState state;
    state.reticleId = value.reticle();
    if (value.has_patch())
    {
        state.patch = FromProtoReticlePatch(value.patch());
    }
    return state;
}

void FillProtoUserCommand(const UserCommand& command, pb::UserCommand* target)
{
    std::visit(
        [target](const auto& value)
        {
            using Command = std::decay_t<decltype(value)>;

            if constexpr (std::is_same_v<Command, ActivatePageCommand>)
            {
                target->mutable_activate_page()->set_page(value.page);
            }
            else if constexpr (std::is_same_v<Command, SetPageViewCommand>)
            {
                auto* message = target->mutable_set_page_view();
                message->set_page(value.page);
                FillProtoVec2(value.view.center, message->mutable_center());
                message->set_zoom(value.view.zoom);
            }
            else if constexpr (std::is_same_v<Command, UpdateWindowDisplayCommand>)
            {
                FillProtoWindowDisplayPatch(value.patch, target->mutable_update_window_display()->mutable_patch());
            }
            else if constexpr (std::is_same_v<Command, UpdateReticleCommand>)
            {
                auto* message = target->mutable_update_reticle();
                FillProtoHandle(value.target, message->mutable_target());
                FillProtoReticlePatch(value.patch, message->mutable_patch());
            }
            else if constexpr (std::is_same_v<Command, UpdateStrobeCommand>)
            {
                auto* message = target->mutable_update_strobe();
                message->set_page(value.page);

                if (value.active.has_value())
                {
                    message->set_active(*value.active);
                }

                if (value.position.has_value())
                {
                    FillProtoVec2(*value.position, message->mutable_position());
                }
            }
            else if constexpr (std::is_same_v<Command, UpsertDynamicReticleCommand>)
            {
                auto* message = target->mutable_upsert_dynamic_reticle();
                FillProtoHandle(value.target, message->mutable_target());
                message->set_template_id(value.templateId);
                FillProtoReticlePatch(value.patch, message->mutable_patch());
            }
            else if constexpr (std::is_same_v<Command, UpsertDynamicReticlesCommand>)
            {
                auto* message = target->mutable_upsert_dynamic_reticles();
                message->set_page(value.page);
                message->set_template_id(value.templateId);

                for (const DynamicReticleState& reticle : value.reticles)
                {
                    FillProtoDynamicReticleState(reticle, message->add_reticles());
                }
            }
            else if constexpr (std::is_same_v<Command, SetDynamicReticleSetVisibilityCommand>)
            {
                auto* message = target->mutable_set_dynamic_reticle_set_visibility();
                message->set_page(value.page);
                message->set_template_id(value.templateId);
                message->set_visible(value.visible);
            }
            else if constexpr (std::is_same_v<Command, RemoveDynamicReticleCommand>)
            {
                FillProtoHandle(value.target, target->mutable_remove_dynamic_reticle()->mutable_target());
            }
            else if constexpr (std::is_same_v<Command, ResetWindowCommand>)
            {
                target->mutable_reset_window();
            }
            else
            {
                static_assert(kUnsupportedCommand<Command>, "Unsupported user command type");
            }
        },
        command);
}

UserCommand FromProtoUserCommand(const pb::UserCommand& value)
{
    switch (value.command_case())
    {
    case pb::UserCommand::kActivatePage:
        return ActivatePageCommand {value.activate_page().page()};

    case pb::UserCommand::kSetPageView:
    {
        SetPageViewCommand command;
        command.page = value.set_page_view().page();
        if (value.set_page_view().has_center())
        {
            command.view.center = FromProtoVec2(value.set_page_view().center());
        }
        command.view.zoom = value.set_page_view().zoom();
        return command;
    }

    case pb::UserCommand::kUpdateWindowDisplay:
    {
        UpdateWindowDisplayCommand command;
        if (value.update_window_display().has_patch())
        {
            command.patch = FromProtoWindowDisplayPatch(value.update_window_display().patch());
        }
        return command;
    }

    case pb::UserCommand::kUpdateReticle:
    {
        UpdateReticleCommand command;
        command.target = FromProtoHandle(value.update_reticle().target());
        if (value.update_reticle().has_patch())
        {
            command.patch = FromProtoReticlePatch(value.update_reticle().patch());
        }
        return command;
    }

    case pb::UserCommand::kUpdateStrobe:
    {
        UpdateStrobeCommand command;
        command.page = value.update_strobe().page();
        if (value.update_strobe().has_active())
        {
            command.active = value.update_strobe().active();
        }
        if (value.update_strobe().has_position())
        {
            command.position = FromProtoVec2(value.update_strobe().position());
        }
        return command;
    }

    case pb::UserCommand::kUpsertDynamicReticle:
    {
        UpsertDynamicReticleCommand command;
        command.target = FromProtoHandle(value.upsert_dynamic_reticle().target());
        command.templateId = value.upsert_dynamic_reticle().template_id();
        if (value.upsert_dynamic_reticle().has_patch())
        {
            command.patch = FromProtoReticlePatch(value.upsert_dynamic_reticle().patch());
        }
        return command;
    }

    case pb::UserCommand::kUpsertDynamicReticles:
    {
        UpsertDynamicReticlesCommand command;
        command.page = value.upsert_dynamic_reticles().page();
        command.templateId = value.upsert_dynamic_reticles().template_id();
        command.reticles.reserve(value.upsert_dynamic_reticles().reticles_size());

        for (const pb::DynamicReticleState& reticle : value.upsert_dynamic_reticles().reticles())
        {
            command.reticles.push_back(FromProtoDynamicReticleState(reticle));
        }

        return command;
    }

    case pb::UserCommand::kSetDynamicReticleSetVisibility:
    {
        SetDynamicReticleSetVisibilityCommand command;
        command.page = value.set_dynamic_reticle_set_visibility().page();
        command.templateId = value.set_dynamic_reticle_set_visibility().template_id();
        command.visible = value.set_dynamic_reticle_set_visibility().visible();
        return command;
    }

    case pb::UserCommand::kRemoveDynamicReticle:
        return RemoveDynamicReticleCommand {FromProtoHandle(value.remove_dynamic_reticle().target())};

    case pb::UserCommand::kResetWindow:
        return ResetWindowCommand {};

    case pb::UserCommand::COMMAND_NOT_SET:
        break;
    }

    throw std::runtime_error("Protocol Buffers command payload is empty");
}

pb::CommandEnvelope BuildEnvelope(const CommandBatch& batch)
{
    pb::CommandEnvelope envelope;
    envelope.set_sequence(batch.sequence);

    for (const UserCommand& command : batch.commands)
    {
        FillProtoUserCommand(command, envelope.add_commands());
    }

    return envelope;
}
} // namespace

std::string SerializeUserCommand(const UserCommand& command)
{
    CommandBatch batch;
    batch.commands.push_back(command);
    return SerializeCommandBatch(batch);
}

std::string SerializeCommandBatch(const CommandBatch& batch)
{
    const pb::CommandEnvelope envelope = BuildEnvelope(batch);
    std::string payload;
    if (!envelope.SerializeToString(&payload))
    {
        throw std::runtime_error("Unable to serialize Protocol Buffers command payload");
    }

    return payload;
}

std::optional<UserCommand> DeserializeUserCommand(const std::string_view payload, std::string* error)
{
    const auto commands = DeserializeUserCommands(payload, error);
    if (!commands.has_value())
    {
        return std::nullopt;
    }

    if (commands->size() != 1U)
    {
        if (error != nullptr)
        {
            *error = "Command payload contains more than one command";
        }
        return std::nullopt;
    }

    return commands->front();
}

std::optional<std::vector<UserCommand>> DeserializeUserCommands(const std::string_view payload, std::string* error)
{
    if (payload.empty())
    {
        if (error != nullptr)
        {
            *error = "Protocol Buffers command payload is empty";
        }

        return std::nullopt;
    }

    if (payload.size() > kMaxCommandPayloadBytes)
    {
        if (error != nullptr)
        {
            *error = "Protocol Buffers command payload exceeds safety size limit";
        }

        return std::nullopt;
    }

    try
    {
        pb::CommandEnvelope envelope;
        if (!envelope.ParseFromArray(payload.data(), static_cast<int>(payload.size())))
        {
            throw std::runtime_error("Unable to parse Protocol Buffers command payload");
        }

        std::vector<UserCommand> commands;
        commands.reserve(envelope.commands_size());

        for (const pb::UserCommand& command : envelope.commands())
        {
            commands.push_back(FromProtoUserCommand(command));
        }

        return commands;
    }
    catch (const std::exception& exception)
    {
        if (error != nullptr)
        {
            *error = exception.what();
        }

        return std::nullopt;
    }
}
} // namespace mfd
