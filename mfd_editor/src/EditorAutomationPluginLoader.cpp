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

#if defined(_WIN32)
#    include <Windows.h>
#endif

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
    }

    editor::automation::IEditorAutomationFacade& facade;
    std::uint64_t nextSessionHandle = 1U;
    std::unordered_map<std::uint64_t, editor::automation::AutomationSessionId> sessions {};
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

MfdEditorAutomationResultCode HostGetSnapshotSummary(void* hostContext,
                                                     MfdEditorAutomationSnapshotSummaryV1* outSummary,
                                                     MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || outSummary == nullptr || outSummary->struct_size < sizeof(MfdEditorAutomationSnapshotSummaryV1))
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

MfdEditorAutomationResultCode HostGetPageInfo(void* hostContext,
                                              const uint32_t pageIndex,
                                              MfdEditorAutomationPageInfoV1* outPage,
                                              MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || outPage == nullptr || outPage->struct_size < sizeof(MfdEditorAutomationPageInfoV1))
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
    outPage->is_default_page = page.page.defaultPage ? 1U : 0U;

    MfdEditorAutomationResultCode result = CopyUtf8ToBuffer(&outPage->page_id, page.id.value);
    result = MergeBufferStatus(result, CopyUtf8ToBuffer(&outPage->page_name, page.page.name));
    result = MergeBufferStatus(result, CopyUtf8ToBuffer(&outPage->page_title, page.page.title));
    result = MergeBufferStatus(result, CopyUtf8ToBuffer(&outPage->source_path, page.sourceFile.generic_u8string()));
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
                                                  MfdEditorAutomationValidationSummaryV1* outValidation,
                                                  MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || outValidation == nullptr ||
        outValidation->struct_size < sizeof(MfdEditorAutomationValidationSummaryV1))
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

    outValidation->valid = validation.value.valid ? 1U : 0U;
    outValidation->diagnostic_count = static_cast<uint32_t>(validation.value.diagnostics.size());
    return MfdEditorAutomationResultCode_Success;
}

MfdEditorAutomationResultCode HostCreatePageAsset(void* hostContext,
                                                  MfdEditorAutomationCreatePageAssetRequestV1* request,
                                                  MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || request == nullptr || request->struct_size < sizeof(MfdEditorAutomationCreatePageAssetRequestV1) ||
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
    return MfdEditorAutomationResultCode_Success;
}

MfdEditorAutomationResultCode HostSaveAll(void* hostContext,
                                          MfdEditorAutomationSaveSummaryV1* outSave,
                                          MfdEditorUtf8Buffer* error)
{
    HostApiState* state = GetHostState(hostContext);
    if (state == nullptr || outSave == nullptr || outSave->struct_size < sizeof(MfdEditorAutomationSaveSummaryV1))
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
        hostApi_.get_page_info = &HostGetPageInfo;
        hostApi_.begin_session = &HostBeginSession;
        hostApi_.validate_session = &HostValidateSession;
        hostApi_.create_page_asset = &HostCreatePageAsset;
        hostApi_.commit_session = &HostCommitSession;
        hostApi_.rollback_session = &HostRollbackSession;
        hostApi_.save_all = &HostSaveAll;

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

        if (pluginApi_.struct_size < sizeof(MfdEditorAutomationPluginApiV1) ||
            pluginApi_.info.struct_size < sizeof(MfdEditorAutomationPluginInfoV1))
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
    MfdEditorAutomationHostApiV1 hostApi_ {};
    MfdEditorAutomationPluginApiV1 pluginApi_ {};
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
