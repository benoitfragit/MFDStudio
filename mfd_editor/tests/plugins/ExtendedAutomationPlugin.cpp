/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Test automation plugin DLL covering the widened stable editor automation ABI surface.
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

MfdEditorStringView MakeView(const std::string& text) noexcept
{
    return MfdEditorStringView {text.c_str(), text.size()};
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

template <std::size_t N>
MfdEditorUtf8Buffer MakeBuffer(char (&storage)[N]) noexcept
{
    return MfdEditorUtf8Buffer {storage, N, 0U};
}

struct ExtendedAutomationPluginContext
{
    const MfdEditorAutomationHostApi* host = nullptr;
    bool started = false;
    bool tickApplied = false;
};

MfdEditorAutomationResultCode ReadWindowInfo(const MfdEditorAutomationHostApi& host,
                                             MfdEditorAutomationWindowInfo& outInfo,
                                             MfdEditorUtf8Buffer* error)
{
    static thread_local char windowIdStorage[128] {};
    static thread_local char titleStorage[128] {};
    static thread_local char sourcePathStorage[260] {};
    static thread_local char fontPathStorage[260] {};
    static thread_local char iconPathStorage[260] {};
    static thread_local char libraryFolderStorage[260] {};
    outInfo = {};
    outInfo.struct_size = sizeof(outInfo);
    outInfo.window_id = MakeBuffer(windowIdStorage);
    outInfo.window_title = MakeBuffer(titleStorage);
    outInfo.source_path = MakeBuffer(sourcePathStorage);
    outInfo.font_file = MakeBuffer(fontPathStorage);
    outInfo.icon_file = MakeBuffer(iconPathStorage);
    outInfo.reticle_library_folder = MakeBuffer(libraryFolderStorage);
    return host.get_window_info(host.host_context, &outInfo, error);
}

MfdEditorAutomationResultCode ReadUiState(const MfdEditorAutomationHostApi& host,
                                          MfdEditorAutomationUiState& outState,
                                          MfdEditorUtf8Buffer* error)
{
    static thread_local char activePageIdStorage[128] {};
    static thread_local char reticleAssetIdStorage[128] {};
    static thread_local char primitiveIdStorage[128] {};
    static thread_local char primitiveOwnerStorage[128] {};
    outState = {};
    outState.struct_size = sizeof(outState);
    outState.active_page_id = MakeBuffer(activePageIdStorage);
    outState.selected_reticle_asset_id = MakeBuffer(reticleAssetIdStorage);
    outState.selected_primitive_id = MakeBuffer(primitiveIdStorage);
    outState.selected_primitive_owner_id = MakeBuffer(primitiveOwnerStorage);
    return host.get_ui_state(host.host_context, &outState, error);
}

MfdEditorAutomationResultCode ReadPageInfo(const MfdEditorAutomationHostApi& host,
                                           const uint32_t pageIndex,
                                           MfdEditorAutomationPageInfo& outInfo,
                                           MfdEditorUtf8Buffer* error)
{
    static thread_local char pageIdStorage[128] {};
    static thread_local char pageNameStorage[128] {};
    static thread_local char pageTitleStorage[128] {};
    static thread_local char sourcePathStorage[260] {};
    outInfo = {};
    outInfo.struct_size = sizeof(outInfo);
    outInfo.page_id = MakeBuffer(pageIdStorage);
    outInfo.page_name = MakeBuffer(pageNameStorage);
    outInfo.page_title = MakeBuffer(pageTitleStorage);
    outInfo.source_path = MakeBuffer(sourcePathStorage);
    return host.get_page_info(host.host_context, pageIndex, &outInfo, error);
}

MfdEditorAutomationResultCode ReadLayerInfo(const MfdEditorAutomationHostApi& host,
                                            const uint32_t pageIndex,
                                            const uint32_t layerIndex,
                                            MfdEditorAutomationLayerInfo& outInfo,
                                            MfdEditorUtf8Buffer* error)
{
    static thread_local char layerIdStorage[128] {};
    static thread_local char layerNameStorage[128] {};
    outInfo = {};
    outInfo.struct_size = sizeof(outInfo);
    outInfo.layer_id = MakeBuffer(layerIdStorage);
    outInfo.layer_name = MakeBuffer(layerNameStorage);
    return host.get_layer_info(host.host_context, pageIndex, layerIndex, &outInfo, error);
}

MfdEditorAutomationResultCode ReadReticleAssetInfo(const MfdEditorAutomationHostApi& host,
                                                   const uint32_t reticleIndex,
                                                   MfdEditorAutomationReticleAssetInfo& outInfo,
                                                   MfdEditorUtf8Buffer* error)
{
    static thread_local char assetIdStorage[128] {};
    static thread_local char templateIdStorage[128] {};
    static thread_local char labelStorage[128] {};
    static thread_local char categoryStorage[128] {};
    static thread_local char clippingPrimitiveStorage[128] {};
    static thread_local char sourcePathStorage[260] {};
    outInfo = {};
    outInfo.struct_size = sizeof(outInfo);
    outInfo.reticle_asset_id = MakeBuffer(assetIdStorage);
    outInfo.template_id = MakeBuffer(templateIdStorage);
    outInfo.label = MakeBuffer(labelStorage);
    outInfo.category = MakeBuffer(categoryStorage);
    outInfo.clipping_primitive_id = MakeBuffer(clippingPrimitiveStorage);
    outInfo.source_path = MakeBuffer(sourcePathStorage);
    return host.get_reticle_asset_info(host.host_context, reticleIndex, &outInfo, error);
}

MfdEditorAutomationResultCode ReadPageReticleInfo(const MfdEditorAutomationHostApi& host,
                                                  const uint32_t pageIndex,
                                                  const uint32_t reticleIndex,
                                                  MfdEditorAutomationPageReticleInfo& outInfo,
                                                  MfdEditorUtf8Buffer* error)
{
    static thread_local char pageReticleIdStorage[128] {};
    static thread_local char instanceIdStorage[128] {};
    static thread_local char sourceAssetIdStorage[128] {};
    static thread_local char layerIdStorage[128] {};
    static thread_local char blinkTypeStorage[128] {};
    static thread_local char clippingPrimitiveStorage[128] {};
    outInfo = {};
    outInfo.struct_size = sizeof(outInfo);
    outInfo.page_reticle_id = MakeBuffer(pageReticleIdStorage);
    outInfo.instance_id = MakeBuffer(instanceIdStorage);
    outInfo.source_reticle_asset_id = MakeBuffer(sourceAssetIdStorage);
    outInfo.layer_id = MakeBuffer(layerIdStorage);
    outInfo.blink_type_name = MakeBuffer(blinkTypeStorage);
    outInfo.clipping_primitive_id = MakeBuffer(clippingPrimitiveStorage);
    return host.get_page_reticle_info(host.host_context, pageIndex, reticleIndex, &outInfo, error);
}

MfdEditorAutomationResultCode ReadPrimitiveInfo(const MfdEditorAutomationHostApi& host,
                                                const bool reticleAssetPrimitive,
                                                const uint32_t pageOrReticleIndex,
                                                const uint32_t reticleOrPrimitiveIndex,
                                                const uint32_t primitiveIndex,
                                                MfdEditorAutomationPrimitiveInfo& outInfo,
                                                MfdEditorUtf8Buffer* error)
{
    static thread_local char primitiveIdStorage[128] {};
    static thread_local char ownerIdStorage[128] {};
    static thread_local char contentStorage[260] {};
    outInfo = {};
    outInfo.struct_size = sizeof(outInfo);
    outInfo.primitive_id = MakeBuffer(primitiveIdStorage);
    outInfo.owner_id = MakeBuffer(ownerIdStorage);
    outInfo.content = MakeBuffer(contentStorage);
    return reticleAssetPrimitive
               ? host.get_reticle_asset_primitive_info(host.host_context,
                                                       pageOrReticleIndex,
                                                       reticleOrPrimitiveIndex,
                                                       &outInfo,
                                                       error)
               : host.get_page_reticle_primitive_info(host.host_context,
                                                      pageOrReticleIndex,
                                                      reticleOrPrimitiveIndex,
                                                      primitiveIndex,
                                                      &outInfo,
                                                      error);
}

MfdEditorAutomationResultCode ReadBlinkTypeInfo(const MfdEditorAutomationHostApi& host,
                                                const uint32_t pageIndex,
                                                const uint32_t blinkIndex,
                                                MfdEditorAutomationPageBlinkTypeInfo& outInfo,
                                                MfdEditorUtf8Buffer* error)
{
    static thread_local char blinkTypeStorage[128] {};
    outInfo = {};
    outInfo.struct_size = sizeof(outInfo);
    outInfo.blink_type_name = MakeBuffer(blinkTypeStorage);
    return host.get_page_blink_type_info(host.host_context, pageIndex, blinkIndex, &outInfo, error);
}

MfdEditorAutomationResultCode ExportJsonPreview(const MfdEditorAutomationHostApi& host,
                                                const uint32_t kind,
                                                const std::string& entityId,
                                                MfdEditorUtf8Buffer* error)
{
    char sourcePathStorage[260] {};
    char jsonStorage[8192] {};
    MfdEditorAutomationExportJsonPreviewRequest request {};
    request.struct_size = sizeof(request);
    request.kind = kind;
    request.entity_id = MakeView(entityId);

    MfdEditorAutomationJsonPreviewResult preview {};
    preview.struct_size = sizeof(preview);
    preview.source_path = MakeBuffer(sourcePathStorage);
    preview.json = MakeBuffer(jsonStorage);
    const MfdEditorAutomationResultCode status =
        host.export_json_preview(host.host_context, &request, &preview, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    if (BufferToString(preview.json).empty())
    {
        WriteMessage(error, "The exported JSON preview should not be empty.");
        return MfdEditorAutomationResultCode_InternalFailure;
    }

    return MfdEditorAutomationResultCode_Success;
}

MfdEditorAutomationResultCode FindPageIndexById(const MfdEditorAutomationHostApi& host,
                                                const std::string& pageId,
                                                uint32_t& outPageIndex,
                                                MfdEditorUtf8Buffer* error)
{
    MfdEditorAutomationWindowInfo windowInfo {};
    MfdEditorAutomationResultCode status = ReadWindowInfo(host, windowInfo, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    for (uint32_t pageIndex = 0U; pageIndex < windowInfo.page_count; ++pageIndex)
    {
        MfdEditorAutomationPageInfo pageInfo {};
        status = ReadPageInfo(host, pageIndex, pageInfo, error);
        if (status != MfdEditorAutomationResultCode_Success)
        {
            return status;
        }

        if (BufferToString(pageInfo.page_id) == pageId)
        {
            outPageIndex = pageIndex;
            return MfdEditorAutomationResultCode_Success;
        }
    }

    WriteMessage(error, "Unable to resolve the requested page id.");
    return MfdEditorAutomationResultCode_NotFound;
}

MfdEditorAutomationResultCode FindReticleAssetIndexById(const MfdEditorAutomationHostApi& host,
                                                        const std::string& assetId,
                                                        uint32_t& outReticleIndex,
                                                        MfdEditorUtf8Buffer* error)
{
    MfdEditorAutomationWindowInfo windowInfo {};
    MfdEditorAutomationResultCode status = ReadWindowInfo(host, windowInfo, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    for (uint32_t reticleIndex = 0U; reticleIndex < windowInfo.reticle_asset_count; ++reticleIndex)
    {
        MfdEditorAutomationReticleAssetInfo reticleInfo {};
        status = ReadReticleAssetInfo(host, reticleIndex, reticleInfo, error);
        if (status != MfdEditorAutomationResultCode_Success)
        {
            return status;
        }

        if (BufferToString(reticleInfo.reticle_asset_id) == assetId)
        {
            outReticleIndex = reticleIndex;
            return MfdEditorAutomationResultCode_Success;
        }
    }

    WriteMessage(error, "Unable to resolve the requested reticle asset id.");
    return MfdEditorAutomationResultCode_NotFound;
}

MfdEditorAutomationResultCode FindLayerIdByName(const MfdEditorAutomationHostApi& host,
                                                const uint32_t pageIndex,
                                                const std::string& layerName,
                                                std::string& outLayerId,
                                                MfdEditorUtf8Buffer* error)
{
    MfdEditorAutomationPageInfo pageInfo {};
    MfdEditorAutomationResultCode status = ReadPageInfo(host, pageIndex, pageInfo, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    for (uint32_t layerIndex = 0U; layerIndex < pageInfo.layer_count; ++layerIndex)
    {
        MfdEditorAutomationLayerInfo layerInfo {};
        status = ReadLayerInfo(host, pageIndex, layerIndex, layerInfo, error);
        if (status != MfdEditorAutomationResultCode_Success)
        {
            return status;
        }

        if (BufferToString(layerInfo.layer_name) == layerName)
        {
            outLayerId = BufferToString(layerInfo.layer_id);
            return MfdEditorAutomationResultCode_Success;
        }
    }

    WriteMessage(error, "Unable to resolve the requested layer name.");
    return MfdEditorAutomationResultCode_NotFound;
}

MfdEditorAutomationResultCode MFD_EDITOR_AUTOMATION_CALL StartPlugin(void* pluginContext,
                                                                     const MfdEditorAutomationHostApi* host,
                                                                     MfdEditorUtf8Buffer* error) noexcept
{
    if (pluginContext == nullptr || host == nullptr ||
        host->begin_session == nullptr ||
        host->validate_session == nullptr ||
        host->commit_session == nullptr ||
        host->create_page_asset == nullptr ||
        host->delete_page_asset == nullptr ||
        host->create_reticle_asset == nullptr ||
        host->delete_reticle_asset == nullptr ||
        host->instantiate_reticle_on_page == nullptr ||
        host->delete_page_reticle == nullptr ||
        host->create_layer == nullptr ||
        host->replace_layer == nullptr ||
        host->delete_layer == nullptr ||
        host->set_layer_visibility == nullptr ||
        host->set_page_reticle_layer == nullptr ||
        host->set_reticle_asset_visibility == nullptr ||
        host->set_reticle_asset_draw_on_top == nullptr ||
        host->set_reticle_transform == nullptr ||
        host->set_reticle_clipping == nullptr ||
        host->upsert_page_blink_type == nullptr ||
        host->delete_page_blink_type == nullptr ||
        host->set_page_default_blink_type == nullptr ||
        host->set_page_reticle_blink == nullptr ||
        host->assign_page_strobe_template == nullptr ||
        host->delete_page_strobe == nullptr ||
        host->select_entity == nullptr ||
        host->get_window_info == nullptr ||
        host->get_ui_state == nullptr ||
        host->get_page_blink_type_info == nullptr ||
        host->get_reticle_asset_primitive_info == nullptr ||
        host->get_page_reticle_primitive_info == nullptr ||
        host->export_json_preview == nullptr)
    {
        WriteMessage(error, "The editor host did not expose the extended automation callbacks.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    auto* context = static_cast<ExtendedAutomationPluginContext*>(pluginContext);
    context->host = host;
    context->started = true;
    return MfdEditorAutomationResultCode_Success;
}

MfdEditorAutomationResultCode MFD_EDITOR_AUTOMATION_CALL TickPlugin(void* pluginContext,
                                                                    MfdEditorUtf8Buffer* error) noexcept
{
    auto* context = static_cast<ExtendedAutomationPluginContext*>(pluginContext);
    if (context == nullptr || !context->started || context->host == nullptr)
    {
        WriteMessage(error, "The extended automation plugin was not started correctly.");
        return MfdEditorAutomationResultCode_InvalidState;
    }
    if (context->tickApplied)
    {
        return MfdEditorAutomationResultCode_Success;
    }

    const MfdEditorAutomationHostApi& host = *context->host;

    MfdEditorAutomationWindowInfo baselineWindowInfo {};
    MfdEditorAutomationResultCode status = ReadWindowInfo(host, baselineWindowInfo, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }
    if (baselineWindowInfo.page_count == 0U || baselineWindowInfo.reticle_asset_count == 0U)
    {
        WriteMessage(error, "The seed document must expose at least one page and one reticle asset.");
        return MfdEditorAutomationResultCode_InvalidState;
    }

    MfdEditorAutomationUiState baselineUiState {};
    status = ReadUiState(host, baselineUiState, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }
    if (baselineUiState.selection_kind != MfdEditorAutomationSelectionKind_Page)
    {
        WriteMessage(error, "The seed editor bridge should start with one page selection.");
        return MfdEditorAutomationResultCode_InvalidState;
    }

    MfdEditorAutomationSessionHandle sessionHandle = 0U;
    status = host.begin_session(host.host_context, MakeView("extended-surface"), &sessionHandle, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    char createdPageIdStorage[128] {};
    MfdEditorAutomationCreatePageAssetRequest createPage {};
    createPage.struct_size = sizeof(createPage);
    createPage.session = sessionHandle;
    createPage.name = MakeView("ExtendedPluginPage");
    createPage.title = MakeView("Extended Plugin Page");
    createPage.relative_source_path = MakeView("extended_plugin_page.json");
    createPage.created_page_id = MakeBuffer(createdPageIdStorage);
    status = host.create_page_asset(host.host_context, &createPage, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        host.rollback_session(host.host_context, sessionHandle, nullptr);
        return status;
    }
    const std::string createdPageId = BufferToString(createPage.created_page_id);

    uint32_t createdPageIndex = 0U;
    status = FindPageIndexById(host, createdPageId, createdPageIndex, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        host.rollback_session(host.host_context, sessionHandle, nullptr);
        return status;
    }

    char createdLayerIdStorage[128] {};
    MfdEditorAutomationCreateLayerRequest createLayer {};
    createLayer.struct_size = sizeof(createLayer);
    createLayer.session = sessionHandle;
    createLayer.page_id = MakeView(createdPageId);
    createLayer.layer_name = MakeView("extended_layer");
    createLayer.insert_index = -1;
    createLayer.visible = 1U;
    createLayer.created_layer_id = MakeBuffer(createdLayerIdStorage);
    status = host.create_layer(host.host_context, &createLayer, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        host.rollback_session(host.host_context, sessionHandle, nullptr);
        return status;
    }
    const std::string createdLayerId = BufferToString(createLayer.created_layer_id);

    MfdEditorAutomationSetLayerVisibilityRequest verifyCreatedLayerVisibility {};
    verifyCreatedLayerVisibility.struct_size = sizeof(verifyCreatedLayerVisibility);
    verifyCreatedLayerVisibility.session = sessionHandle;
    verifyCreatedLayerVisibility.page_id = MakeView(createdPageId);
    verifyCreatedLayerVisibility.layer_id = MakeView(createdLayerId);
    verifyCreatedLayerVisibility.visible = 1U;
    status = host.set_layer_visibility(host.host_context, &verifyCreatedLayerVisibility, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        const std::string message =
            "set_layer_visibility on created layer failed for layer_id='" + createdLayerId + "'";
        WriteMessage(error, message.c_str());
        host.rollback_session(host.host_context, sessionHandle, nullptr);
        return status;
    }

    MfdEditorAutomationReplaceLayerRequest replaceLayer {};
    replaceLayer.struct_size = sizeof(replaceLayer);
    replaceLayer.session = sessionHandle;
    replaceLayer.page_id = MakeView(createdPageId);
    replaceLayer.layer_id = MakeView(createdLayerId);
    replaceLayer.layer_name = MakeView("extended_overlay");
    replaceLayer.visible = 0U;
    status = host.replace_layer(host.host_context, &replaceLayer, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        const std::string message =
            "replace_layer failed for layer_id='" + createdLayerId + "'";
        WriteMessage(error, message.c_str());
        host.rollback_session(host.host_context, sessionHandle, nullptr);
        return status;
    }

    std::string updatedLayerId;
    status = FindLayerIdByName(host, createdPageIndex, "extended_overlay", updatedLayerId, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        host.rollback_session(host.host_context, sessionHandle, nullptr);
        return status;
    }

    MfdEditorAutomationSetLayerVisibilityRequest setLayerVisibility {};
    setLayerVisibility.struct_size = sizeof(setLayerVisibility);
    setLayerVisibility.session = sessionHandle;
    setLayerVisibility.page_id = MakeView(createdPageId);
    setLayerVisibility.layer_id = MakeView(updatedLayerId);
    setLayerVisibility.visible = 1U;
    status = host.set_layer_visibility(host.host_context, &setLayerVisibility, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        const std::string message =
            "set_layer_visibility failed for layer_id='" + updatedLayerId + "'";
        WriteMessage(error, message.c_str());
        host.rollback_session(host.host_context, sessionHandle, nullptr);
        return status;
    }

    char createdAssetIdStorage[128] {};
    MfdEditorAutomationCreateReticleAssetRequest createAsset {};
    createAsset.struct_size = sizeof(createAsset);
    createAsset.session = sessionHandle;
    createAsset.template_id = MakeView("extended_asset");
    createAsset.relative_source_path = MakeView("extended_asset.json");
    createAsset.label = MakeView("Extended Asset");
    createAsset.category = MakeView("Plugin");
    createAsset.seed_primitive_type = MfdEditorAutomationPrimitiveType_Rectangle;
    createAsset.visible = 1U;
    createAsset.draw_on_top = 0U;
    createAsset.transform = MfdEditorAutomationTransform2D {0.0f, 0.0f, 0.0f, 1.0f, 1.0f};
    createAsset.created_reticle_asset_id = MakeBuffer(createdAssetIdStorage);
    status = host.create_reticle_asset(host.host_context, &createAsset, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        host.rollback_session(host.host_context, sessionHandle, nullptr);
        return status;
    }
    const std::string createdAssetId = BufferToString(createAsset.created_reticle_asset_id);

    uint32_t createdAssetIndex = 0U;
    status = FindReticleAssetIndexById(host, createdAssetId, createdAssetIndex, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        host.rollback_session(host.host_context, sessionHandle, nullptr);
        return status;
    }

    MfdEditorAutomationSetReticleAssetVisibilityRequest setAssetVisibility {};
    setAssetVisibility.struct_size = sizeof(setAssetVisibility);
    setAssetVisibility.session = sessionHandle;
    setAssetVisibility.reticle_asset_id = MakeView(createdAssetId);
    setAssetVisibility.visible = 0U;
    status = host.set_reticle_asset_visibility(host.host_context, &setAssetVisibility, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        host.rollback_session(host.host_context, sessionHandle, nullptr);
        return status;
    }

    MfdEditorAutomationSetReticleAssetDrawOnTopRequest setAssetDrawOnTop {};
    setAssetDrawOnTop.struct_size = sizeof(setAssetDrawOnTop);
    setAssetDrawOnTop.session = sessionHandle;
    setAssetDrawOnTop.reticle_asset_id = MakeView(createdAssetId);
    setAssetDrawOnTop.draw_on_top = 1U;
    status = host.set_reticle_asset_draw_on_top(host.host_context, &setAssetDrawOnTop, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        host.rollback_session(host.host_context, sessionHandle, nullptr);
        return status;
    }

    MfdEditorAutomationSetReticleTransformRequest setAssetTransform {};
    setAssetTransform.struct_size = sizeof(setAssetTransform);
    setAssetTransform.session = sessionHandle;
    setAssetTransform.target_kind = MfdEditorAutomationReticleTargetKind_ReticleAsset;
    setAssetTransform.target_id = MakeView(createdAssetId);
    setAssetTransform.transform = MfdEditorAutomationTransform2D {-0.10f, 0.08f, 11.0f, 1.15f, 0.90f};
    status = host.set_reticle_transform(host.host_context, &setAssetTransform, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        host.rollback_session(host.host_context, sessionHandle, nullptr);
        return status;
    }

    MfdEditorAutomationReticleAssetInfo createdAssetInfo {};
    status = ReadReticleAssetInfo(host, createdAssetIndex, createdAssetInfo, error);
    if (status != MfdEditorAutomationResultCode_Success ||
        createdAssetInfo.visible != 0U ||
        createdAssetInfo.draw_on_top != 1U)
    {
        host.rollback_session(host.host_context, sessionHandle, nullptr);
        if (status == MfdEditorAutomationResultCode_Success)
        {
            WriteMessage(error, "The created reticle asset did not reflect the expected visibility and draw ordering.");
            return MfdEditorAutomationResultCode_InternalFailure;
        }
        return status;
    }

    MfdEditorAutomationPrimitiveInfo assetPrimitiveInfo {};
    status = ReadPrimitiveInfo(host, true, createdAssetIndex, 0U, 0U, assetPrimitiveInfo, error);
    if (status != MfdEditorAutomationResultCode_Success ||
        assetPrimitiveInfo.primitive_type != MfdEditorAutomationPrimitiveType_Rectangle)
    {
        host.rollback_session(host.host_context, sessionHandle, nullptr);
        if (status == MfdEditorAutomationResultCode_Success)
        {
            WriteMessage(error, "The created reticle asset should expose one rectangle primitive.");
            return MfdEditorAutomationResultCode_InternalFailure;
        }
        return status;
    }

    char createdPageReticleIdStorage[128] {};
    MfdEditorAutomationInstantiateReticleOnPageRequest instantiateReticle {};
    instantiateReticle.struct_size = sizeof(instantiateReticle);
    instantiateReticle.session = sessionHandle;
    instantiateReticle.page_id = MakeView(createdPageId);
    instantiateReticle.reticle_asset_id = MakeView(createdAssetId);
    instantiateReticle.requested_instance_id = MakeView("extended_instance");
    instantiateReticle.transform = MfdEditorAutomationTransform2D {0.12f, -0.03f, 4.0f, 1.0f, 1.0f};
    instantiateReticle.assign_default_layer = 1U;
    instantiateReticle.created_page_reticle_id = MakeBuffer(createdPageReticleIdStorage);
    status = host.instantiate_reticle_on_page(host.host_context, &instantiateReticle, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        host.rollback_session(host.host_context, sessionHandle, nullptr);
        return status;
    }
    const std::string createdPageReticleId = BufferToString(instantiateReticle.created_page_reticle_id);

    MfdEditorAutomationPageReticleInfo pageReticleInfo {};
    status = ReadPageReticleInfo(host, createdPageIndex, 0U, pageReticleInfo, error);
    if (status != MfdEditorAutomationResultCode_Success ||
        BufferToString(pageReticleInfo.page_reticle_id) != createdPageReticleId)
    {
        host.rollback_session(host.host_context, sessionHandle, nullptr);
        if (status == MfdEditorAutomationResultCode_Success)
        {
            WriteMessage(error, "The created page reticle could not be resolved from the page query surface.");
            return MfdEditorAutomationResultCode_InternalFailure;
        }
        return status;
    }

    MfdEditorAutomationPrimitiveInfo pagePrimitiveInfo {};
    status = ReadPrimitiveInfo(host, false, createdPageIndex, 0U, 0U, pagePrimitiveInfo, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        host.rollback_session(host.host_context, sessionHandle, nullptr);
        return status;
    }

    MfdEditorAutomationUpsertPageBlinkTypeRequest upsertBlinkType {};
    upsertBlinkType.struct_size = sizeof(upsertBlinkType);
    upsertBlinkType.session = sessionHandle;
    upsertBlinkType.page_id = MakeView(createdPageId);
    upsertBlinkType.blink_type_name = MakeView("pulse");
    upsertBlinkType.duration_ms = 250U;
    upsertBlinkType.make_default = 1U;
    status = host.upsert_page_blink_type(host.host_context, &upsertBlinkType, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        host.rollback_session(host.host_context, sessionHandle, nullptr);
        return status;
    }

    MfdEditorAutomationSetPageDefaultBlinkTypeRequest setDefaultBlinkType {};
    setDefaultBlinkType.struct_size = sizeof(setDefaultBlinkType);
    setDefaultBlinkType.session = sessionHandle;
    setDefaultBlinkType.page_id = MakeView(createdPageId);
    setDefaultBlinkType.blink_type_name = MakeView("pulse");
    status = host.set_page_default_blink_type(host.host_context, &setDefaultBlinkType, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        host.rollback_session(host.host_context, sessionHandle, nullptr);
        return status;
    }

    MfdEditorAutomationPageBlinkTypeInfo blinkTypeInfo {};
    status = ReadBlinkTypeInfo(host, createdPageIndex, 0U, blinkTypeInfo, error);
    if (status != MfdEditorAutomationResultCode_Success ||
        blinkTypeInfo.is_default == 0U ||
        BufferToString(blinkTypeInfo.blink_type_name) != "pulse")
    {
        host.rollback_session(host.host_context, sessionHandle, nullptr);
        if (status == MfdEditorAutomationResultCode_Success)
        {
            WriteMessage(error, "The page blink-type query surface did not reflect the expected default entry.");
            return MfdEditorAutomationResultCode_InternalFailure;
        }
        return status;
    }

    MfdEditorAutomationSetPageReticleBlinkRequest setPageReticleBlink {};
    setPageReticleBlink.struct_size = sizeof(setPageReticleBlink);
    setPageReticleBlink.session = sessionHandle;
    setPageReticleBlink.page_reticle_id = MakeView(createdPageReticleId);
    setPageReticleBlink.blink_type_name = MakeView("pulse");
    setPageReticleBlink.enabled = 1U;
    status = host.set_page_reticle_blink(host.host_context, &setPageReticleBlink, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        host.rollback_session(host.host_context, sessionHandle, nullptr);
        return status;
    }

    MfdEditorAutomationSetReticleTransformRequest setPageReticleTransform {};
    setPageReticleTransform.struct_size = sizeof(setPageReticleTransform);
    setPageReticleTransform.session = sessionHandle;
    setPageReticleTransform.target_kind = MfdEditorAutomationReticleTargetKind_PageReticleInstance;
    setPageReticleTransform.target_id = MakeView(createdPageReticleId);
    setPageReticleTransform.transform = MfdEditorAutomationTransform2D {0.21f, -0.09f, 22.0f, 1.35f, 0.85f};
    status = host.set_reticle_transform(host.host_context, &setPageReticleTransform, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        host.rollback_session(host.host_context, sessionHandle, nullptr);
        return status;
    }

    const std::string pagePrimitiveId = BufferToString(pagePrimitiveInfo.primitive_id);
    MfdEditorAutomationSetReticleClippingRequest setPageReticleClipping {};
    setPageReticleClipping.struct_size = sizeof(setPageReticleClipping);
    setPageReticleClipping.session = sessionHandle;
    setPageReticleClipping.target_kind = MfdEditorAutomationReticleTargetKind_PageReticleInstance;
    setPageReticleClipping.clipping_mode = MfdEditorAutomationReticleClipMode_Inner;
    setPageReticleClipping.target_id = MakeView(createdPageReticleId);
    setPageReticleClipping.primitive_id = MakeView(pagePrimitiveId);
    status = host.set_reticle_clipping(host.host_context, &setPageReticleClipping, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        host.rollback_session(host.host_context, sessionHandle, nullptr);
        return status;
    }

    MfdEditorAutomationSetPageReticleLayerRequest setPageReticleLayer {};
    setPageReticleLayer.struct_size = sizeof(setPageReticleLayer);
    setPageReticleLayer.session = sessionHandle;
    setPageReticleLayer.page_reticle_id = MakeView(createdPageReticleId);
    setPageReticleLayer.layer_id = MakeView(updatedLayerId);
    status = host.set_page_reticle_layer(host.host_context, &setPageReticleLayer, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        const std::string message =
            "set_page_reticle_layer failed for page_reticle_id='" + createdPageReticleId +
            "' layer_id='" + updatedLayerId + "'";
        WriteMessage(error, message.c_str());
        host.rollback_session(host.host_context, sessionHandle, nullptr);
        return status;
    }

    MfdEditorAutomationAssignPageStrobeTemplateRequest assignStrobeTemplate {};
    assignStrobeTemplate.struct_size = sizeof(assignStrobeTemplate);
    assignStrobeTemplate.session = sessionHandle;
    assignStrobeTemplate.page_id = MakeView(createdPageId);
    assignStrobeTemplate.reticle_asset_id = MakeView(createdAssetId);
    assignStrobeTemplate.requested_strobe_id = MakeView("extended_strobe");
    status = host.assign_page_strobe_template(host.host_context, &assignStrobeTemplate, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        host.rollback_session(host.host_context, sessionHandle, nullptr);
        return status;
    }

    MfdEditorAutomationSelectEntityRequest selectEntity {};
    selectEntity.struct_size = sizeof(selectEntity);
    selectEntity.session = sessionHandle;
    selectEntity.selection_kind = MfdEditorAutomationSelectionKind_PageReticleInstance;
    selectEntity.target_id = MakeView(createdPageReticleId);
    status = host.select_entity(host.host_context, &selectEntity, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        host.rollback_session(host.host_context, sessionHandle, nullptr);
        return status;
    }

    MfdEditorAutomationUiState selectedUiState {};
    status = ReadUiState(host, selectedUiState, error);
    if (status != MfdEditorAutomationResultCode_Success ||
        selectedUiState.selection_kind != MfdEditorAutomationSelectionKind_PageReticleInstance ||
        selectedUiState.selected_page_reticle_count == 0U)
    {
        host.rollback_session(host.host_context, sessionHandle, nullptr);
        if (status == MfdEditorAutomationResultCode_Success)
        {
            WriteMessage(error, "The automation UI state did not reflect the requested page-reticle selection.");
            return MfdEditorAutomationResultCode_InternalFailure;
        }
        return status;
    }

    status = ExportJsonPreview(host,
                               MfdEditorAutomationExportJsonPreviewKind_Page,
                               createdPageId,
                               error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        host.rollback_session(host.host_context, sessionHandle, nullptr);
        return status;
    }

    status = ExportJsonPreview(host,
                               MfdEditorAutomationExportJsonPreviewKind_ReticleAsset,
                               createdAssetId,
                               error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        host.rollback_session(host.host_context, sessionHandle, nullptr);
        return status;
    }

    status = ExportJsonPreview(host,
                               MfdEditorAutomationExportJsonPreviewKind_PageReticleInstance,
                               createdPageReticleId,
                               error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        host.rollback_session(host.host_context, sessionHandle, nullptr);
        return status;
    }

    MfdEditorAutomationValidationSummary validation {};
    validation.struct_size = sizeof(validation);
    status = host.validate_session(host.host_context, sessionHandle, &validation, error);
    if (status != MfdEditorAutomationResultCode_Success || validation.valid == 0U)
    {
        host.rollback_session(host.host_context, sessionHandle, nullptr);
        if (status == MfdEditorAutomationResultCode_Success)
        {
            WriteMessage(error, "The extended automation session should validate successfully.");
            return MfdEditorAutomationResultCode_ValidationFailed;
        }
        return status;
    }

    MfdEditorAutomationDeletePageStrobeRequest deletePageStrobe {};
    deletePageStrobe.struct_size = sizeof(deletePageStrobe);
    deletePageStrobe.session = sessionHandle;
    deletePageStrobe.page_id = MakeView(createdPageId);
    status = host.delete_page_strobe(host.host_context, &deletePageStrobe, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        host.rollback_session(host.host_context, sessionHandle, nullptr);
        return status;
    }

    MfdEditorAutomationDeletePageReticleRequest deletePageReticle {};
    deletePageReticle.struct_size = sizeof(deletePageReticle);
    deletePageReticle.session = sessionHandle;
    deletePageReticle.page_reticle_id = MakeView(createdPageReticleId);
    status = host.delete_page_reticle(host.host_context, &deletePageReticle, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        host.rollback_session(host.host_context, sessionHandle, nullptr);
        return status;
    }

    MfdEditorAutomationDeletePageBlinkTypeRequest deleteBlinkType {};
    deleteBlinkType.struct_size = sizeof(deleteBlinkType);
    deleteBlinkType.session = sessionHandle;
    deleteBlinkType.page_id = MakeView(createdPageId);
    deleteBlinkType.blink_type_name = MakeView("pulse");
    status = host.delete_page_blink_type(host.host_context, &deleteBlinkType, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        host.rollback_session(host.host_context, sessionHandle, nullptr);
        return status;
    }

    MfdEditorAutomationDeleteReticleAssetRequest deleteReticleAsset {};
    deleteReticleAsset.struct_size = sizeof(deleteReticleAsset);
    deleteReticleAsset.session = sessionHandle;
    deleteReticleAsset.reticle_asset_id = MakeView(createdAssetId);
    status = host.delete_reticle_asset(host.host_context, &deleteReticleAsset, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        host.rollback_session(host.host_context, sessionHandle, nullptr);
        return status;
    }

    MfdEditorAutomationDeleteLayerRequest deleteLayer {};
    deleteLayer.struct_size = sizeof(deleteLayer);
    deleteLayer.session = sessionHandle;
    deleteLayer.page_id = MakeView(createdPageId);
    deleteLayer.layer_id = MakeView(updatedLayerId);
    status = host.delete_layer(host.host_context, &deleteLayer, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        const std::string message =
            "delete_layer failed for layer_id='" + updatedLayerId + "'";
        WriteMessage(error, message.c_str());
        host.rollback_session(host.host_context, sessionHandle, nullptr);
        return status;
    }

    MfdEditorAutomationDeletePageAssetRequest deletePage {};
    deletePage.struct_size = sizeof(deletePage);
    deletePage.session = sessionHandle;
    deletePage.page_id = MakeView(createdPageId);
    status = host.delete_page_asset(host.host_context, &deletePage, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        host.rollback_session(host.host_context, sessionHandle, nullptr);
        return status;
    }

    status = host.commit_session(host.host_context, sessionHandle, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    context->tickApplied = true;
    return MfdEditorAutomationResultCode_Success;
}

void MFD_EDITOR_AUTOMATION_CALL StopPlugin(void* pluginContext) noexcept
{
    if (auto* context = static_cast<ExtendedAutomationPluginContext*>(pluginContext); context != nullptr)
    {
        context->host = nullptr;
        context->started = false;
    }
}

void MFD_EDITOR_AUTOMATION_CALL DestroyPlugin(void* pluginContext) noexcept
{
    delete static_cast<ExtendedAutomationPluginContext*>(pluginContext);
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

    auto* context = new (std::nothrow) ExtendedAutomationPluginContext();
    if (context == nullptr)
    {
        WriteMessage(error, "Unable to allocate the extended automation plugin context.");
        return MfdEditorAutomationResultCode_InternalFailure;
    }

    *outApi = {};
    outApi->struct_size = sizeof(*outApi);
    outApi->info.struct_size = sizeof(outApi->info);
    outApi->info.abi_version = MFD_EDITOR_AUTOMATION_PLUGIN_ABI_VERSION;
    outApi->info.plugin_id = MakeView("test.extended.automation.plugin");
    outApi->info.display_name = MakeView("MFD Editor Extended Automation Plugin");
    outApi->plugin_context = context;
    outApi->start = &StartPlugin;
    outApi->tick = &TickPlugin;
    outApi->stop = &StopPlugin;
    outApi->destroy = &DestroyPlugin;
    return MfdEditorAutomationResultCode_Success;
}
