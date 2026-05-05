/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Hidden loader used by the editor and tests to host one stable-ABI automation plugin DLL in-process.
 */

#include "EditorAutomationPluginLoader.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <exception>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(_WIN32)
#    include <Windows.h>
#endif

#include "EditorAutomationDocumentUtils.h"

namespace
{
#if defined(_WIN32)
std::string FormatWindowsErrorMessage(const DWORD errorCode)
{
    if (errorCode == 0U)
    {
        return {};
    }

    LPWSTR rawBuffer = nullptr;
    const DWORD size = ::FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                            FORMAT_MESSAGE_FROM_SYSTEM |
                                            FORMAT_MESSAGE_IGNORE_INSERTS,
                                        nullptr,
                                        errorCode,
                                        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                        reinterpret_cast<LPWSTR>(&rawBuffer),
                                        0,
                                        nullptr);
    if (size == 0U || rawBuffer == nullptr)
    {
        return "Windows error " + std::to_string(errorCode);
    }

    std::wstring message(rawBuffer, rawBuffer + size);
    ::LocalFree(rawBuffer);
    while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n' || message.back() == L' '))
    {
        message.pop_back();
    }

    if (message.empty())
    {
        return {};
    }

    const int requiredSize = ::WideCharToMultiByte(CP_UTF8,
                                                   0,
                                                   message.c_str(),
                                                   static_cast<int>(message.size()),
                                                   nullptr,
                                                   0,
                                                   nullptr,
                                                   nullptr);
    if (requiredSize <= 0)
    {
        return "Windows error " + std::to_string(errorCode);
    }

    std::string utf8(static_cast<std::size_t>(requiredSize), '\0');
    const int convertedSize = ::WideCharToMultiByte(CP_UTF8,
                                                    0,
                                                    message.c_str(),
                                                    static_cast<int>(message.size()),
                                                    utf8.data(),
                                                    requiredSize,
                                                    nullptr,
                                                    nullptr);
    if (convertedSize <= 0)
    {
        return "Windows error " + std::to_string(errorCode);
    }

    return utf8;
}
#endif

constexpr std::size_t kPluginErrorBufferCapacity = 1024U;

void ResetBuffer(MfdEditorUtf8Buffer& buffer, char* storage, const std::size_t capacity)
{
    if (storage != nullptr && capacity > 0U)
    {
        storage[0] = '\0';
    }
    buffer.data = storage;
    buffer.capacity = capacity;
    buffer.size = 0U;
}

std::string BufferToString(const MfdEditorUtf8Buffer& buffer)
{
    if (buffer.data == nullptr)
    {
        return {};
    }

    const std::size_t available = buffer.capacity > 0U ? buffer.capacity - 1U : 0U;
    const std::size_t readable = (std::min)(buffer.size, available);
    return std::string(buffer.data, buffer.data + readable);
}

MfdEditorAutomationResultCode CopyUtf8ToBuffer(MfdEditorUtf8Buffer* buffer, const std::string_view text)
{
    if (buffer == nullptr)
    {
        return MfdEditorAutomationResultCode_Success;
    }

    buffer->size = text.size();
    if (buffer->data == nullptr || buffer->capacity == 0U)
    {
        return MfdEditorAutomationResultCode_Success;
    }

    const std::size_t writable = (std::min)(text.size(), buffer->capacity - 1U);
    if (writable > 0U)
    {
        std::memcpy(buffer->data, text.data(), writable);
    }
    buffer->data[writable] = '\0';
    return writable == text.size() ? MfdEditorAutomationResultCode_Success : MfdEditorAutomationResultCode_BufferTooSmall;
}

std::string ViewToString(const MfdEditorStringView view)
{
    if (view.data == nullptr || view.size == 0U)
    {
        return {};
    }

    return std::string(view.data, view.data + view.size);
}

bool IsValidView(const MfdEditorStringView view) noexcept
{
    return view.size == 0U || view.data != nullptr;
}

MfdEditorAutomationResultCode MapAutomationErrorCode(const editor::automation::AutomationErrorCode code)
{
    using editor::automation::AutomationErrorCode;
    switch (code)
    {
    case AutomationErrorCode::None:
        return MfdEditorAutomationResultCode_Success;
    case AutomationErrorCode::InvalidArgument:
        return MfdEditorAutomationResultCode_InvalidArgument;
    case AutomationErrorCode::InvalidState:
        return MfdEditorAutomationResultCode_InvalidState;
    case AutomationErrorCode::NotFound:
        return MfdEditorAutomationResultCode_NotFound;
    case AutomationErrorCode::Conflict:
        return MfdEditorAutomationResultCode_Conflict;
    case AutomationErrorCode::ValidationFailed:
        return MfdEditorAutomationResultCode_ValidationFailed;
    case AutomationErrorCode::IoFailure:
        return MfdEditorAutomationResultCode_IoFailure;
    case AutomationErrorCode::TransportFailure:
        return MfdEditorAutomationResultCode_TransportFailure;
    case AutomationErrorCode::Unsupported:
        return MfdEditorAutomationResultCode_Unsupported;
    case AutomationErrorCode::InternalFailure:
        return MfdEditorAutomationResultCode_InternalFailure;
    }

    return MfdEditorAutomationResultCode_InternalFailure;
}

std::string ResultCodeDescription(const MfdEditorAutomationResultCode code)
{
    switch (code)
    {
    case MfdEditorAutomationResultCode_Success:
        return "success";
    case MfdEditorAutomationResultCode_BufferTooSmall:
        return "buffer too small";
    case MfdEditorAutomationResultCode_InvalidArgument:
        return "invalid argument";
    case MfdEditorAutomationResultCode_InvalidState:
        return "invalid state";
    case MfdEditorAutomationResultCode_NotFound:
        return "not found";
    case MfdEditorAutomationResultCode_Conflict:
        return "conflict";
    case MfdEditorAutomationResultCode_ValidationFailed:
        return "validation failed";
    case MfdEditorAutomationResultCode_IoFailure:
        return "I/O failure";
    case MfdEditorAutomationResultCode_TransportFailure:
        return "transport failure";
    case MfdEditorAutomationResultCode_Unsupported:
        return "unsupported";
    case MfdEditorAutomationResultCode_InternalFailure:
        return "internal failure";
    case MfdEditorAutomationResultCode_AbiMismatch:
        return "ABI mismatch";
    }

    return "unknown error";
}

MfdEditorAutomationResultCode MergeBufferStatus(const MfdEditorAutomationResultCode current,
                                                const MfdEditorAutomationResultCode next)
{
    if (current != MfdEditorAutomationResultCode_Success)
    {
        return current;
    }

    return next;
}

MfdEditorAutomationTransform2D ToApiTransform(const mfd::Transform2D& transform) noexcept
{
    return MfdEditorAutomationTransform2D {
        transform.position.x,
        transform.position.y,
        transform.rotationDegrees,
        transform.scale.x,
        transform.scale.y};
}

mfd::Transform2D ToModelTransform(const MfdEditorAutomationTransform2D& transform) noexcept
{
    return mfd::Transform2D {
        {transform.position_x, transform.position_y},
        transform.rotation_degrees,
        {transform.scale_x, transform.scale_y}};
}

MfdEditorAutomationVec2 ToApiVec2(const mfd::Vec2& value) noexcept
{
    return MfdEditorAutomationVec2 {value.x, value.y};
}

MfdEditorAutomationColorRgba ToApiColor(const mfd::ColorRgba& color) noexcept
{
    return MfdEditorAutomationColorRgba {color.r, color.g, color.b, color.a};
}

MfdEditorAutomationPageView ToApiPageView(const mfd::PageViewState& view) noexcept
{
    return MfdEditorAutomationPageView {ToApiVec2(view.center), view.zoom, 0.0f};
}

uint32_t MapSelectionKind(const editor::automation::AutomationSelectionKind kind) noexcept
{
    using editor::automation::AutomationSelectionKind;
    switch (kind)
    {
    case AutomationSelectionKind::None:
        return MfdEditorAutomationSelectionKind_None;
    case AutomationSelectionKind::Page:
        return MfdEditorAutomationSelectionKind_Page;
    case AutomationSelectionKind::PageReticleInstance:
        return MfdEditorAutomationSelectionKind_PageReticleInstance;
    case AutomationSelectionKind::ReticleAsset:
        return MfdEditorAutomationSelectionKind_ReticleAsset;
    case AutomationSelectionKind::Primitive:
        return MfdEditorAutomationSelectionKind_Primitive;
    }

    return MfdEditorAutomationSelectionKind_None;
}

std::optional<editor::automation::AutomationSelectionKind> MapSelectionKind(const uint32_t kind) noexcept
{
    using editor::automation::AutomationSelectionKind;
    switch (kind)
    {
    case MfdEditorAutomationSelectionKind_None:
        return AutomationSelectionKind::None;
    case MfdEditorAutomationSelectionKind_Page:
        return AutomationSelectionKind::Page;
    case MfdEditorAutomationSelectionKind_PageReticleInstance:
        return AutomationSelectionKind::PageReticleInstance;
    case MfdEditorAutomationSelectionKind_ReticleAsset:
        return AutomationSelectionKind::ReticleAsset;
    case MfdEditorAutomationSelectionKind_Primitive:
        return AutomationSelectionKind::Primitive;
    default:
        return std::nullopt;
    }
}

uint32_t MapPrimitiveOwnerKind(const editor::automation::AutomationPrimitiveOwnerKind kind) noexcept
{
    using editor::automation::AutomationPrimitiveOwnerKind;
    switch (kind)
    {
    case AutomationPrimitiveOwnerKind::ReticleAsset:
        return MfdEditorAutomationPrimitiveOwnerKind_ReticleAsset;
    case AutomationPrimitiveOwnerKind::PageReticleInstance:
        return MfdEditorAutomationPrimitiveOwnerKind_PageReticleInstance;
    }

    return MfdEditorAutomationPrimitiveOwnerKind_ReticleAsset;
}

std::optional<editor::automation::AutomationPrimitiveOwnerKind> MapPrimitiveOwnerKind(const uint32_t kind) noexcept
{
    using editor::automation::AutomationPrimitiveOwnerKind;
    switch (kind)
    {
    case MfdEditorAutomationPrimitiveOwnerKind_ReticleAsset:
        return AutomationPrimitiveOwnerKind::ReticleAsset;
    case MfdEditorAutomationPrimitiveOwnerKind_PageReticleInstance:
        return AutomationPrimitiveOwnerKind::PageReticleInstance;
    default:
        return std::nullopt;
    }
}

uint32_t MapPrimitiveType(const mfd::PrimitiveType type) noexcept
{
    switch (type)
    {
    case mfd::PrimitiveType::Text:
        return MfdEditorAutomationPrimitiveType_Text;
    case mfd::PrimitiveType::Time:
        return MfdEditorAutomationPrimitiveType_Time;
    case mfd::PrimitiveType::Line:
        return MfdEditorAutomationPrimitiveType_Line;
    case mfd::PrimitiveType::Circle:
        return MfdEditorAutomationPrimitiveType_Circle;
    case mfd::PrimitiveType::Ring:
        return MfdEditorAutomationPrimitiveType_Ring;
    case mfd::PrimitiveType::Rectangle:
        return MfdEditorAutomationPrimitiveType_Rectangle;
    case mfd::PrimitiveType::Ellipse:
        return MfdEditorAutomationPrimitiveType_Ellipse;
    case mfd::PrimitiveType::Square:
        return MfdEditorAutomationPrimitiveType_Square;
    case mfd::PrimitiveType::Diamond:
        return MfdEditorAutomationPrimitiveType_Diamond;
    case mfd::PrimitiveType::Triangle:
        return MfdEditorAutomationPrimitiveType_Triangle;
    case mfd::PrimitiveType::Polyline:
        return MfdEditorAutomationPrimitiveType_Polyline;
    case mfd::PrimitiveType::Bezier:
        return MfdEditorAutomationPrimitiveType_Bezier;
    case mfd::PrimitiveType::Arc:
        return MfdEditorAutomationPrimitiveType_Arc;
    case mfd::PrimitiveType::Image:
        return MfdEditorAutomationPrimitiveType_Image;
    }

    return MfdEditorAutomationPrimitiveType_Line;
}

std::optional<mfd::PrimitiveType> MapPrimitiveType(const uint32_t type) noexcept
{
    switch (type)
    {
    case MfdEditorAutomationPrimitiveType_Text:
        return mfd::PrimitiveType::Text;
    case MfdEditorAutomationPrimitiveType_Time:
        return mfd::PrimitiveType::Time;
    case MfdEditorAutomationPrimitiveType_Line:
        return mfd::PrimitiveType::Line;
    case MfdEditorAutomationPrimitiveType_Circle:
        return mfd::PrimitiveType::Circle;
    case MfdEditorAutomationPrimitiveType_Ring:
        return mfd::PrimitiveType::Ring;
    case MfdEditorAutomationPrimitiveType_Rectangle:
        return mfd::PrimitiveType::Rectangle;
    case MfdEditorAutomationPrimitiveType_Ellipse:
        return mfd::PrimitiveType::Ellipse;
    case MfdEditorAutomationPrimitiveType_Square:
        return mfd::PrimitiveType::Square;
    case MfdEditorAutomationPrimitiveType_Diamond:
        return mfd::PrimitiveType::Diamond;
    case MfdEditorAutomationPrimitiveType_Triangle:
        return mfd::PrimitiveType::Triangle;
    case MfdEditorAutomationPrimitiveType_Polyline:
        return mfd::PrimitiveType::Polyline;
    case MfdEditorAutomationPrimitiveType_Bezier:
        return mfd::PrimitiveType::Bezier;
    case MfdEditorAutomationPrimitiveType_Arc:
        return mfd::PrimitiveType::Arc;
    case MfdEditorAutomationPrimitiveType_Image:
        return mfd::PrimitiveType::Image;
    default:
        return std::nullopt;
    }
}

