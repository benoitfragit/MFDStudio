/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Fully-commented sample automation plugin covering one readable end-to-end editor automation workflow.
 *
 * @details The sample focuses on one pedagogical workflow across the stable
 * `MfdEditorAutomationHostApi`. Exhaustive callback coverage is maintained in
 * the loader test plugin located in `tests/mfd_editor/plugins/ExtendedAutomationPlugin.cpp`.
 *
 * The workflow is split into two parts:
 *
 * 1. one mutation-heavy preview session that is rolled back, so every semantic
 *    edit callback can be illustrated without leaving authored content behind
 * 2. one lightweight committed session plus host-owned save calls to illustrate
 *    persistence and event consumption
 */

#include "mfd/editor/AutomationPlugin.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <new>
#include <string>

namespace
{
constexpr std::size_t kSmallTextCapacity = 128U;
constexpr std::size_t kPathTextCapacity = 260U;
constexpr std::size_t kEventTextCapacity = 256U;

MfdEditorStringView MakeView(const char* text) noexcept
{
    return MfdEditorStringView {text, text == nullptr ? 0U : std::strlen(text)};
}

MfdEditorStringView MakeView(const std::string& text) noexcept
{
    return MfdEditorStringView {text.data(), text.size()};
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

    const std::size_t writable = (std::min)(length, buffer->capacity - 1U);
    if (writable > 0U && text != nullptr)
    {
        std::memcpy(buffer->data, text, writable);
    }

    buffer->data[writable] = '\0';
    return writable == length ? MfdEditorAutomationResultCode_Success : MfdEditorAutomationResultCode_BufferTooSmall;
}

template <std::size_t N>
MfdEditorUtf8Buffer MakeBuffer(std::array<char, N>& storage) noexcept
{
    return MfdEditorUtf8Buffer {storage.data(), storage.size(), 0U};
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

/**
 * @brief Owns the caller-side UTF-8 storage required by `get_snapshot_summary`.
 */
struct SnapshotSummaryOutput
{
    SnapshotSummaryOutput()
    {
        value.struct_size = sizeof(value);
        value.window_id = MakeBuffer(windowIdStorage);
        value.window_title = MakeBuffer(windowTitleStorage);
    }

    [[nodiscard]] std::string WindowId() const
    {
        return BufferToString(value.window_id);
    }

    MfdEditorAutomationSnapshotSummary value {};
    std::array<char, kSmallTextCapacity> windowIdStorage {};
    std::array<char, kSmallTextCapacity> windowTitleStorage {};
};

/**
 * @brief Owns the caller-side UTF-8 storage required by `get_page_info`.
 */
struct PageInfoOutput
{
    PageInfoOutput()
    {
        value.struct_size = sizeof(value);
        value.page_id = MakeBuffer(pageIdStorage);
        value.page_name = MakeBuffer(pageNameStorage);
        value.page_title = MakeBuffer(pageTitleStorage);
        value.source_path = MakeBuffer(sourcePathStorage);
    }

    [[nodiscard]] std::string PageId() const
    {
        return BufferToString(value.page_id);
    }

    MfdEditorAutomationPageInfo value {};
    std::array<char, kSmallTextCapacity> pageIdStorage {};
    std::array<char, kSmallTextCapacity> pageNameStorage {};
    std::array<char, kSmallTextCapacity> pageTitleStorage {};
    std::array<char, kPathTextCapacity> sourcePathStorage {};
};

/**
 * @brief Owns the caller-side UTF-8 storage required by `get_reticle_asset_info`.
 */
struct ReticleAssetInfoOutput
{
    ReticleAssetInfoOutput()
    {
        value.struct_size = sizeof(value);
        value.reticle_asset_id = MakeBuffer(reticleAssetIdStorage);
        value.template_id = MakeBuffer(templateIdStorage);
        value.source_path = MakeBuffer(sourcePathStorage);
    }

    [[nodiscard]] std::string ReticleAssetId() const
    {
        return BufferToString(value.reticle_asset_id);
    }

    [[nodiscard]] std::string TemplateId() const
    {
        return BufferToString(value.template_id);
    }

    MfdEditorAutomationReticleAssetInfo value {};
    std::array<char, kSmallTextCapacity> reticleAssetIdStorage {};
    std::array<char, kSmallTextCapacity> templateIdStorage {};
    std::array<char, kPathTextCapacity> sourcePathStorage {};
};

/**
 * @brief Owns the caller-side UTF-8 storage required by `get_layer_info`.
 */
struct LayerInfoOutput
{
    LayerInfoOutput()
    {
        value.struct_size = sizeof(value);
        value.layer_id = MakeBuffer(layerIdStorage);
        value.layer_name = MakeBuffer(layerNameStorage);
    }

    [[nodiscard]] std::string LayerId() const
    {
        return BufferToString(value.layer_id);
    }

    MfdEditorAutomationLayerInfo value {};
    std::array<char, kSmallTextCapacity> layerIdStorage {};
    std::array<char, kSmallTextCapacity> layerNameStorage {};
};

/**
 * @brief Owns the caller-side UTF-8 storage required by `get_page_reticle_info`.
 */
struct PageReticleInfoOutput
{
    PageReticleInfoOutput()
    {
        value.struct_size = sizeof(value);
        value.page_reticle_id = MakeBuffer(pageReticleIdStorage);
        value.instance_id = MakeBuffer(instanceIdStorage);
        value.source_reticle_asset_id = MakeBuffer(sourceReticleAssetIdStorage);
        value.layer_id = MakeBuffer(layerIdStorage);
    }

    [[nodiscard]] std::string PageReticleId() const
    {
        return BufferToString(value.page_reticle_id);
    }

    MfdEditorAutomationPageReticleInfo value {};
    std::array<char, kSmallTextCapacity> pageReticleIdStorage {};
    std::array<char, kSmallTextCapacity> instanceIdStorage {};
    std::array<char, kSmallTextCapacity> sourceReticleAssetIdStorage {};
    std::array<char, kSmallTextCapacity> layerIdStorage {};
};

/**
 * @brief Owns the caller-side UTF-8 storage required by `get_session_validation_diagnostic`.
 */
struct ValidationDiagnosticOutput
{
    ValidationDiagnosticOutput()
    {
        value.struct_size = sizeof(value);
        value.entity_id = MakeBuffer(entityIdStorage);
        value.message = MakeBuffer(messageStorage);
    }

    MfdEditorAutomationValidationDiagnostic value {};
    std::array<char, kSmallTextCapacity> entityIdStorage {};
    std::array<char, kEventTextCapacity> messageStorage {};
};

/**
 * @brief Owns the caller-side UTF-8 storage required by `consume_pending_event`.
 */
struct EventOutput
{
    EventOutput()
    {
        value.struct_size = sizeof(value);
        value.entity_id = MakeBuffer(entityIdStorage);
        value.message = MakeBuffer(messageStorage);
    }

    MfdEditorAutomationEvent value {};
    std::array<char, kSmallTextCapacity> entityIdStorage {};
    std::array<char, kEventTextCapacity> messageStorage {};
};

/**
 * @brief Small RAII helper keeping preview sessions rolled back unless the sample dismisses it explicitly.
 */
struct SessionScope
{
    const MfdEditorAutomationHostApi* host = nullptr;
    MfdEditorAutomationSessionHandle handle = 0U;
    bool active = false;

    ~SessionScope() noexcept
    {
        if (active && host != nullptr && host->rollback_session != nullptr)
        {
            host->rollback_session(host->host_context, handle, nullptr);
        }
    }

    void Dismiss() noexcept
    {
        active = false;
    }
};

struct SampleAutomationPluginContext
{
    const MfdEditorAutomationHostApi* host = nullptr;
    bool started = false;
    bool tickApplied = false;
    std::string firstPageId {};
    std::string firstReticleAssetId {};
    std::string firstReticleTemplateId {};
};

MfdEditorAutomationResultCode EnsureSampleWorkflowApi(const MfdEditorAutomationHostApi* host,
                                                      MfdEditorUtf8Buffer* error) noexcept
{
    // The sample keeps one readable workflow and therefore requires only the
    // subset of callbacks it actually exercises.
    if (!MFD_EDITOR_AUTOMATION_HOST_HAS_CALLBACK(host, get_snapshot_summary) ||
        !MFD_EDITOR_AUTOMATION_HOST_HAS_CALLBACK(host, get_page_info) ||
        !MFD_EDITOR_AUTOMATION_HOST_HAS_CALLBACK(host, begin_session) ||
        !MFD_EDITOR_AUTOMATION_HOST_HAS_CALLBACK(host, validate_session) ||
        !MFD_EDITOR_AUTOMATION_HOST_HAS_CALLBACK(host, create_page_asset) ||
        !MFD_EDITOR_AUTOMATION_HOST_HAS_CALLBACK(host, commit_session) ||
        !MFD_EDITOR_AUTOMATION_HOST_HAS_CALLBACK(host, rollback_session) ||
        !MFD_EDITOR_AUTOMATION_HOST_HAS_CALLBACK(host, save_all) ||
        !MFD_EDITOR_AUTOMATION_HOST_HAS_CALLBACK(host, get_reticle_asset_info) ||
        !MFD_EDITOR_AUTOMATION_HOST_HAS_CALLBACK(host, get_page_reticle_info) ||
        !MFD_EDITOR_AUTOMATION_HOST_HAS_CALLBACK(host, get_layer_info) ||
        !MFD_EDITOR_AUTOMATION_HOST_HAS_CALLBACK(host, get_session_validation_diagnostic) ||
        !MFD_EDITOR_AUTOMATION_HOST_HAS_CALLBACK(host, instantiate_reticle_on_page) ||
        !MFD_EDITOR_AUTOMATION_HOST_HAS_CALLBACK(host, move_page_reticle) ||
        !MFD_EDITOR_AUTOMATION_HOST_HAS_CALLBACK(host, set_page_reticle_visibility) ||
        !MFD_EDITOR_AUTOMATION_HOST_HAS_CALLBACK(host, set_page_reticle_draw_on_top) ||
        !MFD_EDITOR_AUTOMATION_HOST_HAS_CALLBACK(host, set_page_reticle_transform) ||
        !MFD_EDITOR_AUTOMATION_HOST_HAS_CALLBACK(host, set_page_reticle_layer) ||
        !MFD_EDITOR_AUTOMATION_HOST_HAS_CALLBACK(host, create_layer) ||
        !MFD_EDITOR_AUTOMATION_HOST_HAS_CALLBACK(host, set_layer_visibility) ||
        !MFD_EDITOR_AUTOMATION_HOST_HAS_CALLBACK(host, save_asset) ||
        !MFD_EDITOR_AUTOMATION_HOST_HAS_CALLBACK(host, consume_pending_event))
    {
        WriteMessage(error, "The editor host does not expose the automation sample workflow surface.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    return MfdEditorAutomationResultCode_Success;
}

MfdEditorAutomationResultCode ReadSnapshotSummary(const MfdEditorAutomationHostApi& host,
                                                  SnapshotSummaryOutput& output,
                                                  MfdEditorUtf8Buffer* error) noexcept
{
    return host.get_snapshot_summary(host.host_context, &output.value, error);
}

MfdEditorAutomationResultCode ReadPageInfo(const MfdEditorAutomationHostApi& host,
                                           const uint32_t pageIndex,
                                           PageInfoOutput& output,
                                           MfdEditorUtf8Buffer* error) noexcept
{
    return host.get_page_info(host.host_context, pageIndex, &output.value, error);
}

MfdEditorAutomationResultCode ReadReticleAssetInfo(const MfdEditorAutomationHostApi& host,
                                                   const uint32_t reticleIndex,
                                                   ReticleAssetInfoOutput& output,
                                                   MfdEditorUtf8Buffer* error) noexcept
{
    return host.get_reticle_asset_info(host.host_context, reticleIndex, &output.value, error);
}

MfdEditorAutomationResultCode ReadLayerInfo(const MfdEditorAutomationHostApi& host,
                                            const uint32_t pageIndex,
                                            const uint32_t layerIndex,
                                            LayerInfoOutput& output,
                                            MfdEditorUtf8Buffer* error) noexcept
{
    return host.get_layer_info(host.host_context, pageIndex, layerIndex, &output.value, error);
}

MfdEditorAutomationResultCode ReadPageReticleInfo(const MfdEditorAutomationHostApi& host,
                                                  const uint32_t pageIndex,
                                                  const uint32_t reticleIndex,
                                                  PageReticleInfoOutput& output,
                                                  MfdEditorUtf8Buffer* error) noexcept
{
    return host.get_page_reticle_info(host.host_context, pageIndex, reticleIndex, &output.value, error);
}

MfdEditorAutomationResultCode BeginSession(const MfdEditorAutomationHostApi& host,
                                           const char* label,
                                           SessionScope& session,
                                           MfdEditorUtf8Buffer* error) noexcept
{
    session.host = &host;
    const MfdEditorAutomationResultCode status =
        host.begin_session(host.host_context, MakeView(label), &session.handle, error);
    session.active = status == MfdEditorAutomationResultCode_Success;
    return status;
}

MfdEditorAutomationResultCode FindPageIndexById(const MfdEditorAutomationHostApi& host,
                                                const std::string& pageId,
                                                uint32_t* outPageIndex,
                                                MfdEditorUtf8Buffer* error) noexcept
{
    if (outPageIndex == nullptr)
    {
        WriteMessage(error, "The page index output pointer is null.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    SnapshotSummaryOutput summary;
    MfdEditorAutomationResultCode status = ReadSnapshotSummary(host, summary, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    for (uint32_t pageIndex = 0; pageIndex < summary.value.page_count; ++pageIndex)
    {
        PageInfoOutput page;
        status = ReadPageInfo(host, pageIndex, page, error);
        if (status != MfdEditorAutomationResultCode_Success)
        {
            return status;
        }

        if (page.PageId() == pageId)
        {
            *outPageIndex = pageIndex;
            return MfdEditorAutomationResultCode_Success;
        }
    }

    WriteMessage(error, "Unable to resolve the requested page id.");
    return MfdEditorAutomationResultCode_NotFound;
}

MfdEditorAutomationResultCode FindLayerIndexById(const MfdEditorAutomationHostApi& host,
                                                 const uint32_t pageIndex,
                                                 const std::string& layerId,
                                                 uint32_t* outLayerIndex,
                                                 MfdEditorUtf8Buffer* error) noexcept
{
    if (outLayerIndex == nullptr)
    {
        WriteMessage(error, "The layer index output pointer is null.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    PageInfoOutput page;
    MfdEditorAutomationResultCode status = ReadPageInfo(host, pageIndex, page, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    for (uint32_t layerIndex = 0; layerIndex < page.value.layer_count; ++layerIndex)
    {
        LayerInfoOutput layer;
        status = ReadLayerInfo(host, pageIndex, layerIndex, layer, error);
        if (status != MfdEditorAutomationResultCode_Success)
        {
            return status;
        }

        if (layer.LayerId() == layerId)
        {
            *outLayerIndex = layerIndex;
            return MfdEditorAutomationResultCode_Success;
        }
    }

    WriteMessage(error, "Unable to resolve the requested layer id.");
    return MfdEditorAutomationResultCode_NotFound;
}

MfdEditorAutomationResultCode FindPageReticleIndexById(const MfdEditorAutomationHostApi& host,
                                                       const uint32_t pageIndex,
                                                       const std::string& pageReticleId,
                                                       uint32_t* outReticleIndex,
                                                       MfdEditorUtf8Buffer* error) noexcept
{
    if (outReticleIndex == nullptr)
    {
        WriteMessage(error, "The reticle index output pointer is null.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    PageInfoOutput page;
    MfdEditorAutomationResultCode status = ReadPageInfo(host, pageIndex, page, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    for (uint32_t reticleIndex = 0; reticleIndex < page.value.reticle_count; ++reticleIndex)
    {
        PageReticleInfoOutput reticle;
        status = ReadPageReticleInfo(host, pageIndex, reticleIndex, reticle, error);
        if (status != MfdEditorAutomationResultCode_Success)
        {
            return status;
        }

        if (reticle.PageReticleId() == pageReticleId)
        {
            *outReticleIndex = reticleIndex;
            return MfdEditorAutomationResultCode_Success;
        }
    }

    WriteMessage(error, "Unable to resolve the requested page reticle id.");
    return MfdEditorAutomationResultCode_NotFound;
}

MfdEditorAutomationResultCode ValidateSessionAndIllustrateDiagnostics(const MfdEditorAutomationHostApi& host,
                                                                     const MfdEditorAutomationSessionHandle sessionHandle,
                                                                     MfdEditorUtf8Buffer* error) noexcept
{
    MfdEditorAutomationValidationSummary validation {};
    validation.struct_size = sizeof(validation);

    MfdEditorAutomationResultCode status =
        host.validate_session(host.host_context, sessionHandle, &validation, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    // The sample deliberately expects a valid session. We still call the
    // diagnostic accessor to show both supported outcomes:
    // - `Success` when diagnostics exist
    // - `NotFound` when `diagnostic_count == 0`
    ValidationDiagnosticOutput diagnostic;
    status = host.get_session_validation_diagnostic(host.host_context, sessionHandle, 0U, &diagnostic.value, nullptr);

    if (validation.valid != 0U && validation.diagnostic_count == 0U)
    {
        if (status != MfdEditorAutomationResultCode_NotFound)
        {
            WriteMessage(error, "A valid session should not expose indexed validation diagnostics.");
            return MfdEditorAutomationResultCode_InternalFailure;
        }

        return MfdEditorAutomationResultCode_Success;
    }

    if (status != MfdEditorAutomationResultCode_Success)
    {
        WriteMessage(error, "The session reported diagnostics but the first diagnostic could not be read.");
        return status;
    }

    WriteMessage(error, "The sample session unexpectedly failed validation.");
    return MfdEditorAutomationResultCode_ValidationFailed;
}

MfdEditorAutomationResultCode CreatePreviewPage(const MfdEditorAutomationHostApi& host,
                                                const MfdEditorAutomationSessionHandle sessionHandle,
                                                std::string& outPageId,
                                                MfdEditorUtf8Buffer* error) noexcept
{
    std::array<char, kSmallTextCapacity> createdPageIdStorage {};
    MfdEditorAutomationCreatePageAssetRequest request {};
    request.struct_size = sizeof(request);
    request.session = sessionHandle;
    request.name = MakeView("SamplePluginPreviewPage");
    request.title = MakeView("Sample Plugin Preview Page");
    request.relative_source_path = MakeView("sample_plugin_preview_page.json");
    request.make_default = 0U;
    request.created_page_id = MakeBuffer(createdPageIdStorage);

    const MfdEditorAutomationResultCode status =
        host.create_page_asset(host.host_context, &request, error);
    if (status == MfdEditorAutomationResultCode_Success)
    {
        outPageId = BufferToString(request.created_page_id);
    }

    return status;
}

MfdEditorAutomationResultCode CreatePreviewLayer(const MfdEditorAutomationHostApi& host,
                                                 const MfdEditorAutomationSessionHandle sessionHandle,
                                                 const std::string& pageId,
                                                 std::string& outLayerId,
                                                 MfdEditorUtf8Buffer* error) noexcept
{
    std::array<char, kSmallTextCapacity> createdLayerIdStorage {};
    MfdEditorAutomationCreateLayerRequest request {};
    request.struct_size = sizeof(request);
    request.session = sessionHandle;
    request.page_id = MakeView(pageId);
    request.layer_name = MakeView("sample_preview_overlay");
    request.insert_index = -1;
    request.visible = 1U;
    request.created_layer_id = MakeBuffer(createdLayerIdStorage);

    const MfdEditorAutomationResultCode status =
        host.create_layer(host.host_context, &request, error);
    if (status == MfdEditorAutomationResultCode_Success)
    {
        outLayerId = BufferToString(request.created_layer_id);
    }

    return status;
}

MfdEditorAutomationResultCode InstantiatePreviewReticle(const MfdEditorAutomationHostApi& host,
                                                        const MfdEditorAutomationSessionHandle sessionHandle,
                                                        const std::string& pageId,
                                                        const std::string& reticleAssetId,
                                                        const char* requestedInstanceId,
                                                        const MfdEditorAutomationTransform2D transform,
                                                        std::string& outPageReticleId,
                                                        MfdEditorUtf8Buffer* error) noexcept
{
    std::array<char, kSmallTextCapacity> createdPageReticleIdStorage {};
    MfdEditorAutomationInstantiateReticleOnPageRequest request {};
    request.struct_size = sizeof(request);
    request.session = sessionHandle;
    request.page_id = MakeView(pageId);
    request.reticle_asset_id = MakeView(reticleAssetId);
    request.requested_instance_id = MakeView(requestedInstanceId);
    request.transform = transform;
    request.assign_default_layer = 1U;
    request.created_page_reticle_id = MakeBuffer(createdPageReticleIdStorage);

    const MfdEditorAutomationResultCode status =
        host.instantiate_reticle_on_page(host.host_context, &request, error);
    if (status == MfdEditorAutomationResultCode_Success)
    {
        outPageReticleId = BufferToString(request.created_page_reticle_id);
    }

    return status;
}

MfdEditorAutomationResultCode ConsumeAllPendingEvents(const MfdEditorAutomationHostApi& host,
                                                      std::size_t* outEventCount,
                                                      MfdEditorUtf8Buffer* error) noexcept
{
    std::size_t eventCount = 0U;
    for (;;)
    {
        EventOutput event;
        const MfdEditorAutomationResultCode status =
            host.consume_pending_event(host.host_context, &event.value, error);
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

    if (outEventCount != nullptr)
    {
        *outEventCount = eventCount;
    }

    return MfdEditorAutomationResultCode_Success;
}

MfdEditorAutomationResultCode RunRollbackMutationWalkthrough(SampleAutomationPluginContext& context,
                                                             MfdEditorUtf8Buffer* error) noexcept
{
    SessionScope session;
    MfdEditorAutomationResultCode status =
        BeginSession(*context.host, "sample-rollback-preview", session, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    // `create_page_asset` illustrates authored page creation inside a preview session.
    std::string previewPageId;
    status = CreatePreviewPage(*context.host, session.handle, previewPageId, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    // `create_layer` illustrates stable page-layer authoring without exposing editor internals.
    std::string previewLayerId;
    status = CreatePreviewLayer(*context.host, session.handle, previewPageId, previewLayerId, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    // Two instances are created so the sample can demonstrate `move_page_reticle`.
    std::string alphaReticleId;
    status = InstantiatePreviewReticle(*context.host,
                                       session.handle,
                                       previewPageId,
                                       context.firstReticleAssetId,
                                       "sample_preview_alpha",
                                       MfdEditorAutomationTransform2D {-0.20f, 0.0f, 0.0f, 1.0f, 1.0f},
                                       alphaReticleId,
                                       error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    std::string betaReticleId;
    status = InstantiatePreviewReticle(*context.host,
                                       session.handle,
                                       previewPageId,
                                       context.firstReticleAssetId,
                                       "sample_preview_beta",
                                       MfdEditorAutomationTransform2D {0.20f, 0.0f, 0.0f, 1.0f, 1.0f},
                                       betaReticleId,
                                       error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    // `move_page_reticle` reorders the second reticle to the front of the page draw list.
    MfdEditorAutomationMovePageReticleRequest moveRequest {};
    moveRequest.struct_size = sizeof(moveRequest);
    moveRequest.session = session.handle;
    moveRequest.page_reticle_id = MakeView(betaReticleId);
    moveRequest.target_index = 0;
    status = context.host->move_page_reticle(context.host->host_context, &moveRequest, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    MfdEditorAutomationSetPageReticleLayerRequest setLayerRequest {};
    setLayerRequest.struct_size = sizeof(setLayerRequest);
    setLayerRequest.session = session.handle;
    setLayerRequest.page_reticle_id = MakeView(betaReticleId);
    setLayerRequest.layer_id = MakeView(previewLayerId);
    status = context.host->set_page_reticle_layer(context.host->host_context, &setLayerRequest, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    MfdEditorAutomationSetLayerVisibilityRequest setLayerVisibilityRequest {};
    setLayerVisibilityRequest.struct_size = sizeof(setLayerVisibilityRequest);
    setLayerVisibilityRequest.session = session.handle;
    setLayerVisibilityRequest.page_id = MakeView(previewPageId);
    setLayerVisibilityRequest.layer_id = MakeView(previewLayerId);
    setLayerVisibilityRequest.visible = 0U;
    status = context.host->set_layer_visibility(context.host->host_context, &setLayerVisibilityRequest, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    // Toggle it back on so the preview session ends in a clean and valid state.
    setLayerVisibilityRequest.visible = 1U;
    status = context.host->set_layer_visibility(context.host->host_context, &setLayerVisibilityRequest, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    MfdEditorAutomationSetPageReticleVisibilityRequest setVisibilityRequest {};
    setVisibilityRequest.struct_size = sizeof(setVisibilityRequest);
    setVisibilityRequest.session = session.handle;
    setVisibilityRequest.page_reticle_id = MakeView(alphaReticleId);
    setVisibilityRequest.visible = 0U;
    status = context.host->set_page_reticle_visibility(context.host->host_context, &setVisibilityRequest, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    MfdEditorAutomationSetPageReticleDrawOnTopRequest setDrawOnTopRequest {};
    setDrawOnTopRequest.struct_size = sizeof(setDrawOnTopRequest);
    setDrawOnTopRequest.session = session.handle;
    setDrawOnTopRequest.page_reticle_id = MakeView(betaReticleId);
    setDrawOnTopRequest.draw_on_top = 1U;
    status = context.host->set_page_reticle_draw_on_top(context.host->host_context, &setDrawOnTopRequest, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    MfdEditorAutomationSetPageReticleTransformRequest setTransformRequest {};
    setTransformRequest.struct_size = sizeof(setTransformRequest);
    setTransformRequest.session = session.handle;
    setTransformRequest.page_reticle_id = MakeView(betaReticleId);
    setTransformRequest.transform = MfdEditorAutomationTransform2D {0.35f, -0.15f, 18.0f, 1.30f, 0.80f};
    status = context.host->set_page_reticle_transform(context.host->host_context, &setTransformRequest, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    // The sample deliberately reads back the newly created entities instead of
    // assuming their indices. This illustrates how plugins can enumerate the
    // stable model after each mutation.
    uint32_t previewPageIndex = 0U;
    status = FindPageIndexById(*context.host, previewPageId, &previewPageIndex, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    PageInfoOutput previewPageInfo;
    status = ReadPageInfo(*context.host, previewPageIndex, previewPageInfo, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    uint32_t previewLayerIndex = 0U;
    status = FindLayerIndexById(*context.host, previewPageIndex, previewLayerId, &previewLayerIndex, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    LayerInfoOutput previewLayerInfo;
    status = ReadLayerInfo(*context.host, previewPageIndex, previewLayerIndex, previewLayerInfo, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    uint32_t betaReticleIndex = 0U;
    status = FindPageReticleIndexById(*context.host, previewPageIndex, betaReticleId, &betaReticleIndex, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    uint32_t alphaReticleIndex = 0U;
    status = FindPageReticleIndexById(*context.host, previewPageIndex, alphaReticleId, &alphaReticleIndex, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    PageReticleInfoOutput betaReticleInfo;
    status = ReadPageReticleInfo(*context.host, previewPageIndex, betaReticleIndex, betaReticleInfo, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    PageReticleInfoOutput alphaReticleInfo;
    status = ReadPageReticleInfo(*context.host, previewPageIndex, alphaReticleIndex, alphaReticleInfo, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    // These checks make the sample self-documenting: the plugin proves the
    // ordering/readback semantics it expects from the host.
    if (previewPageInfo.value.reticle_count != 2U ||
        betaReticleInfo.PageReticleId() != betaReticleId ||
        alphaReticleInfo.PageReticleId() != alphaReticleId ||
        betaReticleInfo.value.reticle_index != 0U)
    {
        WriteMessage(error, "The sample preview page did not expose the expected post-mutation state.");
        return MfdEditorAutomationResultCode_InternalFailure;
    }

    status = ValidateSessionAndIllustrateDiagnostics(*context.host, session.handle, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    status = context.host->rollback_session(context.host->host_context, session.handle, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    session.Dismiss();
    return MfdEditorAutomationResultCode_Success;
}

MfdEditorAutomationResultCode RunCommitAndPersistenceWalkthrough(SampleAutomationPluginContext& context,
                                                                 MfdEditorUtf8Buffer* error) noexcept
{
    // A second session illustrates the explicit commit path without leaving
    // extra authored entities behind.
    SessionScope session;
    MfdEditorAutomationResultCode status =
        BeginSession(*context.host, "sample-commit-checkpoint", session, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    status = ValidateSessionAndIllustrateDiagnostics(*context.host, session.handle, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    status = context.host->commit_session(context.host->host_context, session.handle, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    session.Dismiss();

    // `save_asset` is illustrated for both supported kinds: page and reticle asset.
    MfdEditorAutomationSaveSummary saveSummary {};
    saveSummary.struct_size = sizeof(saveSummary);

    MfdEditorAutomationSaveAssetRequest savePageRequest {};
    savePageRequest.struct_size = sizeof(savePageRequest);
    savePageRequest.kind = MfdEditorAutomationSaveAssetKind_Page;
    savePageRequest.entity_id = MakeView(context.firstPageId);
    status = context.host->save_asset(context.host->host_context, &savePageRequest, &saveSummary, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    MfdEditorAutomationSaveAssetRequest saveReticleRequest {};
    saveReticleRequest.struct_size = sizeof(saveReticleRequest);
    saveReticleRequest.kind = MfdEditorAutomationSaveAssetKind_ReticleAsset;
    saveReticleRequest.entity_id = MakeView(context.firstReticleAssetId);
    status = context.host->save_asset(context.host->host_context, &saveReticleRequest, &saveSummary, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    status = context.host->save_all(context.host->host_context, &saveSummary, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    return MfdEditorAutomationResultCode_Success;
}

MfdEditorAutomationResultCode LoadBaselineReferences(SampleAutomationPluginContext& context,
                                                     MfdEditorUtf8Buffer* error) noexcept
{
    SnapshotSummaryOutput summary;
    MfdEditorAutomationResultCode status = ReadSnapshotSummary(*context.host, summary, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    if (summary.value.page_count == 0U || summary.value.reticle_asset_count == 0U)
    {
        WriteMessage(error, "The sample plugin expects at least one page and one reticle asset.");
        return MfdEditorAutomationResultCode_InvalidState;
    }

    PageInfoOutput firstPage;
    status = ReadPageInfo(*context.host, 0U, firstPage, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    ReticleAssetInfoOutput firstReticleAsset;
    status = ReadReticleAssetInfo(*context.host, 0U, firstReticleAsset, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    // Read the first layer when one exists so the sample covers `get_layer_info`
    // even before it starts mutating the document.
    if (firstPage.value.layer_count > 0U)
    {
        LayerInfoOutput firstLayer;
        status = ReadLayerInfo(*context.host, 0U, 0U, firstLayer, error);
        if (status != MfdEditorAutomationResultCode_Success)
        {
            return status;
        }
    }

    context.firstPageId = firstPage.PageId();
    context.firstReticleAssetId = firstReticleAsset.ReticleAssetId();
    context.firstReticleTemplateId = firstReticleAsset.TemplateId();
    return MfdEditorAutomationResultCode_Success;
}

MfdEditorAutomationResultCode MFD_EDITOR_AUTOMATION_CALL StartPlugin(void* pluginContext,
                                                                     const MfdEditorAutomationHostApi* host,
                                                                     MfdEditorUtf8Buffer* error) noexcept
{
    if (pluginContext == nullptr || host == nullptr)
    {
        WriteMessage(error, "The sample automation plugin received an invalid start request.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    MfdEditorAutomationResultCode status = EnsureSampleWorkflowApi(host, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    auto* context = static_cast<SampleAutomationPluginContext*>(pluginContext);
    context->host = host;

    status = LoadBaselineReferences(*context, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    context->started = true;
    return MfdEditorAutomationResultCode_Success;
}

MfdEditorAutomationResultCode MFD_EDITOR_AUTOMATION_CALL TickPlugin(void* pluginContext,
                                                                    MfdEditorUtf8Buffer* error) noexcept
{
    auto* context = static_cast<SampleAutomationPluginContext*>(pluginContext);
    if (context == nullptr || !context->started || context->host == nullptr)
    {
        WriteMessage(error, "The sample automation plugin was not started correctly.");
        return MfdEditorAutomationResultCode_InvalidState;
    }

    if (context->tickApplied)
    {
        return MfdEditorAutomationResultCode_Success;
    }

    MfdEditorAutomationResultCode status = RunRollbackMutationWalkthrough(*context, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    std::size_t rollbackEventCount = 0U;
    status = ConsumeAllPendingEvents(*context->host, &rollbackEventCount, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    status = RunCommitAndPersistenceWalkthrough(*context, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    std::size_t persistenceEventCount = 0U;
    status = ConsumeAllPendingEvents(*context->host, &persistenceEventCount, error);
    if (status != MfdEditorAutomationResultCode_Success)
    {
        return status;
    }

    if (rollbackEventCount == 0U && persistenceEventCount == 0U)
    {
        WriteMessage(error, "The sample automation plugin expected at least one queued automation event.");
        return MfdEditorAutomationResultCode_InternalFailure;
    }

    context->tickApplied = true;
    return MfdEditorAutomationResultCode_Success;
}

void MFD_EDITOR_AUTOMATION_CALL StopPlugin(void* pluginContext) noexcept
{
    if (auto* context = static_cast<SampleAutomationPluginContext*>(pluginContext); context != nullptr)
    {
        context->host = nullptr;
        context->started = false;
        context->tickApplied = false;
        context->firstPageId.clear();
        context->firstReticleAssetId.clear();
        context->firstReticleTemplateId.clear();
    }
}

void MFD_EDITOR_AUTOMATION_CALL DestroyPlugin(void* pluginContext) noexcept
{
    delete static_cast<SampleAutomationPluginContext*>(pluginContext);
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

