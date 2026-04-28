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

#include <algorithm>
#include <cstring>
#include <new>
#include <string>

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

std::string BufferToString(const MfdEditorUtf8Buffer& buffer)
{
    if (buffer.data == nullptr || buffer.capacity == 0U)
    {
        return {};
    }

    const std::size_t readable = (std::min)(buffer.size, buffer.capacity - 1U);
    return std::string(buffer.data, buffer.data + readable);
}

struct TestAutomationPluginContext
{
    const MfdEditorAutomationHostApi* host = nullptr;
    bool started = false;
    bool tickApplied = false;
    std::string firstReticleAssetId {};
};

MfdEditorAutomationResultCode MFD_EDITOR_AUTOMATION_CALL StartPlugin(void* pluginContext,
                                                                     const MfdEditorAutomationHostApi* host,
                                                                     MfdEditorUtf8Buffer* error) noexcept
{
    if (pluginContext == nullptr || host == nullptr ||
        host->get_snapshot_summary == nullptr ||
        host->get_page_info == nullptr ||
        host->get_reticle_asset_info == nullptr ||
        host->get_layer_info == nullptr ||
        host->begin_session == nullptr ||
        host->validate_session == nullptr ||
        host->get_session_validation_diagnostic == nullptr ||
        host->create_page_asset == nullptr ||
        host->instantiate_reticle_on_page == nullptr ||
        host->get_page_reticle_info == nullptr ||
        host->create_layer == nullptr ||
        host->set_page_reticle_layer == nullptr ||
        host->set_layer_visibility == nullptr ||
        host->set_page_reticle_visibility == nullptr ||
        host->set_page_reticle_draw_on_top == nullptr ||
        host->set_page_reticle_transform == nullptr ||
        host->commit_session == nullptr ||
        host->rollback_session == nullptr ||
        host->save_asset == nullptr ||
        host->consume_pending_event == nullptr)
    {
        WriteMessage(error, "The editor host did not provide the required automation callbacks.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    auto* context = static_cast<TestAutomationPluginContext*>(pluginContext);
    context->host = host;

    char windowIdStorage[128] {};
    char windowTitleStorage[128] {};
    MfdEditorAutomationSnapshotSummary summary {};
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
    MfdEditorAutomationPageInfo pageInfo {};
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

    char reticleAssetIdStorage[128] {};
    char reticleTemplateIdStorage[128] {};
    char reticleSourcePathStorage[260] {};
    MfdEditorAutomationReticleAssetInfo reticleInfo {};
    reticleInfo.struct_size = sizeof(reticleInfo);
    reticleInfo.reticle_asset_id = MfdEditorUtf8Buffer {reticleAssetIdStorage, sizeof(reticleAssetIdStorage), 0U};
    reticleInfo.template_id = MfdEditorUtf8Buffer {reticleTemplateIdStorage, sizeof(reticleTemplateIdStorage), 0U};
    reticleInfo.source_path = MfdEditorUtf8Buffer {reticleSourcePathStorage, sizeof(reticleSourcePathStorage), 0U};
    const MfdEditorAutomationResultCode reticleStatus =
        host->get_reticle_asset_info(host->host_context, 0U, &reticleInfo, error);
    if (reticleStatus != MfdEditorAutomationResultCode_Success)
    {
        return reticleStatus;
    }

    if (pageInfo.layer_count == 0U)
    {
        WriteMessage(error, "The seed editor document must expose at least one layer.");
        return MfdEditorAutomationResultCode_InvalidState;
    }

    char layerIdStorage[128] {};
    char layerNameStorage[128] {};
    MfdEditorAutomationLayerInfo layerInfo {};
    layerInfo.struct_size = sizeof(layerInfo);
    layerInfo.layer_id = MfdEditorUtf8Buffer {layerIdStorage, sizeof(layerIdStorage), 0U};
    layerInfo.layer_name = MfdEditorUtf8Buffer {layerNameStorage, sizeof(layerNameStorage), 0U};
    const MfdEditorAutomationResultCode layerStatus =
        host->get_layer_info(host->host_context, 0U, 0U, &layerInfo, error);
    if (layerStatus != MfdEditorAutomationResultCode_Success)
    {
        return layerStatus;
    }

    context->firstReticleAssetId = BufferToString(reticleInfo.reticle_asset_id);
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
    MfdEditorAutomationCreatePageAssetRequest createPage {};
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

    const std::string createdPageId = BufferToString(createPage.created_page_id);

    char createdLayerIdStorage[128] {};
    MfdEditorAutomationCreateLayerRequest createLayer {};
    createLayer.struct_size = sizeof(createLayer);
    createLayer.session = sessionHandle;
    createLayer.page_id = MfdEditorStringView {createdPageId.c_str(), createdPageId.size()};
    createLayer.layer_name = MakeView("plugin_overlay");
    createLayer.insert_index = -1;
    createLayer.visible = 0U;
    createLayer.created_layer_id = MfdEditorUtf8Buffer {createdLayerIdStorage, sizeof(createdLayerIdStorage), 0U};
    status = context->host->create_layer(context->host->host_context, &createLayer, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        context->host->rollback_session(context->host->host_context, sessionHandle, nullptr);
        return status;
    }

    const std::string createdLayerId = BufferToString(createLayer.created_layer_id);

    char createdPageReticleIdStorage[128] {};
    const MfdEditorStringView createdPageIdView {createdPageId.c_str(), createdPageId.size()};
    const MfdEditorStringView firstReticleAssetIdView {context->firstReticleAssetId.c_str(), context->firstReticleAssetId.size()};
    MfdEditorAutomationInstantiateReticleOnPageRequest instantiateReticle {};
    instantiateReticle.struct_size = sizeof(instantiateReticle);
    instantiateReticle.session = sessionHandle;
    instantiateReticle.page_id = createdPageIdView;
    instantiateReticle.reticle_asset_id = firstReticleAssetIdView;
    instantiateReticle.requested_instance_id = MakeView("plugin_track");
    instantiateReticle.transform = MfdEditorAutomationTransform2D {0.15f, -0.05f, 12.0f, 1.25f, 0.85f};
    instantiateReticle.assign_default_layer = 1U;
    instantiateReticle.created_page_reticle_id =
        MfdEditorUtf8Buffer {createdPageReticleIdStorage, sizeof(createdPageReticleIdStorage), 0U};
    status = context->host->instantiate_reticle_on_page(context->host->host_context, &instantiateReticle, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        context->host->rollback_session(context->host->host_context, sessionHandle, nullptr);
        return status;
    }

    const std::string createdPageReticleId = BufferToString(instantiateReticle.created_page_reticle_id);

    MfdEditorAutomationSetPageReticleLayerRequest assignLayer {};
    assignLayer.struct_size = sizeof(assignLayer);
    assignLayer.session = sessionHandle;
    assignLayer.page_reticle_id = MfdEditorStringView {createdPageReticleId.c_str(), createdPageReticleId.size()};
    assignLayer.layer_id = MfdEditorStringView {createdLayerId.c_str(), createdLayerId.size()};
    status = context->host->set_page_reticle_layer(context->host->host_context, &assignLayer, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        context->host->rollback_session(context->host->host_context, sessionHandle, nullptr);
        return status;
    }

    MfdEditorAutomationSetLayerVisibilityRequest showLayer {};
    showLayer.struct_size = sizeof(showLayer);
    showLayer.session = sessionHandle;
    showLayer.page_id = createdPageIdView;
    showLayer.layer_id = assignLayer.layer_id;
    showLayer.visible = 1U;
    status = context->host->set_layer_visibility(context->host->host_context, &showLayer, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        context->host->rollback_session(context->host->host_context, sessionHandle, nullptr);
        return status;
    }

    MfdEditorAutomationSetPageReticleVisibilityRequest setVisibility {};
    setVisibility.struct_size = sizeof(setVisibility);
    setVisibility.session = sessionHandle;
    setVisibility.page_reticle_id = assignLayer.page_reticle_id;
    setVisibility.visible = 0U;
    status = context->host->set_page_reticle_visibility(context->host->host_context, &setVisibility, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        context->host->rollback_session(context->host->host_context, sessionHandle, nullptr);
        return status;
    }

    MfdEditorAutomationSetPageReticleDrawOnTopRequest setDrawOnTop {};
    setDrawOnTop.struct_size = sizeof(setDrawOnTop);
    setDrawOnTop.session = sessionHandle;
    setDrawOnTop.page_reticle_id = assignLayer.page_reticle_id;
    setDrawOnTop.draw_on_top = 1U;
    status = context->host->set_page_reticle_draw_on_top(context->host->host_context, &setDrawOnTop, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        context->host->rollback_session(context->host->host_context, sessionHandle, nullptr);
        return status;
    }

    MfdEditorAutomationSetPageReticleTransformRequest setTransform {};
    setTransform.struct_size = sizeof(setTransform);
    setTransform.session = sessionHandle;
    setTransform.page_reticle_id = assignLayer.page_reticle_id;
    setTransform.transform = MfdEditorAutomationTransform2D {0.35f, -0.20f, 22.0f, 1.10f, 0.70f};
    status = context->host->set_page_reticle_transform(context->host->host_context, &setTransform, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        context->host->rollback_session(context->host->host_context, sessionHandle, nullptr);
        return status;
    }

    char pageReticleIdStorage[128] {};
    char pageReticleNameStorage[128] {};
    char sourceReticleAssetIdStorage[128] {};
    char assignedLayerIdStorage[128] {};
    MfdEditorAutomationPageReticleInfo pageReticleInfo {};
    pageReticleInfo.struct_size = sizeof(pageReticleInfo);
    pageReticleInfo.page_reticle_id = MfdEditorUtf8Buffer {pageReticleIdStorage, sizeof(pageReticleIdStorage), 0U};
    pageReticleInfo.instance_id = MfdEditorUtf8Buffer {pageReticleNameStorage, sizeof(pageReticleNameStorage), 0U};
    pageReticleInfo.source_reticle_asset_id =
        MfdEditorUtf8Buffer {sourceReticleAssetIdStorage, sizeof(sourceReticleAssetIdStorage), 0U};
    pageReticleInfo.layer_id = MfdEditorUtf8Buffer {assignedLayerIdStorage, sizeof(assignedLayerIdStorage), 0U};
    status = context->host->get_page_reticle_info(context->host->host_context, 1U, 0U, &pageReticleInfo, error);
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
            WriteMessage(error, "The test automation session should validate before commit.");
            return MfdEditorAutomationResultCode_ValidationFailed;
        }
        return status;
    }

    MfdEditorAutomationValidationDiagnostic diagnostic {};
    diagnostic.struct_size = sizeof(diagnostic);
    status = context->host->get_session_validation_diagnostic(
        context->host->host_context, sessionHandle, 0U, &diagnostic, nullptr);
    if (status != MfdEditorAutomationResultCode_NotFound)
    {
        context->host->rollback_session(context->host->host_context, sessionHandle, nullptr);
        WriteMessage(error, "A valid session should not expose validation diagnostics.");
        return MfdEditorAutomationResultCode_InternalFailure;
    }

    status = context->host->commit_session(context->host->host_context, sessionHandle, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    MfdEditorAutomationSaveAssetRequest savePage {};
    savePage.struct_size = sizeof(savePage);
    savePage.kind = MfdEditorAutomationSaveAssetKind_Page;
    savePage.entity_id = createdPageIdView;
    MfdEditorAutomationSaveSummary saveSummary {};
    saveSummary.struct_size = sizeof(saveSummary);
    status = context->host->save_asset(context->host->host_context, &savePage, &saveSummary, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }
    if (saveSummary.saved_file_count < 2U)
    {
        WriteMessage(error, "The test automation plugin expected at least two saved files for one page save.");
        return MfdEditorAutomationResultCode_InternalFailure;
    }

    std::size_t eventCount = 0U;
    for (;;)
    {
        char entityIdStorage[128] {};
        char messageStorage[256] {};
        MfdEditorAutomationEvent event {};
        event.struct_size = sizeof(event);
        event.entity_id = MfdEditorUtf8Buffer {entityIdStorage, sizeof(entityIdStorage), 0U};
        event.message = MfdEditorUtf8Buffer {messageStorage, sizeof(messageStorage), 0U};
        status = context->host->consume_pending_event(context->host->host_context, &event, nullptr);
        if (status == MfdEditorAutomationResultCode_NotFound)
        {
            break;
        }
        if (status != MfdEditorAutomationResultCode_Success)
        {
            return status;
        }
        ++eventCount;
    }

    if (eventCount == 0U)
    {
        WriteMessage(error, "The test automation plugin expected at least one emitted automation event.");
        return MfdEditorAutomationResultCode_InternalFailure;
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
MfdGetEditorAutomationPluginApi(MfdEditorAutomationPluginApi* outApi, MfdEditorUtf8Buffer* error) noexcept
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