uint32_t MapLineStyle(const mfd::LineStyle lineStyle) noexcept
{
    switch (lineStyle)
    {
    case mfd::LineStyle::Solid:
        return MfdEditorAutomationLineStyle_Solid;
    case mfd::LineStyle::Dotted:
        return MfdEditorAutomationLineStyle_Dotted;
    case mfd::LineStyle::Dashed:
        return MfdEditorAutomationLineStyle_Dashed;
    }

    return MfdEditorAutomationLineStyle_Solid;
}

std::optional<mfd::LineStyle> MapLineStyle(const uint32_t lineStyle) noexcept
{
    switch (lineStyle)
    {
    case MfdEditorAutomationLineStyle_Solid:
        return mfd::LineStyle::Solid;
    case MfdEditorAutomationLineStyle_Dotted:
        return mfd::LineStyle::Dotted;
    case MfdEditorAutomationLineStyle_Dashed:
        return mfd::LineStyle::Dashed;
    default:
        return std::nullopt;
    }
}

uint32_t MapReticleClipMode(const mfd::ReticleClipMode mode) noexcept
{
    switch (mode)
    {
    case mfd::ReticleClipMode::None:
        return MfdEditorAutomationReticleClipMode_None;
    case mfd::ReticleClipMode::Inner:
        return MfdEditorAutomationReticleClipMode_Inner;
    case mfd::ReticleClipMode::Outer:
        return MfdEditorAutomationReticleClipMode_Outer;
    }

    return MfdEditorAutomationReticleClipMode_None;
}

std::optional<mfd::ReticleClipMode> MapReticleClipMode(const uint32_t mode) noexcept
{
    switch (mode)
    {
    case MfdEditorAutomationReticleClipMode_None:
        return mfd::ReticleClipMode::None;
    case MfdEditorAutomationReticleClipMode_Inner:
        return mfd::ReticleClipMode::Inner;
    case MfdEditorAutomationReticleClipMode_Outer:
        return mfd::ReticleClipMode::Outer;
    default:
        return std::nullopt;
    }
}

std::optional<editor::automation::AutomationReticleTargetKind> MapReticleTargetKind(const uint32_t kind) noexcept
{
    using editor::automation::AutomationReticleTargetKind;
    switch (kind)
    {
    case MfdEditorAutomationReticleTargetKind_ReticleAsset:
        return AutomationReticleTargetKind::ReticleAsset;
    case MfdEditorAutomationReticleTargetKind_PageReticleInstance:
        return AutomationReticleTargetKind::PageReticleInstance;
    case MfdEditorAutomationReticleTargetKind_PageStrobe:
        return AutomationReticleTargetKind::PageStrobe;
    default:
        return std::nullopt;
    }
}

bool LooksLikeStablePrimitiveId(const std::string_view primitiveId) noexcept
{
    return primitiveId.size() > std::string_view {"primitive:"}.size() &&
           primitiveId.compare(0, std::string_view {"primitive:"}.size(), "primitive:") == 0;
}

uint32_t MapEventKind(const editor::automation::AutomationEventKind kind) noexcept
{
    using editor::automation::AutomationEventKind;
    switch (kind)
    {
    case AutomationEventKind::DocumentChanged:
        return MfdEditorAutomationEventKind_DocumentChanged;
    case AutomationEventKind::SelectionChanged:
        return MfdEditorAutomationEventKind_SelectionChanged;
    case AutomationEventKind::ActivePageChanged:
        return MfdEditorAutomationEventKind_ActivePageChanged;
    case AutomationEventKind::ValidationChanged:
        return MfdEditorAutomationEventKind_ValidationChanged;
    case AutomationEventKind::AssetSaved:
        return MfdEditorAutomationEventKind_AssetSaved;
    case AutomationEventKind::SessionCommitted:
        return MfdEditorAutomationEventKind_SessionCommitted;
    case AutomationEventKind::SessionRolledBack:
        return MfdEditorAutomationEventKind_SessionRolledBack;
    }

    return MfdEditorAutomationEventKind_DocumentChanged;
}

std::optional<editor::automation::SaveAssetRequest::Kind> MapSaveAssetKind(const uint32_t kind) noexcept
{
    switch (kind)
    {
    case MfdEditorAutomationSaveAssetKind_Page:
        return editor::automation::SaveAssetRequest::Kind::Page;
    case MfdEditorAutomationSaveAssetKind_ReticleAsset:
        return editor::automation::SaveAssetRequest::Kind::ReticleAsset;
    default:
        return std::nullopt;
    }
}

std::optional<editor::automation::ExportJsonPreviewRequest::Kind> MapExportJsonPreviewKind(const uint32_t kind) noexcept
{
    using Kind = editor::automation::ExportJsonPreviewRequest::Kind;
    switch (kind)
    {
    case MfdEditorAutomationExportJsonPreviewKind_Window:
        return Kind::Window;
    case MfdEditorAutomationExportJsonPreviewKind_Page:
        return Kind::Page;
    case MfdEditorAutomationExportJsonPreviewKind_ReticleAsset:
        return Kind::ReticleAsset;
    case MfdEditorAutomationExportJsonPreviewKind_PageReticleInstance:
        return Kind::PageReticleInstance;
    default:
        return std::nullopt;
    }
}

std::string PrimitiveContent(const mfd::Primitive& primitive)
{
    if (const auto* text = std::get_if<mfd::TextGeometry>(&primitive.geometry); text != nullptr)
    {
        return text->text;
    }

    if (const auto* time = std::get_if<mfd::TimeGeometry>(&primitive.geometry); time != nullptr)
    {
        return time->format;
    }

    if (const auto* image = std::get_if<mfd::ImageGeometry>(&primitive.geometry); image != nullptr)
    {
        return image->file.generic_u8string();
    }

    return {};
}

struct HostApiState
{
    explicit HostApiState(editor::automation::IEditorAutomationFacade& facadeReference)
        : facade(facadeReference)
    {
    }

    void CleanupTrackedSessions() noexcept
    {
        for (const auto& sessionEntry : sessions)
        {
            facade.SessionService().RollbackSession(editor::automation::RollbackSessionRequest {sessionEntry.second});
        }
        sessions.clear();
        validationReports.clear();
        bufferedEvents.clear();
        nextBufferedEventIndex = 0U;
    }

    void InvalidateValidationReport(const MfdEditorAutomationSessionHandle handle) noexcept
    {
        validationReports.erase(handle);
    }

    [[nodiscard]] std::optional<editor::automation::AutomationEvent> ConsumeNextEvent()
    {
        if (nextBufferedEventIndex >= bufferedEvents.size())
        {
            bufferedEvents = facade.EventService().ConsumePendingEvents();
            nextBufferedEventIndex = 0U;
        }

        if (nextBufferedEventIndex >= bufferedEvents.size())
        {
            return std::nullopt;
        }

        return bufferedEvents[nextBufferedEventIndex++];
    }

    editor::automation::IEditorAutomationFacade& facade;
    std::uint64_t nextSessionHandle = 1U;
    std::unordered_map<std::uint64_t, editor::automation::AutomationSessionId> sessions {};
    std::unordered_map<std::uint64_t, editor::automation::ValidationReport> validationReports {};
    std::vector<editor::automation::AutomationEvent> bufferedEvents {};
    std::size_t nextBufferedEventIndex = 0U;
};

HostApiState* GetHostState(void* hostContext) noexcept
{
    return static_cast<HostApiState*>(hostContext);
}

std::optional<editor::automation::AutomationSessionId> FindTrackedSession(const HostApiState& state,
                                                                          const MfdEditorAutomationSessionHandle handle)
{
    const auto iterator = state.sessions.find(handle);
    if (iterator == state.sessions.end())
    {
        return std::nullopt;
    }

    return iterator->second;
}

MfdEditorAutomationResultCode ResolveClipPrimitiveId(const HostApiState& state,
                                                     const editor::automation::AutomationReticleTargetKind targetKind,
                                                     const std::string_view targetId,
                                                     const std::string_view requestedPrimitiveId,
                                                     std::string& outPrimitiveId,
                                                     MfdEditorUtf8Buffer* error)
{
    outPrimitiveId = requestedPrimitiveId;
    if (requestedPrimitiveId.empty() || !LooksLikeStablePrimitiveId(requestedPrimitiveId))
    {
        return MfdEditorAutomationResultCode_Success;
    }

    const auto snapshot = state.facade.QueryService().GetSnapshot();
    if (!snapshot.ok())
    {
        CopyUtf8ToBuffer(error, snapshot.error.message);
        return MapAutomationErrorCode(snapshot.error.code);
    }

    const auto tryResolve = [&requestedPrimitiveId, &outPrimitiveId](const auto& primitives) {
        for (const auto& primitive : primitives)
        {
            if (primitive.id.value == requestedPrimitiveId)
            {
                outPrimitiveId = primitive.primitive.id;
                return true;
            }
        }

        return false;
    };

    switch (targetKind)
    {
    case editor::automation::AutomationReticleTargetKind::ReticleAsset:
        for (const editor::automation::ReticleAssetSnapshot& reticle : snapshot.value.reticleAssets)
        {
            if (reticle.id.value != targetId)
            {
                continue;
            }

            if (tryResolve(reticle.primitives))
            {
                return MfdEditorAutomationResultCode_Success;
            }

            CopyUtf8ToBuffer(error, "Unknown clipping primitive id for the selected reticle asset.");
            return MfdEditorAutomationResultCode_NotFound;
        }
        break;
    case editor::automation::AutomationReticleTargetKind::PageReticleInstance:
        for (const editor::automation::PageSnapshot& page : snapshot.value.pages)
        {
            for (const editor::automation::PageReticleInstanceSnapshot& reticle : page.reticles)
            {
                if (reticle.id.value != targetId)
                {
                    continue;
                }

                if (tryResolve(reticle.primitives))
                {
                    return MfdEditorAutomationResultCode_Success;
                }

                CopyUtf8ToBuffer(error, "Unknown clipping primitive id for the selected page reticle.");
                return MfdEditorAutomationResultCode_NotFound;
            }
        }
        break;
    case editor::automation::AutomationReticleTargetKind::PageStrobe:
        return MfdEditorAutomationResultCode_Success;
    }

    CopyUtf8ToBuffer(error, "Unknown reticle target id.");
    return MfdEditorAutomationResultCode_NotFound;
}

MfdEditorAutomationResultCode ResolvePrimitiveSelector(const HostApiState& state,
                                                       const editor::automation::AutomationPrimitiveOwnerKind ownerKind,
                                                       const std::string_view ownerId,
                                                       const std::string_view requestedPrimitiveId,
                                                       editor::automation::PrimitiveSelector& outSelector,
                                                       MfdEditorUtf8Buffer* error)
{
    outSelector = editor::automation::PrimitiveSelector {std::string(requestedPrimitiveId), -1};
    if (requestedPrimitiveId.empty() || !LooksLikeStablePrimitiveId(requestedPrimitiveId))
    {
        return MfdEditorAutomationResultCode_Success;
    }

    const auto snapshot = state.facade.QueryService().GetSnapshot();
    if (!snapshot.ok())
    {
        CopyUtf8ToBuffer(error, snapshot.error.message);
        return MapAutomationErrorCode(snapshot.error.code);
    }

    const auto tryResolve = [&requestedPrimitiveId, &outSelector](const auto& primitives) {
        for (const auto& primitive : primitives)
        {
            if (primitive.id.value == requestedPrimitiveId)
            {
                outSelector.primitiveId = primitive.primitive.id;
                outSelector.primitiveIndex = primitive.index;
                return true;
            }
        }

        return false;
    };

    switch (ownerKind)
    {
    case editor::automation::AutomationPrimitiveOwnerKind::ReticleAsset:
        for (const editor::automation::ReticleAssetSnapshot& reticle : snapshot.value.reticleAssets)
        {
            if (reticle.id.value != ownerId)
            {
                continue;
            }

            if (tryResolve(reticle.primitives))
            {
                return MfdEditorAutomationResultCode_Success;
            }

            CopyUtf8ToBuffer(error, "Unknown primitive id for the selected reticle asset.");
            return MfdEditorAutomationResultCode_NotFound;
        }
        break;
    case editor::automation::AutomationPrimitiveOwnerKind::PageReticleInstance:
        for (const editor::automation::PageSnapshot& page : snapshot.value.pages)
        {
            for (const editor::automation::PageReticleInstanceSnapshot& reticle : page.reticles)
            {
                if (reticle.id.value != ownerId)
                {
                    continue;
                }

                if (tryResolve(reticle.primitives))
                {
                    return MfdEditorAutomationResultCode_Success;
                }

                CopyUtf8ToBuffer(error, "Unknown primitive id for the selected page reticle.");
                return MfdEditorAutomationResultCode_NotFound;
            }
        }
        break;
    }

    CopyUtf8ToBuffer(error, "Unknown primitive owner id.");
    return MfdEditorAutomationResultCode_NotFound;
}

template <typename ActionType>
MfdEditorAutomationResultCode ApplySessionAction(HostApiState& state,
                                                 const MfdEditorAutomationSessionHandle sessionHandle,
                                                 ActionType action,
                                                 MfdEditorUtf8Buffer* error)
{
    const std::optional<editor::automation::AutomationSessionId> sessionId = FindTrackedSession(state, sessionHandle);
    if (!sessionId.has_value())
    {
        CopyUtf8ToBuffer(error, "Unknown automation session handle.");
        return MfdEditorAutomationResultCode_NotFound;
    }

    const editor::automation::AutomationStatus applyStatus =
        state.facade.SessionService().ApplyAction(
            editor::automation::ApplyActionRequest {*sessionId, std::move(action)});
    if (!applyStatus.ok())
    {
        CopyUtf8ToBuffer(error, applyStatus.error.message);
        return MapAutomationErrorCode(applyStatus.error.code);
    }

    state.InvalidateValidationReport(sessionHandle);
    return MfdEditorAutomationResultCode_Success;
}

