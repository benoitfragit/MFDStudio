/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Test automation plugin DLL used by GoogleTest to validate the stable editor plugin ABI.
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

struct TestAutomationPluginContext
{
    const MfdEditorAutomationHostApiV1* host = nullptr;
    bool started = false;
    bool tickApplied = false;
};

MfdEditorAutomationResultCode MFD_EDITOR_AUTOMATION_CALL StartPlugin(void* pluginContext,
                                                                     const MfdEditorAutomationHostApiV1* host,
                                                                     MfdEditorUtf8Buffer* error) noexcept
{
    if (pluginContext == nullptr || host == nullptr ||
        host->get_snapshot_summary == nullptr ||
        host->get_page_info == nullptr ||
        host->begin_session == nullptr ||
        host->validate_session == nullptr ||
        host->create_page_asset == nullptr ||
        host->commit_session == nullptr ||
        host->rollback_session == nullptr)
    {
        WriteMessage(error, "The editor host did not provide the required automation callbacks.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    auto* context = static_cast<TestAutomationPluginContext*>(pluginContext);
    context->host = host;

    char windowIdStorage[128] {};
    char windowTitleStorage[128] {};
    MfdEditorAutomationSnapshotSummaryV1 summary {};
    summary.struct_size = sizeof(summary);
    summary.window_id = MfdEditorUtf8Buffer {windowIdStorage, sizeof(windowIdStorage), 0U};
    summary.window_title = MfdEditorUtf8Buffer {windowTitleStorage, sizeof(windowTitleStorage), 0U};
    const MfdEditorAutomationResultCode snapshotStatus =
        host->get_snapshot_summary(host->host_context, &summary, error);
    if (snapshotStatus != MfdEditorAutomationResultCode_Success)
    {
        return snapshotStatus;
    }
    if (summary.page_count == 0U)
    {
        WriteMessage(error, "The seed editor document must expose at least one page.");
        return MfdEditorAutomationResultCode_InvalidState;
    }

    char pageIdStorage[128] {};
    char pageNameStorage[128] {};
    char pageTitleStorage[128] {};
    char pagePathStorage[260] {};
    MfdEditorAutomationPageInfoV1 pageInfo {};
    pageInfo.struct_size = sizeof(pageInfo);
    pageInfo.page_id = MfdEditorUtf8Buffer {pageIdStorage, sizeof(pageIdStorage), 0U};
    pageInfo.page_name = MfdEditorUtf8Buffer {pageNameStorage, sizeof(pageNameStorage), 0U};
    pageInfo.page_title = MfdEditorUtf8Buffer {pageTitleStorage, sizeof(pageTitleStorage), 0U};
    pageInfo.source_path = MfdEditorUtf8Buffer {pagePathStorage, sizeof(pagePathStorage), 0U};
    const MfdEditorAutomationResultCode pageStatus =
        host->get_page_info(host->host_context, 0U, &pageInfo, error);
    if (pageStatus != MfdEditorAutomationResultCode_Success)
    {
        return pageStatus;
    }

    context->started = true;
    return MfdEditorAutomationResultCode_Success;
}

MfdEditorAutomationResultCode MFD_EDITOR_AUTOMATION_CALL TickPlugin(void* pluginContext,
                                                                    MfdEditorUtf8Buffer* error) noexcept
{
    auto* context = static_cast<TestAutomationPluginContext*>(pluginContext);
    if (context == nullptr || !context->started || context->host == nullptr)
    {
        WriteMessage(error, "The test automation plugin was not started correctly.");
        return MfdEditorAutomationResultCode_InvalidState;
    }
    if (context->tickApplied)
    {
        return MfdEditorAutomationResultCode_Success;
    }

    MfdEditorAutomationSessionHandle sessionHandle = 0U;
    MfdEditorAutomationResultCode status =
        context->host->begin_session(context->host->host_context, MakeView("test-plugin"), &sessionHandle, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    char createdPageIdStorage[128] {};
    MfdEditorAutomationCreatePageAssetRequestV1 createPage {};
    createPage.struct_size = sizeof(createPage);
    createPage.session = sessionHandle;
    createPage.name = MakeView("PluginPage");
    createPage.title = MakeView("Plugin Page");
    createPage.relative_source_path = MakeView("plugin_page.json");
    createPage.make_default = 0U;
    createPage.created_page_id = MfdEditorUtf8Buffer {createdPageIdStorage, sizeof(createdPageIdStorage), 0U};
    status = context->host->create_page_asset(context->host->host_context, &createPage, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        context->host->rollback_session(context->host->host_context, sessionHandle, nullptr);
        return status;
    }

    MfdEditorAutomationValidationSummaryV1 validation {};
    validation.struct_size = sizeof(validation);
    status = context->host->validate_session(context->host->host_context, sessionHandle, &validation, error);
    if (status != MfdEditorAutomationResultCode_Success || validation.valid == 0U)
    {
        context->host->rollback_session(context->host->host_context, sessionHandle, nullptr);
        if (status == MfdEditorAutomationResultCode_Success)
        {
            WriteMessage(error, "The test automation session should validate before commit.");
            return MfdEditorAutomationResultCode_ValidationFailed;
        }
        return status;
    }

    status = context->host->commit_session(context->host->host_context, sessionHandle, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    context->tickApplied = true;
    return MfdEditorAutomationResultCode_Success;
}

void MFD_EDITOR_AUTOMATION_CALL StopPlugin(void* pluginContext) noexcept
{
    if (auto* context = static_cast<TestAutomationPluginContext*>(pluginContext); context != nullptr)
    {
        context->host = nullptr;
        context->started = false;
    }
}

void MFD_EDITOR_AUTOMATION_CALL DestroyPlugin(void* pluginContext) noexcept
{
    delete static_cast<TestAutomationPluginContext*>(pluginContext);
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

    auto* context = new (std::nothrow) TestAutomationPluginContext();
    if (context == nullptr)
    {
        WriteMessage(error, "Unable to allocate the test automation plugin context.");
        return MfdEditorAutomationResultCode_InternalFailure;
    }

    *outApi = {};
    outApi->struct_size = sizeof(*outApi);
    outApi->info.struct_size = sizeof(outApi->info);
    outApi->info.abi_version = MFD_EDITOR_AUTOMATION_PLUGIN_ABI_VERSION;
    outApi->info.plugin_id = MakeView("test.automation.plugin");
    outApi->info.display_name = MakeView("MFD Editor Test Automation Plugin");
    outApi->plugin_context = context;
    outApi->start = &StartPlugin;
    outApi->tick = &TickPlugin;
    outApi->stop = &StopPlugin;
    outApi->destroy = &DestroyPlugin;
    return MfdEditorAutomationResultCode_Success;
}
