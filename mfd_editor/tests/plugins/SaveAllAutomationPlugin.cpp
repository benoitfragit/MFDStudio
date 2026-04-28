/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Test automation plugin DLL validating commit plus host-owned save-all through the stable ABI.
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

struct SaveAllAutomationPluginContext
{
    const MfdEditorAutomationHostApi* host = nullptr;
    bool started = false;
    bool tickApplied = false;
};

MfdEditorAutomationResultCode MFD_EDITOR_AUTOMATION_CALL StartPlugin(void* pluginContext,
                                                                     const MfdEditorAutomationHostApi* host,
                                                                     MfdEditorUtf8Buffer* error) noexcept
{
    if (pluginContext == nullptr || host == nullptr ||
        host->begin_session == nullptr ||
        host->create_page_asset == nullptr ||
        host->validate_session == nullptr ||
        host->commit_session == nullptr ||
        host->save_all == nullptr)
    {
        WriteMessage(error, "The editor host did not provide the required save-all callbacks.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    auto* context = static_cast<SaveAllAutomationPluginContext*>(pluginContext);
    context->host = host;
    context->started = true;
    return MfdEditorAutomationResultCode_Success;
}

MfdEditorAutomationResultCode MFD_EDITOR_AUTOMATION_CALL TickPlugin(void* pluginContext,
                                                                    MfdEditorUtf8Buffer* error) noexcept
{
    auto* context = static_cast<SaveAllAutomationPluginContext*>(pluginContext);
    if (context == nullptr || !context->started || context->host == nullptr)
    {
        WriteMessage(error, "The save-all automation plugin was not started.");
        return MfdEditorAutomationResultCode_InvalidState;
    }
    if (context->tickApplied)
    {
        return MfdEditorAutomationResultCode_Success;
    }

    MfdEditorAutomationSessionHandle sessionHandle = 0U;
    MfdEditorAutomationResultCode status =
        context->host->begin_session(context->host->host_context, MakeView("save-all"), &sessionHandle, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    char createdPageIdStorage[128] {};
    MfdEditorAutomationCreatePageAssetRequest createPage {};
    createPage.struct_size = sizeof(createPage);
    createPage.session = sessionHandle;
    createPage.name = MakeView("SavedPluginPage");
    createPage.title = MakeView("Saved Plugin Page");
    createPage.relative_source_path = MakeView("saved_plugin_page.json");
    createPage.created_page_id = MfdEditorUtf8Buffer {createdPageIdStorage, sizeof(createdPageIdStorage), 0U};
    status = context->host->create_page_asset(context->host->host_context, &createPage, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        context->host->rollback_session(context->host->host_context, sessionHandle, nullptr);
        return status;
    }

    MfdEditorAutomationValidationSummary validation {};
    validation.struct_size = sizeof(validation);
    status = context->host->validate_session(context->host->host_context, sessionHandle, &validation, error);
    if (status != MfdEditorAutomationResultCode_Success || validation.valid == 0U)
    {
        context->host->rollback_session(context->host->host_context, sessionHandle, nullptr);
        if (status == MfdEditorAutomationResultCode_Success)
        {
            WriteMessage(error, "The save-all automation session should validate before commit.");
            return MfdEditorAutomationResultCode_ValidationFailed;
        }
        return status;
    }

    status = context->host->commit_session(context->host->host_context, sessionHandle, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    MfdEditorAutomationSaveSummary saveSummary {};
    saveSummary.struct_size = sizeof(saveSummary);
    status = context->host->save_all(context->host->host_context, &saveSummary, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }
    if (saveSummary.saved_file_count < 3U)
    {
        WriteMessage(error, "The save-all automation plugin expected at least three saved files.");
        return MfdEditorAutomationResultCode_InternalFailure;
    }

    context->tickApplied = true;
    return MfdEditorAutomationResultCode_Success;
}

void MFD_EDITOR_AUTOMATION_CALL StopPlugin(void* pluginContext) noexcept
{
    if (auto* context = static_cast<SaveAllAutomationPluginContext*>(pluginContext); context != nullptr)
    {
        context->host = nullptr;
        context->started = false;
    }
}

void MFD_EDITOR_AUTOMATION_CALL DestroyPlugin(void* pluginContext) noexcept
{
    delete static_cast<SaveAllAutomationPluginContext*>(pluginContext);
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

    auto* context = new (std::nothrow) SaveAllAutomationPluginContext();
    if (context == nullptr)
    {
        WriteMessage(error, "Unable to allocate the save-all automation plugin context.");
        return MfdEditorAutomationResultCode_InternalFailure;
    }

    *outApi = {};
    outApi->struct_size = sizeof(*outApi);
    outApi->info.struct_size = sizeof(outApi->info);
    outApi->info.abi_version = MFD_EDITOR_AUTOMATION_PLUGIN_ABI_VERSION;
    outApi->info.plugin_id = MakeView("test.save.all.automation.plugin");
    outApi->info.display_name = MakeView("MFD Editor Save All Automation Plugin");
    outApi->plugin_context = context;
    outApi->start = &StartPlugin;
    outApi->tick = &TickPlugin;
    outApi->stop = &StopPlugin;
    outApi->destroy = &DestroyPlugin;
    return MfdEditorAutomationResultCode_Success;
}

