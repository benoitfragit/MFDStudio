/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Minimal sample automation plugin DLL querying the stable host ABI without mutating the editor.
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

struct SampleAutomationPluginContext
{
    const MfdEditorAutomationHostApiV1* host = nullptr;
    bool started = false;
};

MfdEditorAutomationResultCode MFD_EDITOR_AUTOMATION_CALL StartPlugin(void* pluginContext,
                                                                     const MfdEditorAutomationHostApiV1* host,
                                                                     MfdEditorUtf8Buffer* error) noexcept
{
    if (pluginContext == nullptr || host == nullptr || host->get_snapshot_summary == nullptr)
    {
        WriteMessage(error, "The editor host did not provide the required snapshot callback.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    auto* context = static_cast<SampleAutomationPluginContext*>(pluginContext);
    context->host = host;

    char windowIdStorage[128] {};
    char windowTitleStorage[128] {};
    MfdEditorAutomationSnapshotSummaryV1 summary {};
    summary.struct_size = sizeof(summary);
    summary.window_id = MfdEditorUtf8Buffer {windowIdStorage, sizeof(windowIdStorage), 0U};
    summary.window_title = MfdEditorUtf8Buffer {windowTitleStorage, sizeof(windowTitleStorage), 0U};
    const MfdEditorAutomationResultCode status =
        host->get_snapshot_summary(host->host_context, &summary, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    context->started = true;
    return MfdEditorAutomationResultCode_Success;
}

MfdEditorAutomationResultCode MFD_EDITOR_AUTOMATION_CALL TickPlugin(void* pluginContext,
                                                                    MfdEditorUtf8Buffer*) noexcept
{
    const auto* context = static_cast<const SampleAutomationPluginContext*>(pluginContext);
    return context != nullptr && context->started ? MfdEditorAutomationResultCode_Success
                                                  : MfdEditorAutomationResultCode_InvalidState;
}

void MFD_EDITOR_AUTOMATION_CALL StopPlugin(void* pluginContext) noexcept
{
    if (auto* context = static_cast<SampleAutomationPluginContext*>(pluginContext); context != nullptr)
    {
        context->host = nullptr;
        context->started = false;
    }
}

void MFD_EDITOR_AUTOMATION_CALL DestroyPlugin(void* pluginContext) noexcept
{
    delete static_cast<SampleAutomationPluginContext*>(pluginContext);
}
} // namespace

extern "C" MFD_EDITOR_AUTOMATION_EXPORT MfdEditorAutomationResultCode MFD_EDITOR_AUTOMATION_CALL
MfdGetEditorAutomationPluginApi(MfdEditorAutomationPluginApiV1* outApi, MfdEditorUtf8Buffer* error) noexcept
{
    if (outApi == nullptr)
    {
        WriteMessage(error, "The plugin API output descriptor is null.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    auto* context = new (std::nothrow) SampleAutomationPluginContext();
    if (context == nullptr)
    {
        WriteMessage(error, "Unable to allocate the sample automation plugin context.");
        return MfdEditorAutomationResultCode_InternalFailure;
    }

    *outApi = {};
    outApi->struct_size = sizeof(*outApi);
    outApi->info.struct_size = sizeof(outApi->info);
    outApi->info.abi_version = MFD_EDITOR_AUTOMATION_PLUGIN_ABI_VERSION;
    outApi->info.plugin_id = MakeView("sample.automation");
    outApi->info.display_name = MakeView("MFD Editor Automation Sample");
    outApi->plugin_context = context;
    outApi->start = &StartPlugin;
    outApi->tick = &TickPlugin;
    outApi->stop = &StopPlugin;
    outApi->destroy = &DestroyPlugin;
    return MfdEditorAutomationResultCode_Success;
}
