/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Test automation plugin DLL proving that the stable ABI accepts stateless plugins with a null context.
 */

#include "mfd/editor/AutomationPlugin.h"

#include <cstring>

namespace
{
MfdEditorStringView MakeView(const char* text) noexcept
{
    return MfdEditorStringView {text, text == nullptr ? 0U : std::strlen(text)};
}

MfdEditorAutomationResultCode WriteMessage(MfdEditorUtf8Buffer* buffer, const char* text) noexcept
{
    if (buffer == nullptr)
    {
        return MfdEditorAutomationResultCode_Success;
    }

    const std::size_t length = text == nullptr ? 0U : std::strlen(text);
    buffer->size = length;
    if (buffer->data == nullptr || buffer->capacity == 0U)
    {
        return MfdEditorAutomationResultCode_Success;
    }

    const std::size_t writable = length < (buffer->capacity - 1U) ? length : (buffer->capacity - 1U);
    if (writable > 0U && text != nullptr)
    {
        std::memcpy(buffer->data, text, writable);
    }
    buffer->data[writable] = '\0';
    return writable == length ? MfdEditorAutomationResultCode_Success : MfdEditorAutomationResultCode_BufferTooSmall;
}

MfdEditorAutomationResultCode MFD_EDITOR_AUTOMATION_CALL StartPlugin(void*,
                                                                     const MfdEditorAutomationHostApi* host,
                                                                     MfdEditorUtf8Buffer* error) noexcept
{
    if (host == nullptr || host->get_snapshot_summary == nullptr)
    {
        WriteMessage(error, "The editor host did not provide the required snapshot callback.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    return MfdEditorAutomationResultCode_Success;
}

MfdEditorAutomationResultCode MFD_EDITOR_AUTOMATION_CALL TickPlugin(void*, MfdEditorUtf8Buffer*) noexcept
{
    return MfdEditorAutomationResultCode_Success;
}

void MFD_EDITOR_AUTOMATION_CALL StopPlugin(void*) noexcept
{
}

void MFD_EDITOR_AUTOMATION_CALL DestroyPlugin(void*) noexcept
{
}
} // namespace

extern "C" MFD_EDITOR_AUTOMATION_EXPORT MfdEditorAutomationResultCode MFD_EDITOR_AUTOMATION_CALL
MfdGetEditorAutomationPluginApi(MfdEditorAutomationPluginApi* outApi, MfdEditorUtf8Buffer* error) noexcept
{
    if (outApi == nullptr)
    {
        WriteMessage(error, "The plugin API output descriptor is null.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    *outApi = {};
    outApi->struct_size = sizeof(*outApi);
    outApi->info.struct_size = sizeof(outApi->info);
    outApi->info.abi_version = MFD_EDITOR_AUTOMATION_PLUGIN_ABI_VERSION;
    outApi->info.plugin_id = MakeView("test.stateless.automation.plugin");
    outApi->info.display_name = MakeView("MFD Editor Stateless Automation Plugin");
    outApi->plugin_context = nullptr;
    outApi->start = &StartPlugin;
    outApi->tick = &TickPlugin;
    outApi->stop = &StopPlugin;
    outApi->destroy = &DestroyPlugin;
    return MfdEditorAutomationResultCode_Success;
}