MfdEditorAutomationResultCode HostGetSnapshotSummary(void* hostContext,
                                                     MfdEditorAutomationSnapshotSummary* outSummary,
                                                     MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || outSummary == nullptr || outSummary->struct_size < sizeof(MfdEditorAutomationSnapshotSummary))
    {
        CopyUtf8ToBuffer(error, "Invalid snapshot summary output buffer.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    const auto snapshot = state->facade.QueryService().GetSnapshot();
    if (!snapshot.ok())
    {
        CopyUtf8ToBuffer(error, snapshot.error.message);
        return MapAutomationErrorCode(snapshot.error.code);
    }

    outSummary->page_count = static_cast<uint32_t>(snapshot.value.pages.size());
    outSummary->reticle_asset_count = static_cast<uint32_t>(snapshot.value.reticleAssets.size());
    outSummary->has_open_window = snapshot.value.hasOpenWindow ? 1U : 0U;
    outSummary->session_active = snapshot.value.sessionActive ? 1U : 0U;
    outSummary->active_page_index = -1;

    for (std::size_t pageIndex = 0; pageIndex < snapshot.value.pages.size(); ++pageIndex)
    {
        if (snapshot.value.pages[pageIndex].id.value == snapshot.value.uiState.activePageId.value)
        {
            outSummary->active_page_index = static_cast<int32_t>(pageIndex);
            break;
        }
    }

    MfdEditorAutomationResultCode result =
        CopyUtf8ToBuffer(&outSummary->window_id, snapshot.value.windowId.value);
    result = MergeBufferStatus(result, CopyUtf8ToBuffer(&outSummary->window_title, snapshot.value.window.title));
    return result;
}

MfdEditorAutomationResultCode HostGetWindowInfo(void* hostContext,
                                                MfdEditorAutomationWindowInfo* outWindow,
                                                MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || outWindow == nullptr || outWindow->struct_size < sizeof(MfdEditorAutomationWindowInfo))
    {
        CopyUtf8ToBuffer(error, "Invalid window info output buffer.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    const auto snapshot = state->facade.QueryService().GetSnapshot();
    if (!snapshot.ok())
    {
        CopyUtf8ToBuffer(error, snapshot.error.message);
        return MapAutomationErrorCode(snapshot.error.code);
    }

    outWindow->page_count = static_cast<uint32_t>(snapshot.value.pages.size());
    outWindow->reticle_asset_count = static_cast<uint32_t>(snapshot.value.reticleAssets.size());
    outWindow->active_page_index = -1;
    outWindow->width = snapshot.value.window.width;
    outWindow->height = snapshot.value.window.height;
    outWindow->position_x = snapshot.value.window.positionX;
    outWindow->position_y = snapshot.value.window.positionY;
    outWindow->target_fps = snapshot.value.window.targetFps;
    outWindow->has_open_window = snapshot.value.hasOpenWindow ? 1U : 0U;
    outWindow->session_active = snapshot.value.sessionActive ? 1U : 0U;

    for (std::size_t pageIndex = 0; pageIndex < snapshot.value.pages.size(); ++pageIndex)
    {
        if (snapshot.value.pages[pageIndex].id.value == snapshot.value.uiState.activePageId.value)
        {
            outWindow->active_page_index = static_cast<int32_t>(pageIndex);
            break;
        }
    }

    MfdEditorAutomationResultCode result = CopyUtf8ToBuffer(&outWindow->window_id, snapshot.value.windowId.value);
    result = MergeBufferStatus(result, CopyUtf8ToBuffer(&outWindow->window_title, snapshot.value.window.title));
    result = MergeBufferStatus(
        result,
        CopyUtf8ToBuffer(&outWindow->source_path, snapshot.value.window.sourceFile.generic_u8string()));
    result = MergeBufferStatus(
        result,
        CopyUtf8ToBuffer(&outWindow->font_file, snapshot.value.window.fontFile.generic_u8string()));
    result = MergeBufferStatus(
        result,
        CopyUtf8ToBuffer(&outWindow->icon_file, snapshot.value.window.iconFile.generic_u8string()));
    result = MergeBufferStatus(
        result,
        CopyUtf8ToBuffer(&outWindow->reticle_library_folder,
                         snapshot.value.window.reticleLibraryFolder.generic_u8string()));
    return result;
}

MfdEditorAutomationResultCode HostGetUiState(void* hostContext,
                                             MfdEditorAutomationUiState* outState,
                                             MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || outState == nullptr || outState->struct_size < sizeof(MfdEditorAutomationUiState))
    {
        CopyUtf8ToBuffer(error, "Invalid UI state output buffer.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    const auto snapshot = state->facade.QueryService().GetSnapshot();
    if (!snapshot.ok())
    {
        CopyUtf8ToBuffer(error, snapshot.error.message);
        return MapAutomationErrorCode(snapshot.error.code);
    }

    outState->selection_kind = MapSelectionKind(snapshot.value.uiState.selectionKind);
    outState->selected_page_reticle_count = static_cast<uint32_t>(snapshot.value.uiState.selectedPageReticleIds.size());
    outState->selected_primitive_index = snapshot.value.uiState.selectedPrimitiveIndex;
    outState->selected_primitive_owner_kind = MapPrimitiveOwnerKind(snapshot.value.uiState.selectedPrimitiveOwnerKind);
    outState->page_preview_view = ToApiPageView(snapshot.value.uiState.pagePreviewView);
    outState->library_preview_view = ToApiPageView(snapshot.value.uiState.libraryPreviewView);

    MfdEditorAutomationResultCode result =
        CopyUtf8ToBuffer(&outState->active_page_id, snapshot.value.uiState.activePageId.value);
    result = MergeBufferStatus(
        result,
        CopyUtf8ToBuffer(&outState->selected_reticle_asset_id, snapshot.value.uiState.selectedReticleAssetId.value));
    result = MergeBufferStatus(
        result,
        CopyUtf8ToBuffer(&outState->selected_primitive_id, snapshot.value.uiState.selectedPrimitiveId.value));
    result = MergeBufferStatus(
        result,
        CopyUtf8ToBuffer(&outState->selected_primitive_owner_id, snapshot.value.uiState.selectedPrimitiveOwnerId));
    return result;
}

MfdEditorAutomationResultCode HostGetPageInfo(void* hostContext,
                                              const uint32_t pageIndex,
                                              MfdEditorAutomationPageInfo* outPage,
                                              MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || outPage == nullptr || outPage->struct_size < sizeof(MfdEditorAutomationPageInfo))
    {
        CopyUtf8ToBuffer(error, "Invalid page info output buffer.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    const auto snapshot = state->facade.QueryService().GetSnapshot();
    if (!snapshot.ok())
    {
        CopyUtf8ToBuffer(error, snapshot.error.message);
        return MapAutomationErrorCode(snapshot.error.code);
    }

    if (pageIndex >= snapshot.value.pages.size())
    {
        CopyUtf8ToBuffer(error, "Unknown page index.");
        return MfdEditorAutomationResultCode_NotFound;
    }

    const editor::automation::PageSnapshot& page = snapshot.value.pages[pageIndex];
    outPage->index = pageIndex;
    outPage->reticle_count = static_cast<uint32_t>(page.reticles.size());
    outPage->layer_count = static_cast<uint32_t>(page.layers.size());
    outPage->blink_type_count = static_cast<uint32_t>(page.page.blinkTypes.size());
    outPage->is_default_page = page.page.defaultPage ? 1U : 0U;
    outPage->is_active_page = page.id.value == snapshot.value.uiState.activePageId.value ? 1U : 0U;
    outPage->has_strobe = page.page.strobe.has_value() ? 1U : 0U;
    outPage->background_color = ToApiColor(page.page.backgroundColor);
    outPage->view = ToApiPageView(page.page.view);

    MfdEditorAutomationResultCode result = CopyUtf8ToBuffer(&outPage->page_id, page.id.value);
    result = MergeBufferStatus(result, CopyUtf8ToBuffer(&outPage->page_name, page.page.name));
    result = MergeBufferStatus(result, CopyUtf8ToBuffer(&outPage->page_title, page.page.title));
    result = MergeBufferStatus(result, CopyUtf8ToBuffer(&outPage->source_path, page.sourceFile.generic_u8string()));
    return result;
}

MfdEditorAutomationResultCode HostGetPageBlinkTypeInfo(void* hostContext,
                                                       const uint32_t pageIndex,
                                                       const uint32_t blinkIndex,
                                                       MfdEditorAutomationPageBlinkTypeInfo* outBlinkType,
                                                       MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || outBlinkType == nullptr ||
        outBlinkType->struct_size < sizeof(MfdEditorAutomationPageBlinkTypeInfo))
    {
        CopyUtf8ToBuffer(error, "Invalid page blink-type info output buffer.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    const auto snapshot = state->facade.QueryService().GetSnapshot();
    if (!snapshot.ok())
    {
        CopyUtf8ToBuffer(error, snapshot.error.message);
        return MapAutomationErrorCode(snapshot.error.code);
    }

    if (pageIndex >= snapshot.value.pages.size())
    {
        CopyUtf8ToBuffer(error, "Unknown page index.");
        return MfdEditorAutomationResultCode_NotFound;
    }

    const editor::automation::PageSnapshot& page = snapshot.value.pages[pageIndex];
    if (blinkIndex >= page.page.blinkTypes.size())
    {
        CopyUtf8ToBuffer(error, "Unknown page blink-type index.");
        return MfdEditorAutomationResultCode_NotFound;
    }

    const mfd::PageBlinkDefinition& blinkType = page.page.blinkTypes[blinkIndex];
    outBlinkType->page_index = pageIndex;
    outBlinkType->blink_index = blinkIndex;
    outBlinkType->duration_ms = blinkType.durationMs;
    outBlinkType->is_default =
        page.page.normalizedDefaultBlinkTypeName == blinkType.normalizedName ? 1U : 0U;
    return CopyUtf8ToBuffer(&outBlinkType->blink_type_name, blinkType.name);
}

MfdEditorAutomationResultCode FillPrimitiveInfo(const editor::automation::PrimitiveSnapshot& primitive,
                                                MfdEditorAutomationPrimitiveInfo& outPrimitive)
{
    outPrimitive.owner_kind = MapPrimitiveOwnerKind(primitive.ownerKind);
    outPrimitive.primitive_index = static_cast<uint32_t>(primitive.index);
    outPrimitive.primitive_type = MapPrimitiveType(primitive.primitive.type);
    outPrimitive.visible = primitive.primitive.style.visible ? 1U : 0U;
    outPrimitive.exposed = primitive.primitive.exposed ? 1U : 0U;
    outPrimitive.filled = primitive.primitive.style.filled ? 1U : 0U;
    outPrimitive.thickness = primitive.primitive.style.thickness;
    outPrimitive.transform = ToApiTransform(primitive.primitive.transform);
    outPrimitive.color = ToApiColor(primitive.primitive.style.color);
    outPrimitive.fill_color = ToApiColor(primitive.primitive.style.fillColor);
    outPrimitive.line_style = MapLineStyle(primitive.primitive.style.lineStyle);
    std::fill(std::begin(outPrimitive.reserved_extension), std::end(outPrimitive.reserved_extension), uint8_t {0U});

    MfdEditorAutomationResultCode result =
        CopyUtf8ToBuffer(&outPrimitive.primitive_id, primitive.id.value);
    result = MergeBufferStatus(result, CopyUtf8ToBuffer(&outPrimitive.owner_id, primitive.ownerId));
    result = MergeBufferStatus(result, CopyUtf8ToBuffer(&outPrimitive.content, PrimitiveContent(primitive.primitive)));
    return result;
}

MfdEditorAutomationResultCode HostBeginSession(void* hostContext,
                                               const MfdEditorStringView label,
                                               MfdEditorAutomationSessionHandle* outSession,
                                               MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || outSession == nullptr || !IsValidView(label))
    {
        CopyUtf8ToBuffer(error, "Invalid begin-session request.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    const auto session = state->facade.SessionService().BeginSession(editor::automation::BeginSessionRequest {ViewToString(label)});
    if (!session.ok())
    {
        CopyUtf8ToBuffer(error, session.error.message);
        return MapAutomationErrorCode(session.error.code);
    }

    const std::uint64_t handle = state->nextSessionHandle++;
    state->sessions.emplace(handle, session.value);
    *outSession = handle;
    return MfdEditorAutomationResultCode_Success;
}

MfdEditorAutomationResultCode HostValidateSession(void* hostContext,
                                                  const MfdEditorAutomationSessionHandle sessionHandle,
                                                  MfdEditorAutomationValidationSummary* outValidation,
                                                  MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || outValidation == nullptr ||
        outValidation->struct_size < sizeof(MfdEditorAutomationValidationSummary))
    {
        CopyUtf8ToBuffer(error, "Invalid validate-session request.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    const std::optional<editor::automation::AutomationSessionId> sessionId = FindTrackedSession(*state, sessionHandle);
    if (!sessionId.has_value())
    {
        CopyUtf8ToBuffer(error, "Unknown automation session handle.");
        return MfdEditorAutomationResultCode_NotFound;
    }

    const auto validation =
        state->facade.SessionService().ValidateSession(editor::automation::ValidateSessionRequest {*sessionId});
    if (!validation.ok())
    {
        CopyUtf8ToBuffer(error, validation.error.message);
        return MapAutomationErrorCode(validation.error.code);
    }

    state->validationReports[sessionHandle] = validation.value;
    outValidation->valid = validation.value.valid ? 1U : 0U;
    outValidation->diagnostic_count = static_cast<uint32_t>(validation.value.diagnostics.size());
    return MfdEditorAutomationResultCode_Success;
}

MfdEditorAutomationResultCode HostCreatePageAsset(void* hostContext,
                                                  MfdEditorAutomationCreatePageAssetRequest* request,
                                                  MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || request == nullptr || request->struct_size < sizeof(MfdEditorAutomationCreatePageAssetRequest) ||
        !IsValidView(request->name) || !IsValidView(request->title) || !IsValidView(request->relative_source_path))
    {
        CopyUtf8ToBuffer(error, "Invalid create-page request.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    mfd::PageDefinition page;
    page.name = ViewToString(request->name);
    page.title = ViewToString(request->title);
    page.defaultPage = request->make_default != 0U;
    if (page.title.empty())
    {
        page.title = page.name;
    }

    const std::optional<editor::automation::AutomationSessionId> sessionId =
        FindTrackedSession(*state, request->session);
    if (!sessionId.has_value())
    {
        CopyUtf8ToBuffer(error, "Unknown automation session handle.");
        return MfdEditorAutomationResultCode_NotFound;
    }

    std::optional<std::filesystem::path> filePathHint;
    const std::string relativePath = ViewToString(request->relative_source_path);
    if (!relativePath.empty())
    {
        filePathHint = std::filesystem::u8path(relativePath);
    }

    const editor::automation::AutomationStatus applyStatus =
        state->facade.SessionService().ApplyAction(
            editor::automation::ApplyActionRequest {
                *sessionId,
                editor::automation::CreatePageAssetRequest {page, filePathHint}});
    if (!applyStatus.ok())
    {
        CopyUtf8ToBuffer(error, applyStatus.error.message);
        return MapAutomationErrorCode(applyStatus.error.code);
    }

    state->InvalidateValidationReport(request->session);

    const auto snapshot = state->facade.QueryService().GetSnapshot();
    if (!snapshot.ok())
    {
        CopyUtf8ToBuffer(error, snapshot.error.message);
        return MapAutomationErrorCode(snapshot.error.code);
    }
    if (snapshot.value.pages.empty())
    {
        CopyUtf8ToBuffer(error, "The created page could not be resolved from the live snapshot.");
        return MfdEditorAutomationResultCode_InternalFailure;
    }

    const editor::automation::PageSnapshot& createdPage = snapshot.value.pages.back();
    return CopyUtf8ToBuffer(&request->created_page_id, createdPage.id.value);
}

MfdEditorAutomationResultCode HostDeletePageAsset(void* hostContext,
                                                  MfdEditorAutomationDeletePageAssetRequest* request,
                                                  MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || request == nullptr ||
        request->struct_size < sizeof(MfdEditorAutomationDeletePageAssetRequest) ||
        !IsValidView(request->page_id))
    {
        CopyUtf8ToBuffer(error, "Invalid delete-page request.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    return ApplySessionAction(
        *state,
        request->session,
        editor::automation::DeletePageAssetRequest {editor::automation::PageId {ViewToString(request->page_id)}},
        error);
}

MfdEditorAutomationResultCode HostCreateReticleAsset(void* hostContext,
                                                     MfdEditorAutomationCreateReticleAssetRequest* request,
                                                     MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || request == nullptr ||
        request->struct_size < sizeof(MfdEditorAutomationCreateReticleAssetRequest) ||
        !IsValidView(request->template_id) ||
        !IsValidView(request->relative_source_path) ||
        !IsValidView(request->label) ||
        !IsValidView(request->category))
    {
        CopyUtf8ToBuffer(error, "Invalid create-reticle-asset request.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    const std::optional<editor::automation::AutomationSessionId> sessionId =
        FindTrackedSession(*state, request->session);
    if (!sessionId.has_value())
    {
        CopyUtf8ToBuffer(error, "Unknown automation session handle.");
        return MfdEditorAutomationResultCode_NotFound;
    }

    const std::optional<mfd::PrimitiveType> seedPrimitiveType = MapPrimitiveType(request->seed_primitive_type);
    if (!seedPrimitiveType.has_value())
    {
        CopyUtf8ToBuffer(error, "Unsupported primitive type for reticle-asset seeding.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    const std::string templateId = ViewToString(request->template_id);
    mfd::ReticleGroup reticle = editor::automation::MakePrimitiveReticle(templateId, *seedPrimitiveType);
    reticle.info.label = ViewToString(request->label);
    reticle.info.category = ViewToString(request->category);
    reticle.visible = request->visible != 0U;
    reticle.drawOnTop = request->draw_on_top != 0U;
    reticle.transform = ToModelTransform(request->transform);

    std::optional<std::filesystem::path> filePathHint;
    const std::string relativePath = ViewToString(request->relative_source_path);
    if (!relativePath.empty())
    {
        filePathHint = std::filesystem::u8path(relativePath);
    }

    const editor::automation::AutomationStatus applyStatus =
        state->facade.SessionService().ApplyAction(
            editor::automation::ApplyActionRequest {
                *sessionId,
                editor::automation::CreateReticleAssetRequest {reticle, filePathHint}});
    if (!applyStatus.ok())
    {
        CopyUtf8ToBuffer(error, applyStatus.error.message);
        return MapAutomationErrorCode(applyStatus.error.code);
    }

    state->InvalidateValidationReport(request->session);
    return CopyUtf8ToBuffer(
        &request->created_reticle_asset_id,
        editor::automation::MakeReticleAssetId(templateId).value);
}

MfdEditorAutomationResultCode HostDeleteReticleAsset(void* hostContext,
                                                     MfdEditorAutomationDeleteReticleAssetRequest* request,
                                                     MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || request == nullptr ||
        request->struct_size < sizeof(MfdEditorAutomationDeleteReticleAssetRequest) ||
        !IsValidView(request->reticle_asset_id))
    {
        CopyUtf8ToBuffer(error, "Invalid delete-reticle-asset request.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    return ApplySessionAction(
        *state,
        request->session,
        editor::automation::DeleteReticleAssetRequest {
            editor::automation::ReticleAssetId {ViewToString(request->reticle_asset_id)}},
        error);
}

MfdEditorAutomationResultCode HostCommitSession(void* hostContext,
                                                const MfdEditorAutomationSessionHandle sessionHandle,
                                                MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr)
    {
        CopyUtf8ToBuffer(error, "Invalid commit-session request.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    const std::optional<editor::automation::AutomationSessionId> sessionId = FindTrackedSession(*state, sessionHandle);
    if (!sessionId.has_value())
    {
        CopyUtf8ToBuffer(error, "Unknown automation session handle.");
        return MfdEditorAutomationResultCode_NotFound;
    }

    const editor::automation::AutomationStatus status =
        state->facade.SessionService().CommitSession(editor::automation::CommitSessionRequest {*sessionId});
    if (!status.ok())
    {
        CopyUtf8ToBuffer(error, status.error.message);
        return MapAutomationErrorCode(status.error.code);
    }

    state->sessions.erase(sessionHandle);
    state->validationReports.erase(sessionHandle);
    return MfdEditorAutomationResultCode_Success;
}

MfdEditorAutomationResultCode HostRollbackSession(void* hostContext,
                                                  const MfdEditorAutomationSessionHandle sessionHandle,
                                                  MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr)
    {
        CopyUtf8ToBuffer(error, "Invalid rollback-session request.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    const std::optional<editor::automation::AutomationSessionId> sessionId = FindTrackedSession(*state, sessionHandle);
    if (!sessionId.has_value())
    {
        CopyUtf8ToBuffer(error, "Unknown automation session handle.");
        return MfdEditorAutomationResultCode_NotFound;
    }

    const editor::automation::AutomationStatus status =
        state->facade.SessionService().RollbackSession(editor::automation::RollbackSessionRequest {*sessionId});
    if (!status.ok())
    {
        CopyUtf8ToBuffer(error, status.error.message);
        return MapAutomationErrorCode(status.error.code);
    }

    state->sessions.erase(sessionHandle);
    state->validationReports.erase(sessionHandle);
    return MfdEditorAutomationResultCode_Success;
}

MfdEditorAutomationResultCode HostSaveAll(void* hostContext,
                                          MfdEditorAutomationSaveSummary* outSave,
                                          MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || outSave == nullptr || outSave->struct_size < sizeof(MfdEditorAutomationSaveSummary))
    {
        CopyUtf8ToBuffer(error, "Invalid save-all request.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    const auto save = state->facade.PersistenceService().SaveAll();
    if (!save.ok())
    {
        CopyUtf8ToBuffer(error, save.error.message);
        return MapAutomationErrorCode(save.error.code);
    }

    outSave->saved_file_count = static_cast<uint32_t>(save.value.savedFiles.size());
    return MfdEditorAutomationResultCode_Success;
}

MfdEditorAutomationResultCode HostGetReticleAssetInfo(void* hostContext,
                                                      const uint32_t reticleIndex,
                                                      MfdEditorAutomationReticleAssetInfo* outReticle,
                                                      MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || outReticle == nullptr ||
        outReticle->struct_size < sizeof(MfdEditorAutomationReticleAssetInfo))
    {
        CopyUtf8ToBuffer(error, "Invalid reticle asset info output buffer.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    const auto snapshot = state->facade.QueryService().GetSnapshot();
    if (!snapshot.ok())
    {
        CopyUtf8ToBuffer(error, snapshot.error.message);
        return MapAutomationErrorCode(snapshot.error.code);
    }

    if (reticleIndex >= snapshot.value.reticleAssets.size())
    {
        CopyUtf8ToBuffer(error, "Unknown reticle asset index.");
        return MfdEditorAutomationResultCode_NotFound;
    }

    const editor::automation::ReticleAssetSnapshot& reticle = snapshot.value.reticleAssets[reticleIndex];
    outReticle->index = reticleIndex;
    outReticle->primitive_count = static_cast<uint32_t>(reticle.primitives.size());
    outReticle->clipping_mode = MapReticleClipMode(reticle.reticle.clipping.mode);
    outReticle->visible = reticle.reticle.visible ? 1U : 0U;
    outReticle->draw_on_top = reticle.reticle.drawOnTop ? 1U : 0U;
    outReticle->transform = ToApiTransform(reticle.reticle.transform);

    MfdEditorAutomationResultCode result = CopyUtf8ToBuffer(&outReticle->reticle_asset_id, reticle.id.value);
    result = MergeBufferStatus(result, CopyUtf8ToBuffer(&outReticle->template_id, reticle.reticle.id));
    result = MergeBufferStatus(result, CopyUtf8ToBuffer(&outReticle->label, reticle.reticle.info.label));
    result = MergeBufferStatus(result, CopyUtf8ToBuffer(&outReticle->category, reticle.reticle.info.category));
    result = MergeBufferStatus(
        result,
        CopyUtf8ToBuffer(&outReticle->clipping_primitive_id, reticle.reticle.clipping.primitiveId));
    result = MergeBufferStatus(result, CopyUtf8ToBuffer(&outReticle->source_path, reticle.sourceFile.generic_u8string()));
    return result;
}

MfdEditorAutomationResultCode HostGetPageReticleInfo(void* hostContext,
                                                     const uint32_t pageIndex,
                                                     const uint32_t reticleIndex,
                                                     MfdEditorAutomationPageReticleInfo* outReticle,
                                                     MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || outReticle == nullptr ||
        outReticle->struct_size < sizeof(MfdEditorAutomationPageReticleInfo))
    {
        CopyUtf8ToBuffer(error, "Invalid page reticle info output buffer.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    const auto snapshot = state->facade.QueryService().GetSnapshot();
    if (!snapshot.ok())
    {
        CopyUtf8ToBuffer(error, snapshot.error.message);
        return MapAutomationErrorCode(snapshot.error.code);
    }

    if (pageIndex >= snapshot.value.pages.size())
    {
        CopyUtf8ToBuffer(error, "Unknown page index.");
        return MfdEditorAutomationResultCode_NotFound;
    }

    const editor::automation::PageSnapshot& page = snapshot.value.pages[pageIndex];
    if (reticleIndex >= page.reticles.size())
    {
        CopyUtf8ToBuffer(error, "Unknown page reticle index.");
        return MfdEditorAutomationResultCode_NotFound;
    }

    const editor::automation::PageReticleInstanceSnapshot& reticle = page.reticles[reticleIndex];
    std::string layerId;
    if (!reticle.reticle.layerId.empty())
    {
        for (const editor::automation::LayerSnapshot& layer : page.layers)
        {
            if (layer.layer.id == reticle.reticle.layerId)
            {
                layerId = layer.id.value;
                break;
            }
        }
    }

    const std::string sourceReticleAssetId =
        reticle.reticle.sourceTemplateId.empty()
            ? std::string {}
            : editor::automation::MakeReticleAssetId(reticle.reticle.sourceTemplateId).value;

    outReticle->page_index = pageIndex;
    outReticle->reticle_index = reticleIndex;
    outReticle->primitive_count = static_cast<uint32_t>(reticle.primitives.size());
    outReticle->clipping_mode = MapReticleClipMode(reticle.reticle.clipping.mode);
    outReticle->visible = reticle.reticle.visible ? 1U : 0U;
    outReticle->draw_on_top = reticle.reticle.drawOnTop ? 1U : 0U;
    outReticle->blink_enabled = reticle.reticle.blink.enabled ? 1U : 0U;
    outReticle->transform = ToApiTransform(reticle.reticle.transform);

    MfdEditorAutomationResultCode result = CopyUtf8ToBuffer(&outReticle->page_reticle_id, reticle.id.value);
    result = MergeBufferStatus(result, CopyUtf8ToBuffer(&outReticle->instance_id, reticle.reticle.id));
    result = MergeBufferStatus(result, CopyUtf8ToBuffer(&outReticle->source_reticle_asset_id, sourceReticleAssetId));
    result = MergeBufferStatus(result, CopyUtf8ToBuffer(&outReticle->layer_id, layerId));
    result = MergeBufferStatus(result, CopyUtf8ToBuffer(&outReticle->blink_type_name, reticle.reticle.blink.typeName));
    result = MergeBufferStatus(
        result,
        CopyUtf8ToBuffer(&outReticle->clipping_primitive_id, reticle.reticle.clipping.primitiveId));
    return result;
}

MfdEditorAutomationResultCode HostGetLayerInfo(void* hostContext,
                                               const uint32_t pageIndex,
                                               const uint32_t layerIndex,
                                               MfdEditorAutomationLayerInfo* outLayer,
                                               MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || outLayer == nullptr || outLayer->struct_size < sizeof(MfdEditorAutomationLayerInfo))
    {
        CopyUtf8ToBuffer(error, "Invalid layer info output buffer.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    const auto snapshot = state->facade.QueryService().GetSnapshot();
    if (!snapshot.ok())
    {
        CopyUtf8ToBuffer(error, snapshot.error.message);
        return MapAutomationErrorCode(snapshot.error.code);
    }

    if (pageIndex >= snapshot.value.pages.size())
    {
        CopyUtf8ToBuffer(error, "Unknown page index.");
        return MfdEditorAutomationResultCode_NotFound;
    }

    const editor::automation::PageSnapshot& page = snapshot.value.pages[pageIndex];
    if (layerIndex >= page.layers.size())
    {
        CopyUtf8ToBuffer(error, "Unknown layer index.");
        return MfdEditorAutomationResultCode_NotFound;
    }

    const editor::automation::LayerSnapshot& layer = page.layers[layerIndex];
    outLayer->page_index = pageIndex;
    outLayer->layer_index = layerIndex;
    outLayer->assigned_reticle_count = 0U;
    outLayer->visible = layer.layer.visible ? 1U : 0U;

    for (const editor::automation::PageReticleInstanceSnapshot& reticle : page.reticles)
    {
        if (reticle.reticle.layerId == layer.layer.id)
        {
            ++outLayer->assigned_reticle_count;
        }
    }

    MfdEditorAutomationResultCode result = CopyUtf8ToBuffer(&outLayer->layer_id, layer.id.value);
    result = MergeBufferStatus(result, CopyUtf8ToBuffer(&outLayer->layer_name, layer.layer.id));
    return result;
}

MfdEditorAutomationResultCode HostGetReticleAssetPrimitiveInfo(void* hostContext,
                                                               const uint32_t reticleIndex,
                                                               const uint32_t primitiveIndex,
                                                               MfdEditorAutomationPrimitiveInfo* outPrimitive,
                                                               MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || outPrimitive == nullptr ||
        outPrimitive->struct_size < sizeof(MfdEditorAutomationPrimitiveInfo))
    {
        CopyUtf8ToBuffer(error, "Invalid reticle-asset primitive info output buffer.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    const auto snapshot = state->facade.QueryService().GetSnapshot();
    if (!snapshot.ok())
    {
        CopyUtf8ToBuffer(error, snapshot.error.message);
        return MapAutomationErrorCode(snapshot.error.code);
    }

    if (reticleIndex >= snapshot.value.reticleAssets.size())
    {
        CopyUtf8ToBuffer(error, "Unknown reticle asset index.");
        return MfdEditorAutomationResultCode_NotFound;
    }

    const editor::automation::ReticleAssetSnapshot& reticle = snapshot.value.reticleAssets[reticleIndex];
    if (primitiveIndex >= reticle.primitives.size())
    {
        CopyUtf8ToBuffer(error, "Unknown primitive index.");
        return MfdEditorAutomationResultCode_NotFound;
    }

    return FillPrimitiveInfo(reticle.primitives[primitiveIndex], *outPrimitive);
}

MfdEditorAutomationResultCode HostGetPageReticlePrimitiveInfo(void* hostContext,
                                                              const uint32_t pageIndex,
                                                              const uint32_t reticleIndex,
                                                              const uint32_t primitiveIndex,
                                                              MfdEditorAutomationPrimitiveInfo* outPrimitive,
                                                              MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || outPrimitive == nullptr ||
        outPrimitive->struct_size < sizeof(MfdEditorAutomationPrimitiveInfo))
    {
        CopyUtf8ToBuffer(error, "Invalid page-reticle primitive info output buffer.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    const auto snapshot = state->facade.QueryService().GetSnapshot();
    if (!snapshot.ok())
    {
        CopyUtf8ToBuffer(error, snapshot.error.message);
        return MapAutomationErrorCode(snapshot.error.code);
    }

    if (pageIndex >= snapshot.value.pages.size())
    {
        CopyUtf8ToBuffer(error, "Unknown page index.");
        return MfdEditorAutomationResultCode_NotFound;
    }

    const editor::automation::PageSnapshot& page = snapshot.value.pages[pageIndex];
    if (reticleIndex >= page.reticles.size())
    {
        CopyUtf8ToBuffer(error, "Unknown page reticle index.");
        return MfdEditorAutomationResultCode_NotFound;
    }

    const editor::automation::PageReticleInstanceSnapshot& reticle = page.reticles[reticleIndex];
    if (primitiveIndex >= reticle.primitives.size())
    {
        CopyUtf8ToBuffer(error, "Unknown primitive index.");
        return MfdEditorAutomationResultCode_NotFound;
    }

    return FillPrimitiveInfo(reticle.primitives[primitiveIndex], *outPrimitive);
}

MfdEditorAutomationResultCode HostGetSessionValidationDiagnostic(void* hostContext,
                                                                 const MfdEditorAutomationSessionHandle sessionHandle,
                                                                 const uint32_t diagnosticIndex,
                                                                 MfdEditorAutomationValidationDiagnostic* outDiagnostic,
                                                                 MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || outDiagnostic == nullptr ||
        outDiagnostic->struct_size < sizeof(MfdEditorAutomationValidationDiagnostic))
    {
        CopyUtf8ToBuffer(error, "Invalid validation diagnostic output buffer.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    if (!FindTrackedSession(*state, sessionHandle).has_value())
    {
        CopyUtf8ToBuffer(error, "Unknown automation session handle.");
        return MfdEditorAutomationResultCode_NotFound;
    }

    const auto iterator = state->validationReports.find(sessionHandle);
    if (iterator == state->validationReports.end())
    {
        CopyUtf8ToBuffer(error, "Validate the session before reading validation diagnostics.");
        return MfdEditorAutomationResultCode_InvalidState;
    }

    if (diagnosticIndex >= iterator->second.diagnostics.size())
    {
        CopyUtf8ToBuffer(error, "Unknown validation diagnostic index.");
        return MfdEditorAutomationResultCode_NotFound;
    }

    const editor::automation::ValidationDiagnostic& diagnostic =
        iterator->second.diagnostics[diagnosticIndex];
    outDiagnostic->index = diagnosticIndex;
    outDiagnostic->diagnostic_code = static_cast<int32_t>(MapAutomationErrorCode(diagnostic.code));

    MfdEditorAutomationResultCode result = CopyUtf8ToBuffer(&outDiagnostic->entity_id, diagnostic.entityId);
    result = MergeBufferStatus(result, CopyUtf8ToBuffer(&outDiagnostic->message, diagnostic.message));
    return result;
}

MfdEditorAutomationResultCode HostInstantiateReticleOnPage(void* hostContext,
                                                           MfdEditorAutomationInstantiateReticleOnPageRequest* request,
                                                           MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || request == nullptr ||
        request->struct_size < sizeof(MfdEditorAutomationInstantiateReticleOnPageRequest) ||
        !IsValidView(request->page_id) ||
        !IsValidView(request->reticle_asset_id) ||
        !IsValidView(request->requested_instance_id))
    {
        CopyUtf8ToBuffer(error, "Invalid instantiate-reticle request.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    const std::optional<editor::automation::AutomationSessionId> sessionId =
        FindTrackedSession(*state, request->session);
    if (!sessionId.has_value())
    {
        CopyUtf8ToBuffer(error, "Unknown automation session handle.");
        return MfdEditorAutomationResultCode_NotFound;
    }

    const std::string pageId = ViewToString(request->page_id);
    const std::string reticleAssetId = ViewToString(request->reticle_asset_id);
    const std::string requestedInstanceId = ViewToString(request->requested_instance_id);
    const editor::automation::AutomationStatus applyStatus =
        state->facade.SessionService().ApplyAction(
            editor::automation::ApplyActionRequest {
                *sessionId,
                editor::automation::InstantiateReticleOnPageRequest {
                    editor::automation::PageId {pageId},
                    editor::automation::ReticleAssetId {reticleAssetId},
                    requestedInstanceId.empty() ? std::nullopt : std::optional<std::string> {requestedInstanceId},
                    ToModelTransform(request->transform),
                    request->assign_default_layer != 0U}});
    if (!applyStatus.ok())
    {
        CopyUtf8ToBuffer(error, applyStatus.error.message);
        return MapAutomationErrorCode(applyStatus.error.code);
    }

    state->InvalidateValidationReport(request->session);

    const auto snapshot = state->facade.QueryService().GetSnapshot();
    if (!snapshot.ok())
    {
        CopyUtf8ToBuffer(error, snapshot.error.message);
        return MapAutomationErrorCode(snapshot.error.code);
    }

    for (const editor::automation::PageSnapshot& page : snapshot.value.pages)
    {
        if (page.id.value != pageId)
        {
            continue;
        }

        if (page.reticles.empty())
        {
            break;
        }

        return CopyUtf8ToBuffer(&request->created_page_reticle_id, page.reticles.back().id.value);
    }

    CopyUtf8ToBuffer(error, "The created page reticle could not be resolved from the live snapshot.");
    return MfdEditorAutomationResultCode_InternalFailure;
}

MfdEditorAutomationResultCode HostDeletePageReticle(void* hostContext,
                                                    MfdEditorAutomationDeletePageReticleRequest* request,
                                                    MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || request == nullptr ||
        request->struct_size < sizeof(MfdEditorAutomationDeletePageReticleRequest) ||
        !IsValidView(request->page_reticle_id))
    {
        CopyUtf8ToBuffer(error, "Invalid delete-page-reticle request.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    return ApplySessionAction(
        *state,
        request->session,
        editor::automation::DeletePageReticleInstanceRequest {
            editor::automation::PageReticleInstanceId {ViewToString(request->page_reticle_id)}},
        error);
}

MfdEditorAutomationResultCode HostMovePageReticle(void* hostContext,
                                                  MfdEditorAutomationMovePageReticleRequest* request,
                                                  MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || request == nullptr ||
        request->struct_size < sizeof(MfdEditorAutomationMovePageReticleRequest) ||
        !IsValidView(request->page_reticle_id))
    {
        CopyUtf8ToBuffer(error, "Invalid move-page-reticle request.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    const std::optional<editor::automation::AutomationSessionId> sessionId =
        FindTrackedSession(*state, request->session);
    if (!sessionId.has_value())
    {
        CopyUtf8ToBuffer(error, "Unknown automation session handle.");
        return MfdEditorAutomationResultCode_NotFound;
    }

    const editor::automation::AutomationStatus applyStatus =
        state->facade.SessionService().ApplyAction(
            editor::automation::ApplyActionRequest {
                *sessionId,
                editor::automation::MovePageReticleRequest {
                    editor::automation::PageReticleInstanceId {ViewToString(request->page_reticle_id)},
                    request->target_index}});
    if (!applyStatus.ok())
    {
        CopyUtf8ToBuffer(error, applyStatus.error.message);
        return MapAutomationErrorCode(applyStatus.error.code);
    }

    state->InvalidateValidationReport(request->session);
    return MfdEditorAutomationResultCode_Success;
}

MfdEditorAutomationResultCode HostSetPageReticleVisibility(void* hostContext,
                                                           MfdEditorAutomationSetPageReticleVisibilityRequest* request,
                                                           MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || request == nullptr ||
        request->struct_size < sizeof(MfdEditorAutomationSetPageReticleVisibilityRequest) ||
        !IsValidView(request->page_reticle_id))
    {
        CopyUtf8ToBuffer(error, "Invalid set-page-reticle-visibility request.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    const std::optional<editor::automation::AutomationSessionId> sessionId =
        FindTrackedSession(*state, request->session);
    if (!sessionId.has_value())
    {
        CopyUtf8ToBuffer(error, "Unknown automation session handle.");
        return MfdEditorAutomationResultCode_NotFound;
    }

    const editor::automation::AutomationStatus applyStatus =
        state->facade.SessionService().ApplyAction(
            editor::automation::ApplyActionRequest {
                *sessionId,
                editor::automation::SetReticleVisibilityRequest {
                    editor::automation::PageReticleInstanceId {ViewToString(request->page_reticle_id)},
                    request->visible != 0U}});
    if (!applyStatus.ok())
    {
        CopyUtf8ToBuffer(error, applyStatus.error.message);
        return MapAutomationErrorCode(applyStatus.error.code);
    }

    state->InvalidateValidationReport(request->session);
    return MfdEditorAutomationResultCode_Success;
}

MfdEditorAutomationResultCode HostSetPageReticleDrawOnTop(void* hostContext,
                                                          MfdEditorAutomationSetPageReticleDrawOnTopRequest* request,
                                                          MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || request == nullptr ||
        request->struct_size < sizeof(MfdEditorAutomationSetPageReticleDrawOnTopRequest) ||
        !IsValidView(request->page_reticle_id))
    {
        CopyUtf8ToBuffer(error, "Invalid set-page-reticle-draw-on-top request.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    const std::optional<editor::automation::AutomationSessionId> sessionId =
        FindTrackedSession(*state, request->session);
    if (!sessionId.has_value())
    {
        CopyUtf8ToBuffer(error, "Unknown automation session handle.");
        return MfdEditorAutomationResultCode_NotFound;
    }

    const editor::automation::AutomationStatus applyStatus =
        state->facade.SessionService().ApplyAction(
            editor::automation::ApplyActionRequest {
                *sessionId,
                editor::automation::SetReticleDrawOnTopRequest {
                    editor::automation::PageReticleInstanceId {ViewToString(request->page_reticle_id)},
                    request->draw_on_top != 0U}});
    if (!applyStatus.ok())
    {
        CopyUtf8ToBuffer(error, applyStatus.error.message);
        return MapAutomationErrorCode(applyStatus.error.code);
    }

    state->InvalidateValidationReport(request->session);
    return MfdEditorAutomationResultCode_Success;
}

MfdEditorAutomationResultCode HostSetPageReticleTransform(void* hostContext,
                                                          MfdEditorAutomationSetPageReticleTransformRequest* request,
                                                          MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || request == nullptr ||
        request->struct_size < sizeof(MfdEditorAutomationSetPageReticleTransformRequest) ||
        !IsValidView(request->page_reticle_id))
    {
        CopyUtf8ToBuffer(error, "Invalid set-page-reticle-transform request.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    const std::optional<editor::automation::AutomationSessionId> sessionId =
        FindTrackedSession(*state, request->session);
    if (!sessionId.has_value())
    {
        CopyUtf8ToBuffer(error, "Unknown automation session handle.");
        return MfdEditorAutomationResultCode_NotFound;
    }

    const editor::automation::AutomationStatus applyStatus =
        state->facade.SessionService().ApplyAction(
            editor::automation::ApplyActionRequest {
                *sessionId,
                editor::automation::SetReticleTransformRequest {
                    editor::automation::AutomationReticleTargetKind::PageReticleInstance,
                    ViewToString(request->page_reticle_id),
                    ToModelTransform(request->transform)}});
    if (!applyStatus.ok())
    {
        CopyUtf8ToBuffer(error, applyStatus.error.message);
        return MapAutomationErrorCode(applyStatus.error.code);
    }

    state->InvalidateValidationReport(request->session);
    return MfdEditorAutomationResultCode_Success;
}

MfdEditorAutomationResultCode HostSetReticleAssetVisibility(void* hostContext,
                                                            MfdEditorAutomationSetReticleAssetVisibilityRequest* request,
                                                            MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || request == nullptr ||
        request->struct_size < sizeof(MfdEditorAutomationSetReticleAssetVisibilityRequest) ||
        !IsValidView(request->reticle_asset_id))
    {
        CopyUtf8ToBuffer(error, "Invalid set-reticle-asset-visibility request.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    return ApplySessionAction(
        *state,
        request->session,
        editor::automation::SetReticleAssetVisibilityRequest {
            editor::automation::ReticleAssetId {ViewToString(request->reticle_asset_id)},
            request->visible != 0U},
        error);
}

MfdEditorAutomationResultCode HostSetReticleAssetDrawOnTop(void* hostContext,
                                                           MfdEditorAutomationSetReticleAssetDrawOnTopRequest* request,
                                                           MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || request == nullptr ||
        request->struct_size < sizeof(MfdEditorAutomationSetReticleAssetDrawOnTopRequest) ||
        !IsValidView(request->reticle_asset_id))
    {
        CopyUtf8ToBuffer(error, "Invalid set-reticle-asset-draw-on-top request.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    return ApplySessionAction(
        *state,
        request->session,
        editor::automation::SetReticleAssetDrawOnTopRequest {
            editor::automation::ReticleAssetId {ViewToString(request->reticle_asset_id)},
            request->draw_on_top != 0U},
        error);
}

MfdEditorAutomationResultCode HostSetReticleTransform(void* hostContext,
                                                      MfdEditorAutomationSetReticleTransformRequest* request,
                                                      MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || request == nullptr ||
        request->struct_size < sizeof(MfdEditorAutomationSetReticleTransformRequest) ||
        !IsValidView(request->target_id))
    {
        CopyUtf8ToBuffer(error, "Invalid set-reticle-transform request.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    const std::optional<editor::automation::AutomationReticleTargetKind> targetKind =
        MapReticleTargetKind(request->target_kind);
    if (!targetKind.has_value())
    {
        CopyUtf8ToBuffer(error, "Unsupported reticle target kind.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    return ApplySessionAction(
        *state,
        request->session,
        editor::automation::SetReticleTransformRequest {
            *targetKind,
            ViewToString(request->target_id),
            ToModelTransform(request->transform)},
        error);
}

MfdEditorAutomationResultCode HostSetReticleClipping(void* hostContext,
                                                     MfdEditorAutomationSetReticleClippingRequest* request,
                                                     MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || request == nullptr ||
        request->struct_size < sizeof(MfdEditorAutomationSetReticleClippingRequest) ||
        !IsValidView(request->target_id) ||
        !IsValidView(request->primitive_id))
    {
        CopyUtf8ToBuffer(error, "Invalid set-reticle-clipping request.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    const std::optional<editor::automation::AutomationReticleTargetKind> targetKind =
        MapReticleTargetKind(request->target_kind);
    if (!targetKind.has_value())
    {
        CopyUtf8ToBuffer(error, "Unsupported reticle target kind.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    const std::optional<mfd::ReticleClipMode> clipMode = MapReticleClipMode(request->clipping_mode);
    if (!clipMode.has_value())
    {
        CopyUtf8ToBuffer(error, "Unsupported reticle clipping mode.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    const std::string targetId = ViewToString(request->target_id);
    std::string primitiveId;
    const MfdEditorAutomationResultCode resolvePrimitiveStatus =
        ResolveClipPrimitiveId(*state, *targetKind, targetId, ViewToString(request->primitive_id), primitiveId, error);
    if (resolvePrimitiveStatus != MfdEditorAutomationResultCode_Success)
    {
        return resolvePrimitiveStatus;
    }

    return ApplySessionAction(
        *state,
        request->session,
        editor::automation::SetReticleClippingRequest {
            *targetKind,
            targetId,
            mfd::ReticleClipState {*clipMode, primitiveId}},
        error);
}

MfdEditorAutomationResultCode HostSetPrimitiveLineStyle(void* hostContext,
                                                        MfdEditorAutomationSetPrimitiveLineStyleRequest* request,
                                                        MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || request == nullptr ||
        request->struct_size < sizeof(MfdEditorAutomationSetPrimitiveLineStyleRequest) ||
        !IsValidView(request->owner_id) ||
        !IsValidView(request->primitive_id))
    {
        CopyUtf8ToBuffer(error, "Invalid set-primitive-line-style request.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    const std::optional<editor::automation::AutomationPrimitiveOwnerKind> ownerKind =
        MapPrimitiveOwnerKind(request->owner_kind);
    if (!ownerKind.has_value())
    {
        CopyUtf8ToBuffer(error, "Unsupported primitive owner kind.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    const std::optional<mfd::LineStyle> lineStyle = MapLineStyle(request->line_style);
    if (!lineStyle.has_value())
    {
        CopyUtf8ToBuffer(error, "Unsupported primitive line style.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    const std::string ownerId = ViewToString(request->owner_id);
    editor::automation::PrimitiveSelector primitiveSelector {};
    const MfdEditorAutomationResultCode resolveStatus =
        ResolvePrimitiveSelector(*state, *ownerKind, ownerId, ViewToString(request->primitive_id), primitiveSelector, error);
    if (resolveStatus != MfdEditorAutomationResultCode_Success)
    {
        return resolveStatus;
    }

    return ApplySessionAction(
        *state,
        request->session,
        editor::automation::SetPrimitiveLineStyleRequest {
            *ownerKind,
            ownerId,
            std::move(primitiveSelector),
            *lineStyle},
        error);
}

MfdEditorAutomationResultCode HostSetPageReticleLayer(void* hostContext,
                                                      MfdEditorAutomationSetPageReticleLayerRequest* request,
                                                      MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || request == nullptr ||
        request->struct_size < sizeof(MfdEditorAutomationSetPageReticleLayerRequest) ||
        !IsValidView(request->page_reticle_id) ||
        !IsValidView(request->layer_id))
    {
        CopyUtf8ToBuffer(error, "Invalid set-page-reticle-layer request.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    const std::optional<editor::automation::AutomationSessionId> sessionId =
        FindTrackedSession(*state, request->session);
    if (!sessionId.has_value())
    {
        CopyUtf8ToBuffer(error, "Unknown automation session handle.");
        return MfdEditorAutomationResultCode_NotFound;
    }

    const editor::automation::AutomationStatus applyStatus =
        state->facade.SessionService().ApplyAction(
            editor::automation::ApplyActionRequest {
                *sessionId,
                editor::automation::SetPageReticleLayerRequest {
                    editor::automation::PageReticleInstanceId {ViewToString(request->page_reticle_id)},
                    editor::automation::LayerId {ViewToString(request->layer_id)}}});
    if (!applyStatus.ok())
    {
        CopyUtf8ToBuffer(error, applyStatus.error.message);
        return MapAutomationErrorCode(applyStatus.error.code);
    }

    state->InvalidateValidationReport(request->session);
    return MfdEditorAutomationResultCode_Success;
}

MfdEditorAutomationResultCode HostCreateLayer(void* hostContext,
                                              MfdEditorAutomationCreateLayerRequest* request,
                                              MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || request == nullptr ||
        request->struct_size < sizeof(MfdEditorAutomationCreateLayerRequest) ||
        !IsValidView(request->page_id) ||
        !IsValidView(request->layer_name))
    {
        CopyUtf8ToBuffer(error, "Invalid create-layer request.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    const std::optional<editor::automation::AutomationSessionId> sessionId =
        FindTrackedSession(*state, request->session);
    if (!sessionId.has_value())
    {
        CopyUtf8ToBuffer(error, "Unknown automation session handle.");
        return MfdEditorAutomationResultCode_NotFound;
    }

    const std::string pageId = ViewToString(request->page_id);
    const std::string layerName = ViewToString(request->layer_name);
    const std::optional<int> insertIndex =
        request->insert_index < 0 ? std::nullopt : std::optional<int> {request->insert_index};
    const editor::automation::AutomationStatus applyStatus =
        state->facade.SessionService().ApplyAction(
            editor::automation::ApplyActionRequest {
                *sessionId,
                editor::automation::CreateLayerRequest {
                    editor::automation::PageId {pageId},
                    mfd::EditorLayerDefinition {layerName, request->visible != 0U},
                    insertIndex}});
    if (!applyStatus.ok())
    {
        CopyUtf8ToBuffer(error, applyStatus.error.message);
        return MapAutomationErrorCode(applyStatus.error.code);
    }

    state->InvalidateValidationReport(request->session);

    const auto snapshot = state->facade.QueryService().GetSnapshot();
    if (!snapshot.ok())
    {
        CopyUtf8ToBuffer(error, snapshot.error.message);
        return MapAutomationErrorCode(snapshot.error.code);
    }

    for (const editor::automation::PageSnapshot& page : snapshot.value.pages)
    {
        if (page.id.value != pageId)
        {
            continue;
        }

        for (const editor::automation::LayerSnapshot& layer : page.layers)
        {
            if (layer.layer.id == layerName)
            {
                return CopyUtf8ToBuffer(&request->created_layer_id, layer.id.value);
            }
        }
    }

    CopyUtf8ToBuffer(error, "The created layer could not be resolved from the live snapshot.");
    return MfdEditorAutomationResultCode_InternalFailure;
}

MfdEditorAutomationResultCode HostReplaceLayer(void* hostContext,
                                               MfdEditorAutomationReplaceLayerRequest* request,
                                               MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || request == nullptr ||
        request->struct_size < sizeof(MfdEditorAutomationReplaceLayerRequest) ||
        !IsValidView(request->page_id) ||
        !IsValidView(request->layer_id) ||
        !IsValidView(request->layer_name))
    {
        CopyUtf8ToBuffer(error, "Invalid replace-layer request.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    return ApplySessionAction(
        *state,
        request->session,
        editor::automation::ReplaceLayerRequest {
            editor::automation::PageId {ViewToString(request->page_id)},
            editor::automation::LayerId {ViewToString(request->layer_id)},
            mfd::EditorLayerDefinition {ViewToString(request->layer_name), request->visible != 0U}},
        error);
}

MfdEditorAutomationResultCode HostDeleteLayer(void* hostContext,
                                              MfdEditorAutomationDeleteLayerRequest* request,
                                              MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || request == nullptr ||
        request->struct_size < sizeof(MfdEditorAutomationDeleteLayerRequest) ||
        !IsValidView(request->page_id) ||
        !IsValidView(request->layer_id))
    {
        CopyUtf8ToBuffer(error, "Invalid delete-layer request.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    return ApplySessionAction(
        *state,
        request->session,
        editor::automation::DeleteLayerRequest {
            editor::automation::PageId {ViewToString(request->page_id)},
            editor::automation::LayerId {ViewToString(request->layer_id)}},
        error);
}

MfdEditorAutomationResultCode HostSetLayerVisibility(void* hostContext,
                                                     MfdEditorAutomationSetLayerVisibilityRequest* request,
                                                     MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || request == nullptr ||
        request->struct_size < sizeof(MfdEditorAutomationSetLayerVisibilityRequest) ||
        !IsValidView(request->page_id) ||
        !IsValidView(request->layer_id))
    {
        CopyUtf8ToBuffer(error, "Invalid set-layer-visibility request.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    const std::optional<editor::automation::AutomationSessionId> sessionId =
        FindTrackedSession(*state, request->session);
    if (!sessionId.has_value())
    {
        CopyUtf8ToBuffer(error, "Unknown automation session handle.");
        return MfdEditorAutomationResultCode_NotFound;
    }

    const editor::automation::AutomationStatus applyStatus =
        state->facade.SessionService().ApplyAction(
            editor::automation::ApplyActionRequest {
                *sessionId,
                editor::automation::SetLayerVisibilityRequest {
                    editor::automation::PageId {ViewToString(request->page_id)},
                    editor::automation::LayerId {ViewToString(request->layer_id)},
                    request->visible != 0U}});
    if (!applyStatus.ok())
    {
        CopyUtf8ToBuffer(error, applyStatus.error.message);
        return MapAutomationErrorCode(applyStatus.error.code);
    }

    state->InvalidateValidationReport(request->session);
    return MfdEditorAutomationResultCode_Success;
}

MfdEditorAutomationResultCode HostUpsertPageBlinkType(void* hostContext,
                                                      MfdEditorAutomationUpsertPageBlinkTypeRequest* request,
                                                      MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || request == nullptr ||
        request->struct_size < sizeof(MfdEditorAutomationUpsertPageBlinkTypeRequest) ||
        !IsValidView(request->page_id) ||
        !IsValidView(request->existing_blink_type_name) ||
        !IsValidView(request->blink_type_name))
    {
        CopyUtf8ToBuffer(error, "Invalid upsert-page-blink-type request.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    const std::optional<editor::automation::AutomationSessionId> sessionId =
        FindTrackedSession(*state, request->session);
    if (!sessionId.has_value())
    {
        CopyUtf8ToBuffer(error, "Unknown automation session handle.");
        return MfdEditorAutomationResultCode_NotFound;
    }

    const auto snapshot = state->facade.QueryService().GetSnapshot();
    if (!snapshot.ok())
    {
        CopyUtf8ToBuffer(error, snapshot.error.message);
        return MapAutomationErrorCode(snapshot.error.code);
    }

    const std::string pageId = ViewToString(request->page_id);
    const std::string existingName = ViewToString(request->existing_blink_type_name);
    const std::string nextName = ViewToString(request->blink_type_name);
    if (nextName.empty())
    {
        CopyUtf8ToBuffer(error, "Blink type name cannot be empty.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    const auto pageIterator = std::find_if(
        snapshot.value.pages.begin(),
        snapshot.value.pages.end(),
        [&pageId](const editor::automation::PageSnapshot& candidate)
        {
            return candidate.id.value == pageId;
        });
    if (pageIterator == snapshot.value.pages.end())
    {
        CopyUtf8ToBuffer(error, "Unknown page id.");
        return MfdEditorAutomationResultCode_NotFound;
    }

    std::vector<mfd::PageBlinkDefinition> blinkTypes = pageIterator->page.blinkTypes;
    const std::string normalizedExisting = mfd::NormalizePageName(existingName);
    const std::string normalizedNext = mfd::NormalizePageName(nextName);

    if (existingName.empty())
    {
        blinkTypes.push_back(mfd::PageBlinkDefinition {nextName, normalizedNext, request->duration_ms});
    }
    else
    {
        auto existingIterator = std::find_if(
            blinkTypes.begin(),
            blinkTypes.end(),
            [&normalizedExisting](const mfd::PageBlinkDefinition& candidate)
            {
                return candidate.normalizedName == normalizedExisting;
            });
        if (existingIterator == blinkTypes.end())
        {
            CopyUtf8ToBuffer(error, "Unknown page blink type.");
            return MfdEditorAutomationResultCode_NotFound;
        }

        existingIterator->name = nextName;
        existingIterator->normalizedName = normalizedNext;
        existingIterator->durationMs = request->duration_ms;
    }

    const editor::automation::AutomationStatus replaceStatus =
        state->facade.SessionService().ApplyAction(
            editor::automation::ApplyActionRequest {
                *sessionId,
                editor::automation::ReplacePageBlinkTypesRequest {
                    editor::automation::PageId {pageId},
                    blinkTypes}});
    if (!replaceStatus.ok())
    {
        CopyUtf8ToBuffer(error, replaceStatus.error.message);
        return MapAutomationErrorCode(replaceStatus.error.code);
    }

    std::string defaultBlinkTypeName = pageIterator->page.defaultBlinkTypeName;
    if (!existingName.empty() &&
        pageIterator->page.normalizedDefaultBlinkTypeName == normalizedExisting)
    {
        defaultBlinkTypeName = nextName;
    }
    if (request->make_default != 0U)
    {
        defaultBlinkTypeName = nextName;
    }

    const editor::automation::AutomationStatus defaultStatus =
        state->facade.SessionService().ApplyAction(
            editor::automation::ApplyActionRequest {
                *sessionId,
                editor::automation::SetPageDefaultBlinkTypeRequest {
                    editor::automation::PageId {pageId},
                    defaultBlinkTypeName}});
    if (!defaultStatus.ok())
    {
        CopyUtf8ToBuffer(error, defaultStatus.error.message);
        return MapAutomationErrorCode(defaultStatus.error.code);
    }

    state->InvalidateValidationReport(request->session);
    return MfdEditorAutomationResultCode_Success;
}

MfdEditorAutomationResultCode HostDeletePageBlinkType(void* hostContext,
                                                      MfdEditorAutomationDeletePageBlinkTypeRequest* request,
                                                      MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || request == nullptr ||
        request->struct_size < sizeof(MfdEditorAutomationDeletePageBlinkTypeRequest) ||
        !IsValidView(request->page_id) ||
        !IsValidView(request->blink_type_name))
    {
        CopyUtf8ToBuffer(error, "Invalid delete-page-blink-type request.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    const std::optional<editor::automation::AutomationSessionId> sessionId =
        FindTrackedSession(*state, request->session);
    if (!sessionId.has_value())
    {
        CopyUtf8ToBuffer(error, "Unknown automation session handle.");
        return MfdEditorAutomationResultCode_NotFound;
    }

    const auto snapshot = state->facade.QueryService().GetSnapshot();
    if (!snapshot.ok())
    {
        CopyUtf8ToBuffer(error, snapshot.error.message);
        return MapAutomationErrorCode(snapshot.error.code);
    }

    const std::string pageId = ViewToString(request->page_id);
    const std::string blinkTypeName = ViewToString(request->blink_type_name);
    const std::string normalizedBlinkTypeName = mfd::NormalizePageName(blinkTypeName);
    const auto pageIterator = std::find_if(
        snapshot.value.pages.begin(),
        snapshot.value.pages.end(),
        [&pageId](const editor::automation::PageSnapshot& candidate)
        {
            return candidate.id.value == pageId;
        });
    if (pageIterator == snapshot.value.pages.end())
    {
        CopyUtf8ToBuffer(error, "Unknown page id.");
        return MfdEditorAutomationResultCode_NotFound;
    }

    std::vector<mfd::PageBlinkDefinition> blinkTypes = pageIterator->page.blinkTypes;
    const auto removeIterator = std::remove_if(
        blinkTypes.begin(),
        blinkTypes.end(),
        [&normalizedBlinkTypeName](const mfd::PageBlinkDefinition& candidate)
        {
            return candidate.normalizedName == normalizedBlinkTypeName;
        });
    if (removeIterator == blinkTypes.end())
    {
        CopyUtf8ToBuffer(error, "Unknown page blink type.");
        return MfdEditorAutomationResultCode_NotFound;
    }
    blinkTypes.erase(removeIterator, blinkTypes.end());

    const editor::automation::AutomationStatus replaceStatus =
        state->facade.SessionService().ApplyAction(
            editor::automation::ApplyActionRequest {
                *sessionId,
                editor::automation::ReplacePageBlinkTypesRequest {
                    editor::automation::PageId {pageId},
                    blinkTypes}});
    if (!replaceStatus.ok())
    {
        CopyUtf8ToBuffer(error, replaceStatus.error.message);
        return MapAutomationErrorCode(replaceStatus.error.code);
    }

    const std::string defaultBlinkTypeName =
        pageIterator->page.normalizedDefaultBlinkTypeName == normalizedBlinkTypeName
            ? std::string {}
            : pageIterator->page.defaultBlinkTypeName;
    const editor::automation::AutomationStatus defaultStatus =
        state->facade.SessionService().ApplyAction(
            editor::automation::ApplyActionRequest {
                *sessionId,
                editor::automation::SetPageDefaultBlinkTypeRequest {
                    editor::automation::PageId {pageId},
                    defaultBlinkTypeName}});
    if (!defaultStatus.ok())
    {
        CopyUtf8ToBuffer(error, defaultStatus.error.message);
        return MapAutomationErrorCode(defaultStatus.error.code);
    }

    state->InvalidateValidationReport(request->session);
    return MfdEditorAutomationResultCode_Success;
}

MfdEditorAutomationResultCode HostSetPageDefaultBlinkType(void* hostContext,
                                                          MfdEditorAutomationSetPageDefaultBlinkTypeRequest* request,
                                                          MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || request == nullptr ||
        request->struct_size < sizeof(MfdEditorAutomationSetPageDefaultBlinkTypeRequest) ||
        !IsValidView(request->page_id) ||
        !IsValidView(request->blink_type_name))
    {
        CopyUtf8ToBuffer(error, "Invalid set-page-default-blink-type request.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    return ApplySessionAction(
        *state,
        request->session,
        editor::automation::SetPageDefaultBlinkTypeRequest {
            editor::automation::PageId {ViewToString(request->page_id)},
            ViewToString(request->blink_type_name)},
        error);
}

MfdEditorAutomationResultCode HostSetPageReticleBlink(void* hostContext,
                                                      MfdEditorAutomationSetPageReticleBlinkRequest* request,
                                                      MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || request == nullptr ||
        request->struct_size < sizeof(MfdEditorAutomationSetPageReticleBlinkRequest) ||
        !IsValidView(request->page_reticle_id) ||
        !IsValidView(request->blink_type_name))
    {
        CopyUtf8ToBuffer(error, "Invalid set-page-reticle-blink request.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    mfd::ReticleBlinkState blink;
    blink.enabled = request->enabled != 0U;
    blink.typeName = ViewToString(request->blink_type_name);
    return ApplySessionAction(
        *state,
        request->session,
        editor::automation::SetPageReticleBlinkRequest {
            editor::automation::PageReticleInstanceId {ViewToString(request->page_reticle_id)},
            blink},
        error);
}

MfdEditorAutomationResultCode HostAssignPageStrobeTemplate(void* hostContext,
                                                           MfdEditorAutomationAssignPageStrobeTemplateRequest* request,
                                                           MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || request == nullptr ||
        request->struct_size < sizeof(MfdEditorAutomationAssignPageStrobeTemplateRequest) ||
        !IsValidView(request->page_id) ||
        !IsValidView(request->reticle_asset_id) ||
        !IsValidView(request->requested_strobe_id))
    {
        CopyUtf8ToBuffer(error, "Invalid assign-page-strobe-template request.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    const std::string requestedStrobeId = ViewToString(request->requested_strobe_id);
    return ApplySessionAction(
        *state,
        request->session,
        editor::automation::AssignPageStrobeTemplateRequest {
            editor::automation::PageId {ViewToString(request->page_id)},
            editor::automation::ReticleAssetId {ViewToString(request->reticle_asset_id)},
            requestedStrobeId.empty() ? std::nullopt : std::optional<std::string> {requestedStrobeId}},
        error);
}

MfdEditorAutomationResultCode HostDeletePageStrobe(void* hostContext,
                                                   MfdEditorAutomationDeletePageStrobeRequest* request,
                                                   MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || request == nullptr ||
        request->struct_size < sizeof(MfdEditorAutomationDeletePageStrobeRequest) ||
        !IsValidView(request->page_id))
    {
        CopyUtf8ToBuffer(error, "Invalid delete-page-strobe request.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    return ApplySessionAction(
        *state,
        request->session,
        editor::automation::DeletePageStrobeRequest {
            editor::automation::PageId {ViewToString(request->page_id)}},
        error);
}

MfdEditorAutomationResultCode HostSelectEntity(void* hostContext,
                                               MfdEditorAutomationSelectEntityRequest* request,
                                               MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || request == nullptr ||
        request->struct_size < sizeof(MfdEditorAutomationSelectEntityRequest) ||
        !IsValidView(request->target_id))
    {
        CopyUtf8ToBuffer(error, "Invalid select-entity request.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    const std::optional<editor::automation::AutomationSelectionKind> selectionKind =
        MapSelectionKind(request->selection_kind);
    if (!selectionKind.has_value())
    {
        CopyUtf8ToBuffer(error, "Unsupported selection kind.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    return ApplySessionAction(
        *state,
        request->session,
        editor::automation::SelectEntityRequest {*selectionKind, ViewToString(request->target_id)},
        error);
}

MfdEditorAutomationResultCode HostExportJsonPreview(void* hostContext,
                                                    const MfdEditorAutomationExportJsonPreviewRequest* request,
                                                    MfdEditorAutomationJsonPreviewResult* outPreview,
                                                    MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || request == nullptr || outPreview == nullptr ||
        request->struct_size < sizeof(MfdEditorAutomationExportJsonPreviewRequest) ||
        outPreview->struct_size < sizeof(MfdEditorAutomationJsonPreviewResult) ||
        !IsValidView(request->entity_id))
    {
        CopyUtf8ToBuffer(error, "Invalid export-json-preview request.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    const std::optional<editor::automation::ExportJsonPreviewRequest::Kind> kind =
        MapExportJsonPreviewKind(request->kind);
    if (!kind.has_value())
    {
        CopyUtf8ToBuffer(error, "Unsupported JSON preview kind.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    const auto preview = state->facade.PersistenceService().ExportJsonPreview(
        editor::automation::ExportJsonPreviewRequest {*kind, ViewToString(request->entity_id)});
    if (!preview.ok())
    {
        CopyUtf8ToBuffer(error, preview.error.message);
        return MapAutomationErrorCode(preview.error.code);
    }

    MfdEditorAutomationResultCode result =
        CopyUtf8ToBuffer(&outPreview->source_path, preview.value.sourcePath.generic_u8string());
    result = MergeBufferStatus(result, CopyUtf8ToBuffer(&outPreview->json, preview.value.json));
    return result;
}

MfdEditorAutomationResultCode HostSaveAsset(void* hostContext,
                                            const MfdEditorAutomationSaveAssetRequest* request,
                                            MfdEditorAutomationSaveSummary* outSave,
                                            MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || request == nullptr || outSave == nullptr ||
        request->struct_size < sizeof(MfdEditorAutomationSaveAssetRequest) ||
        outSave->struct_size < sizeof(MfdEditorAutomationSaveSummary) ||
        !IsValidView(request->entity_id))
    {
        CopyUtf8ToBuffer(error, "Invalid save-asset request.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    const std::optional<editor::automation::SaveAssetRequest::Kind> kind = MapSaveAssetKind(request->kind);
    if (!kind.has_value())
    {
        CopyUtf8ToBuffer(error, "Unsupported save-asset kind.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    const auto save = state->facade.PersistenceService().SaveAsset(
        editor::automation::SaveAssetRequest {*kind, ViewToString(request->entity_id)});
    if (!save.ok())
    {
        CopyUtf8ToBuffer(error, save.error.message);
        return MapAutomationErrorCode(save.error.code);
    }

    outSave->saved_file_count = static_cast<uint32_t>(save.value.savedFiles.size());
    return MfdEditorAutomationResultCode_Success;
}

MfdEditorAutomationResultCode HostConsumePendingEvent(void* hostContext,
                                                      MfdEditorAutomationEvent* outEvent,
                                                      MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || outEvent == nullptr || outEvent->struct_size < sizeof(MfdEditorAutomationEvent))
    {
        CopyUtf8ToBuffer(error, "Invalid event output buffer.");
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    const std::optional<editor::automation::AutomationEvent> event = state->ConsumeNextEvent();
    if (!event.has_value())
    {
        CopyUtf8ToBuffer(error, "No pending automation event.");
        return MfdEditorAutomationResultCode_NotFound;
    }

    outEvent->kind = MapEventKind(event->kind);

    MfdEditorAutomationResultCode result = CopyUtf8ToBuffer(&outEvent->entity_id, event->entityId);
    result = MergeBufferStatus(result, CopyUtf8ToBuffer(&outEvent->message, event->message));
    return result;
}
} // namespace

namespace editor
{
class EditorAutomationPluginLoader::Impl
{
public:
    ~Impl() noexcept
    {
        Unload();
    }

    [[nodiscard]] bool Load(const std::filesystem::path& pluginFile,
                            [[maybe_unused]] editor::automation::IEditorAutomationFacade& facade,
                            std::string& error)
    {
        Unload();

        if (pluginFile.empty())
        {
            error = "The automation plugin DLL path is empty.";
            return false;
        }

#if defined(_WIN32)
        handle_ = ::LoadLibraryW(pluginFile.c_str());
        if (handle_ == nullptr)
        {
            error = "Unable to load automation plugin '" + pluginFile.string() +
                    "': " + FormatWindowsErrorMessage(::GetLastError());
            return false;
        }

        const FARPROC apiSymbol = ::GetProcAddress(handle_, MFD_EDITOR_AUTOMATION_PLUGIN_ENTRY_POINT);
        if (apiSymbol == nullptr)
        {
            error = "The automation plugin '" + pluginFile.string() +
                    "' does not export the required stable entry point.";
            Unload();
            return false;
        }

        const auto getApi = reinterpret_cast<MfdGetEditorAutomationPluginApiFn>(apiSymbol);
        if (getApi == nullptr)
        {
            error = "The automation plugin '" + pluginFile.string() + "' exposes an invalid stable entry point.";
            Unload();
            return false;
        }

        hostState_ = std::make_unique<HostApiState>(facade);
        hostApi_ = {};
        hostApi_.struct_size = sizeof(hostApi_);
        hostApi_.abi_version = MFD_EDITOR_AUTOMATION_PLUGIN_ABI_VERSION;
        hostApi_.host_context = hostState_.get();
        hostApi_.get_snapshot_summary = &HostGetSnapshotSummary;
        hostApi_.get_window_info = &HostGetWindowInfo;
        hostApi_.get_ui_state = &HostGetUiState;
        hostApi_.get_page_info = &HostGetPageInfo;
        hostApi_.get_page_blink_type_info = &HostGetPageBlinkTypeInfo;
        hostApi_.begin_session = &HostBeginSession;
        hostApi_.validate_session = &HostValidateSession;
        hostApi_.create_page_asset = &HostCreatePageAsset;
        hostApi_.delete_page_asset = &HostDeletePageAsset;
        hostApi_.commit_session = &HostCommitSession;
        hostApi_.rollback_session = &HostRollbackSession;
        hostApi_.save_all = &HostSaveAll;
        hostApi_.get_reticle_asset_info = &HostGetReticleAssetInfo;
        hostApi_.get_reticle_asset_primitive_info = &HostGetReticleAssetPrimitiveInfo;
        hostApi_.get_page_reticle_info = &HostGetPageReticleInfo;
        hostApi_.get_page_reticle_primitive_info = &HostGetPageReticlePrimitiveInfo;
        hostApi_.get_layer_info = &HostGetLayerInfo;
        hostApi_.get_session_validation_diagnostic = &HostGetSessionValidationDiagnostic;
        hostApi_.create_reticle_asset = &HostCreateReticleAsset;
        hostApi_.delete_reticle_asset = &HostDeleteReticleAsset;
        hostApi_.instantiate_reticle_on_page = &HostInstantiateReticleOnPage;
        hostApi_.delete_page_reticle = &HostDeletePageReticle;
        hostApi_.move_page_reticle = &HostMovePageReticle;
        hostApi_.set_page_reticle_visibility = &HostSetPageReticleVisibility;
        hostApi_.set_page_reticle_draw_on_top = &HostSetPageReticleDrawOnTop;
        hostApi_.set_page_reticle_transform = &HostSetPageReticleTransform;
        hostApi_.set_page_reticle_layer = &HostSetPageReticleLayer;
        hostApi_.set_reticle_asset_visibility = &HostSetReticleAssetVisibility;
        hostApi_.set_reticle_asset_draw_on_top = &HostSetReticleAssetDrawOnTop;
        hostApi_.set_reticle_transform = &HostSetReticleTransform;
        hostApi_.set_reticle_clipping = &HostSetReticleClipping;
        hostApi_.create_layer = &HostCreateLayer;
        hostApi_.replace_layer = &HostReplaceLayer;
        hostApi_.delete_layer = &HostDeleteLayer;
        hostApi_.set_layer_visibility = &HostSetLayerVisibility;
        hostApi_.upsert_page_blink_type = &HostUpsertPageBlinkType;
        hostApi_.delete_page_blink_type = &HostDeletePageBlinkType;
        hostApi_.set_page_default_blink_type = &HostSetPageDefaultBlinkType;
        hostApi_.set_page_reticle_blink = &HostSetPageReticleBlink;
        hostApi_.assign_page_strobe_template = &HostAssignPageStrobeTemplate;
        hostApi_.delete_page_strobe = &HostDeletePageStrobe;
        hostApi_.select_entity = &HostSelectEntity;
        hostApi_.save_asset = &HostSaveAsset;
        hostApi_.export_json_preview = &HostExportJsonPreview;
        hostApi_.consume_pending_event = &HostConsumePendingEvent;
        hostApi_.set_primitive_line_style = &HostSetPrimitiveLineStyle;

        std::array<char, kPluginErrorBufferCapacity> errorStorage {};
        MfdEditorUtf8Buffer errorBuffer {};
        ResetBuffer(errorBuffer, errorStorage.data(), errorStorage.size());

        pluginApi_ = {};
        pluginApi_.struct_size = sizeof(pluginApi_);
        const MfdEditorAutomationResultCode getApiStatus = getApi(&pluginApi_, &errorBuffer);
        if (getApiStatus != MfdEditorAutomationResultCode_Success)
        {
            error = BufferToString(errorBuffer);
            if (error.empty())
            {
                error = "The automation plugin factory failed: " + ResultCodeDescription(getApiStatus) + ".";
            }
            Unload();
            return false;
        }

        if (pluginApi_.struct_size < sizeof(MfdEditorAutomationPluginApi) ||
            pluginApi_.info.struct_size < sizeof(MfdEditorAutomationPluginInfo))
        {
            error = "The automation plugin '" + pluginFile.string() + "' returned an incomplete ABI descriptor.";
            Unload();
            return false;
        }
        if (pluginApi_.info.abi_version != MFD_EDITOR_AUTOMATION_PLUGIN_ABI_VERSION)
        {
            error = "The automation plugin '" + pluginFile.string() + "' targets ABI version " +
                    std::to_string(pluginApi_.info.abi_version) + " instead of " +
                    std::to_string(MFD_EDITOR_AUTOMATION_PLUGIN_ABI_VERSION) + ".";
            Unload();
            return false;
        }
        if (pluginApi_.start == nullptr ||
            pluginApi_.tick == nullptr ||
            pluginApi_.stop == nullptr ||
            pluginApi_.destroy == nullptr)
        {
            error = "The automation plugin '" + pluginFile.string() + "' returned one incomplete callback table.";
            Unload();
            return false;
        }

        ResetBuffer(errorBuffer, errorStorage.data(), errorStorage.size());
        try
        {
            const MfdEditorAutomationResultCode startStatus =
                pluginApi_.start(pluginApi_.plugin_context, &hostApi_, &errorBuffer);
            if (startStatus != MfdEditorAutomationResultCode_Success)
            {
                error = BufferToString(errorBuffer);
                if (error.empty())
                {
                    error = "The automation plugin refused to start: " + ResultCodeDescription(startStatus) + ".";
                }
                Unload();
                return false;
            }
        }
        catch (const std::exception& exception)
        {
            error = "The automation plugin start routine threw: " + std::string(exception.what());
            Unload();
            return false;
        }
        catch (...)
        {
            error = "The automation plugin start routine threw an unknown exception.";
            Unload();
            return false;
        }

        pluginStarted_ = true;
        pluginFile_ = pluginFile.lexically_normal();
        lastRuntimeError_.clear();
        return true;
#else
        error = "Automation plugins are only supported on Windows.";
        return false;
#endif
    }

    void Tick() noexcept
    {
        if (!pluginStarted_ || pluginApi_.tick == nullptr)
        {
            return;
        }

        std::array<char, kPluginErrorBufferCapacity> errorStorage {};
        MfdEditorUtf8Buffer errorBuffer {};
        ResetBuffer(errorBuffer, errorStorage.data(), errorStorage.size());

        try
        {
            const MfdEditorAutomationResultCode tickStatus =
                pluginApi_.tick(pluginApi_.plugin_context, &errorBuffer);
            if (tickStatus == MfdEditorAutomationResultCode_Success)
            {
                lastRuntimeError_.clear();
                return;
            }

            lastRuntimeError_ = BufferToString(errorBuffer);
            if (lastRuntimeError_.empty())
            {
                lastRuntimeError_ = ResultCodeDescription(tickStatus);
            }
        }
        catch (const std::exception& exception)
        {
            lastRuntimeError_ = exception.what();
        }
        catch (...)
        {
            lastRuntimeError_ = "Unknown automation plugin exception.";
        }
    }

    void Unload() noexcept
    {
        if (pluginApi_.stop != nullptr)
        {
            try
            {
                pluginApi_.stop(pluginApi_.plugin_context);
            }
            catch (...)
            {
            }
        }

        if (hostState_ != nullptr)
        {
            hostState_->CleanupTrackedSessions();
        }

        if (pluginApi_.destroy != nullptr)
        {
            try
            {
                pluginApi_.destroy(pluginApi_.plugin_context);
            }
            catch (...)
            {
            }
        }

        pluginApi_ = {};
        hostApi_ = {};
        hostState_.reset();
        pluginStarted_ = false;
        pluginFile_.clear();
        lastRuntimeError_.clear();

#if defined(_WIN32)
        if (handle_ != nullptr)
        {
            ::FreeLibrary(handle_);
            handle_ = nullptr;
        }
#endif
    }

    [[nodiscard]] bool IsLoaded() const noexcept
    {
        return pluginStarted_;
    }

    [[nodiscard]] std::string PluginDisplayName() const
    {
        if (!pluginStarted_)
        {
            return {};
        }

        return ViewToString(pluginApi_.info.display_name);
    }

    [[nodiscard]] std::filesystem::path PluginFile() const
    {
        return pluginFile_;
    }

    [[nodiscard]] const std::string& LastRuntimeError() const noexcept
    {
        return lastRuntimeError_;
    }

private:
#if defined(_WIN32)
    HMODULE handle_ = nullptr;
#endif
    std::unique_ptr<HostApiState> hostState_ {};
    MfdEditorAutomationHostApi hostApi_ {};
    MfdEditorAutomationPluginApi pluginApi_ {};
    bool pluginStarted_ = false;
    std::filesystem::path pluginFile_ {};
    std::string lastRuntimeError_ {};
};

EditorAutomationPluginLoader::EditorAutomationPluginLoader() = default;

EditorAutomationPluginLoader::~EditorAutomationPluginLoader() noexcept = default;

bool EditorAutomationPluginLoader::Load(const std::filesystem::path& pluginFile,
                                        editor::automation::IEditorAutomationFacade& facade,
                                        std::string& error)
{
    if (impl_ == nullptr)
    {
        impl_ = std::make_unique<Impl>();
    }

    return impl_->Load(pluginFile, facade, error);
}

void EditorAutomationPluginLoader::Tick() noexcept
{
    if (impl_ != nullptr)
    {
        impl_->Tick();
    }
}

void EditorAutomationPluginLoader::Unload() noexcept
{
    if (impl_ != nullptr)
    {
        impl_->Unload();
    }
}

bool EditorAutomationPluginLoader::IsLoaded() const noexcept
{
    return impl_ != nullptr && impl_->IsLoaded();
}

std::string EditorAutomationPluginLoader::PluginDisplayName() const
{
    return impl_ == nullptr ? std::string {} : impl_->PluginDisplayName();
}

std::filesystem::path EditorAutomationPluginLoader::PluginFile() const
{
    return impl_ == nullptr ? std::filesystem::path {} : impl_->PluginFile();
}

const std::string& EditorAutomationPluginLoader::LastRuntimeError() const noexcept
{
    static const std::string kEmpty {};
    return impl_ == nullptr ? kEmpty : impl_->LastRuntimeError();
}
} // namespace editor

