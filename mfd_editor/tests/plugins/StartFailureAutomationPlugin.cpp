/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Test automation plugin DLL refusing to start with one explicit error message.
 */

#include "mfd/editor/AutomationPlugin.h"

#include <cstring>
#include <new>

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

struct StartFailureAutomationPluginContext
{
};

MfdEditorAutomationResultCode MFD_EDITOR_AUTOMATION_CALL StartPlugin(void*,
                                                                     const MfdEditorAutomationHostApi*,
                                                                     MfdEditorUtf8Buffer* error) noexcept
{
    WriteMessage(error, "Start failure from test plugin");
    return MfdEditorAutomationResultCode_ValidationFailed;
}

MfdEditorAutomationResultCode MFD_EDITOR_AUTOMATION_CALL TickPlugin(void*, MfdEditorUtf8Buffer*) noexcept
{
    return MfdEditorAutomationResultCode_Success;
}

void MFD_EDITOR_AUTOMATION_CALL StopPlugin(void*) noexcept
{
}

void MFD_EDITOR_AUTOMATION_CALL DestroyPlugin(void* pluginContext) noexcept
{
    delete static_cast<StartFailureAutomationPluginContext*>(pluginContext);
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

    auto* context = new (std::nothrow) StartFailureAutomationPluginContext();
    if (context == nullptr)
    {
        WriteMessage(error, "Unable to allocate the start-failure automation plugin context.");
        return MfdEditorAutomationResultCode_InternalFailure;
    }

    *outApi = {};
    outApi->struct_size = sizeof(*outApi);
    outApi->info.struct_size = sizeof(outApi->info);
    outApi->info.abi_version = MFD_EDITOR_AUTOMATION_PLUGIN_ABI_VERSION;
    outApi->info.plugin_id = MakeView("test.start.failure.automation.plugin");
    outApi->info.display_name = MakeView("MFD Editor Start Failure Automation Plugin");
    outApi->plugin_context = context;
    outApi->start = &StartPlugin;
    outApi->tick = &TickPlugin;
    outApi->stop = &StopPlugin;
    outApi->destroy = &DestroyPlugin;
    return MfdEditorAutomationResultCode_Success;
}

